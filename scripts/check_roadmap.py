"""Validate docs/roadmap/roadmap.js. Run after every roadmap edit.

    python scripts/check_roadmap.py
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROADMAP = Path(__file__).resolve().parent.parent / "docs" / "roadmap" / "roadmap.js"
MILESTONE_STATUSES = {"done", "active", "planned"}
TASK_STATUSES = {"todo", "next", "doing", "blocked", "done", "dropped"}
RISK_STATUSES = {"open", "mitigated", "retired"}
RELEASE_STATUSES = {"released", "next", "planned"}
DATE = re.compile(r"\d{4}-\d{2}-\d{2}$")
VERSION = re.compile(r"(\d+)\.(\d+)\.(\d+)$")
ASSIGNMENT = re.compile(r"window\.TYPER_ROADMAP\s*=\s*(\{.*\})\s*;\s*$", re.S)
SENTENCE_BREAK = re.compile(r"(?<=[.!?])[\"”’)]*\s+(?=[\"“(]?[A-Z0-9])")

# Writing rules per release tier: patch = one sentence, minor = one paragraph, major = an essay.
PATCH_MAX_WORDS = 50
MINOR_WORDS = (50, 220)
MAJOR_MIN_PARAGRAPHS = 4
MAJOR_MIN_WORDS = 450


def load(path: Path = ROADMAP) -> dict:
    match = ASSIGNMENT.search(path.read_text(encoding="utf-8"))
    if not match:
        raise ValueError("expected `window.TYPER_ROADMAP = { ... };`")
    return json.loads(match.group(1))


def sentences(text: str) -> list[str]:
    return [s for s in SENTENCE_BREAK.split(text.strip()) if s]


def words(text: str) -> int:
    return len(text.split())


def tier(version: tuple[int, int, int]) -> str:
    return "patch" if version[2] else "minor" if version[1] else "major"


def check_releases(data: dict, milestone_ids: set[str], errors: list[str]) -> None:
    releases = data.get("releases", [])
    if not isinstance(releases, list) or not releases:
        errors.append("top level: 'releases' must be a non-empty list")
        return
    previous: tuple[int, int, int] | None = None
    seen_unreleased = False
    nexts = 0
    for r in releases:
        v = str(r.get("version", "?"))
        where = f"release {v}"
        match = VERSION.match(v)
        if not match or v == "0.0.0":
            errors.append(f"{where}: version must be MAJOR.MINOR.PATCH")
            continue
        parsed = tuple(int(g) for g in match.groups())
        if previous is not None and parsed <= previous:
            errors.append(f"{where}: versions must be unique and in ascending order")
        previous = parsed
        status = r.get("status")
        if status not in RELEASE_STATUSES:
            errors.append(f"{where}: status must be one of {sorted(RELEASE_STATUSES)}")
        if status == "released":
            if seen_unreleased:
                errors.append(f"{where}: released versions must come before unreleased ones")
            if not DATE.match(str(r.get("date", ""))):
                errors.append(f"{where}: a released version needs 'date' (YYYY-MM-DD)")
        else:
            seen_unreleased = True
        nexts += status == "next"
        if not r.get("title"):
            errors.append(f"{where}: needs a 'title'")
        if r.get("milestone") is not None and r["milestone"] not in milestone_ids:
            errors.append(f"{where}: unknown milestone {r['milestone']}")

        kind = tier(parsed)
        text = r.get("text", "")
        if kind == "patch":
            if not isinstance(text, str) or "\n" in text or len(sentences(text)) != 1 or not text.rstrip().endswith((".", "!", "?")):
                errors.append(f"{where}: a patch release is described in exactly one sentence ('text')")
            elif words(text) > PATCH_MAX_WORDS:
                errors.append(f"{where}: keep the sentence under {PATCH_MAX_WORDS} words")
        elif kind == "minor":
            lo, hi = MINOR_WORDS
            if not isinstance(text, str) or "\n" in text or not lo <= words(text) <= hi or len(sentences(text)) < 2:
                errors.append(f"{where}: a minor release is described in one paragraph of {lo}-{hi} words ('text')")
        else:
            essay = r.get("essay")
            if (not isinstance(essay, list) or len(essay) < MAJOR_MIN_PARAGRAPHS
                    or not all(isinstance(p, str) and p.strip() for p in essay)
                    or sum(words(p) for p in essay) < MAJOR_MIN_WORDS):
                errors.append(f"{where}: a major release needs an 'essay' of at least "
                              f"{MAJOR_MIN_PARAGRAPHS} paragraphs and {MAJOR_MIN_WORDS} words")
    if nexts > 1:
        errors.append("releases: at most one release can be 'next'")


def check(data: dict) -> list[str]:
    errors: list[str] = []

    def need(obj: dict, key: str, where: str, kind: type = str) -> bool:
        if not isinstance(obj.get(key), kind) or obj.get(key) in ("", []):
            errors.append(f"{where}: '{key}' must be a non-empty {kind.__name__}")
            return False
        return True

    for key in ("project", "updated", "now", "prd", "repo"):
        need(data, key, "top level")
    if not DATE.match(str(data.get("updated", ""))):
        errors.append("top level: 'updated' must be YYYY-MM-DD")

    stories = data.get("stories", {})
    if not isinstance(stories, dict) or not stories:
        errors.append("top level: 'stories' must be a non-empty object")
        stories = {}

    seen: set[str] = set()
    milestone_ids: set[str] = set()
    active = 0
    for m in data.get("milestones", []):
        mid = m.get("id", "?")
        where = f"milestone {mid}"
        for key in ("id", "title", "goal", "done_when"):
            need(m, key, where)
        if m.get("status") not in MILESTONE_STATUSES:
            errors.append(f"{where}: status must be one of {sorted(MILESTONE_STATUSES)}")
        active += m.get("status") == "active"
        if mid in seen:
            errors.append(f"{where}: duplicate id")
        seen.add(mid)
        milestone_ids.add(mid)
        if not need(m, "tasks", where, list):
            continue
        for t in m["tasks"]:
            tid = t.get("id", "?")
            twhere = f"task {tid}"
            need(t, "id", twhere)
            need(t, "title", twhere)
            need(t, "area", twhere)
            if not str(tid).startswith(f"{mid}."):
                errors.append(f"{twhere}: id must start with '{mid}.'")
            if tid in seen:
                errors.append(f"{twhere}: duplicate id")
            seen.add(tid)
            if t.get("status") not in TASK_STATUSES:
                errors.append(f"{twhere}: status must be one of {sorted(TASK_STATUSES)}")
            if m.get("status") == "done" and t.get("status") not in ("done", "dropped"):
                errors.append(f"{twhere}: milestone {mid} is done but this task is '{t.get('status')}'")
            refs = t.get("stories", [])
            if not isinstance(refs, list):
                errors.append(f"{twhere}: 'stories' must be a list of story numbers")
                refs = []
            for n in refs:
                if str(n) not in stories:
                    errors.append(f"{twhere}: unknown story {n}")
            if "refs" in t and not all(isinstance(r, str) for r in t["refs"]):
                errors.append(f"{twhere}: 'refs' must be a list of strings")

    if not data.get("milestones"):
        errors.append("top level: 'milestones' must be a non-empty list")
    if active != 1:
        errors.append(f"exactly one milestone must be 'active' (found {active})")

    check_releases(data, milestone_ids, errors)

    for d in data.get("decisions", []):
        if not DATE.match(str(d.get("date", ""))) or not d.get("text"):
            errors.append(f"decision {d!r}: needs 'date' (YYYY-MM-DD) and 'text'")
    for r in data.get("risks", []):
        if r.get("status") not in RISK_STATUSES or not r.get("text"):
            errors.append(f"risk {r.get('text', '?')!r}: needs 'text' and status in {sorted(RISK_STATUSES)}")

    log = data.get("log", [])
    if not log:
        errors.append("top level: 'log' must have at least one entry")
    for entry in log:
        if not DATE.match(str(entry.get("date", ""))) or not entry.get("text"):
            errors.append(f"log entry {entry!r}: needs 'date' (YYYY-MM-DD) and 'text'")
    dates = [e.get("date", "") for e in log]
    if dates != sorted(dates, reverse=True):
        errors.append("log: entries must be newest first")
    if log and data.get("updated") != log[0].get("date"):
        errors.append("top level: 'updated' must equal the newest log entry's date")

    return errors


def main() -> int:
    try:
        data = load()
    except (OSError, ValueError) as exc:
        print(f"roadmap: cannot read {ROADMAP}: {exc}")
        return 1
    errors = check(data)
    for error in errors:
        print(f"roadmap: {error}")
    if errors:
        return 1
    tasks = [t for m in data["milestones"] for t in m["tasks"] if t["status"] != "dropped"]
    done = sum(t["status"] == "done" for t in tasks)
    released = [r["version"] for r in data["releases"] if r["status"] == "released"]
    latest = released[-1] if released else "none"
    print(f"roadmap ok: {done}/{len(tasks)} tasks done, latest release {latest}, updated {data['updated']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
