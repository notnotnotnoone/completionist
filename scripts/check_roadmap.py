"""Validate the release-destination roadmap. Run after every edit."""
from __future__ import annotations

import json
import re
import sys
from datetime import date
from pathlib import Path

ROADMAP = Path(__file__).resolve().parent.parent / "docs/roadmap/roadmap.js"
TASK_STATUSES = {"todo", "next", "doing", "blocked", "done", "dropped"}
RELEASE_STATUSES = {"released", "next", "planned"}
RISK_STATUSES = {"open", "mitigated", "retired"}
VERSION = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
ASSIGNMENT = re.compile(r"window\.COMPLETIONIST_ROADMAP\s*=\s*(\{.*\})\s*;\s*$", re.S)
SENTENCE_BREAK = re.compile(r'(?<=[.!?])["”’)]*\s+(?=["“(]?[A-Z0-9])')


def load(path: Path = ROADMAP) -> dict:
    match = ASSIGNMENT.search(path.read_text(encoding="utf-8"))
    if not match:
        raise ValueError("expected window.COMPLETIONIST_ROADMAP = { ... };")
    data = json.loads(match.group(1))
    if not isinstance(data, dict):
        raise ValueError("roadmap must be an object")
    return data


def vkey(version: str) -> tuple[int, ...]:
    return tuple(int(p) for p in version.split("."))


def tier(version: tuple[int, ...]) -> str:
    return "patch" if version[2] else "minor" if version[1] else "major"


def sentences(text: str) -> list[str]:
    return [s for s in SENTENCE_BREAK.split(text.strip()) if s]


def valid_date(value: object) -> bool:
    if not isinstance(value, str): return False
    try: return date.fromisoformat(value).isoformat() == value
    except ValueError: return False


def check(data: dict) -> list[str]:
    errors: list[str] = []

    def need(obj: dict, key: str, where: str) -> None:
        if not isinstance(obj.get(key), str) or not obj[key].strip():
            errors.append(f"{where}: '{key}' must be a non-empty string")

    def objects(key: str, required: bool = True) -> list[dict]:
        value = data.get(key, [])
        if not isinstance(value, list) or not all(isinstance(o, dict) for o in value) or (required and not value):
            errors.append(f"top level: '{key}' must be {'a non-empty' if required else 'an'} object list")
            return []
        return value

    if data.get("schema") != 4: errors.append("schema must be 4; run scripts/migrate_roadmap.py")
    if "milestones" in data: errors.append("milestones are obsolete; use tasks and releasePlans")
    for key in ("project", "now", "prd", "repo"): need(data, key, "top level")
    if not valid_date(data.get("updated")): errors.append("updated must be YYYY-MM-DD")
    releases, tasks, highways = objects("releases"), objects("tasks"), objects("highways")
    by_version = {r.get("version"): r for r in releases if isinstance(r.get("version"), str)}
    owners: dict[str, str] = {}
    highway_ids: set[str] = set()
    for h in highways:
        hid = h.get("id", "?"); where = f"highway {hid}"
        need(h, "id", where); need(h, "title", where)
        if not isinstance(hid, str): continue
        if hid in highway_ids: errors.append(f"{where}: duplicate id")
        highway_ids.add(hid)
        areas = h.get("areas")
        if not isinstance(areas, list) or not areas or not all(isinstance(a, str) and a for a in areas):
            errors.append(f"{where}: areas must be a non-empty string list"); areas = []
        for area in areas:
            if area in owners: errors.append(f"{where}: area {area} belongs to two highways")
            owners[area] = hid
        opening = h.get("opens")
        if opening is not None and (not isinstance(opening, str) or opening not in by_version or not VERSION.fullmatch(opening) or vkey(opening)[1:] != (0, 0)):
            errors.append(f"{where}: opens must reference a major release")

    previous = None; next_count = 0; seen_future = False
    for r in releases:
        version = r.get("version", "?"); where = f"release {version}"
        if not isinstance(version, str) or not VERSION.fullmatch(version) or version == "0.0.0":
            errors.append(f"{where}: version must be MAJOR.MINOR.PATCH"); continue
        parsed = vkey(version)
        if previous is not None and parsed <= previous: errors.append(f"{where}: versions must be unique and ascending")
        previous = parsed
        status = r.get("status")
        if not isinstance(status, str) or status not in RELEASE_STATUSES: errors.append(f"{where}: invalid status")
        if status == "released":
            if seen_future: errors.append(f"{where}: released versions must precede planned versions")
            if not valid_date(r.get("date")): errors.append(f"{where}: released version needs a valid date")
        else: seen_future = True
        next_count += status == "next"
        need(r, "title", where)
        kind = tier(parsed); touched = r.get("highways")
        if touched is not None or kind == "patch":
            if not isinstance(touched, list) or not touched or not all(isinstance(x, str) for x in touched) or len(set(touched)) != len(touched):
                errors.append(f"{where}: highways must be a non-empty unique list")
            else:
                for lane in touched:
                    if lane not in highway_ids: errors.append(f"{where}: unknown highway {lane}")
                    h = next((h for h in highways if h.get("id") == lane), {})
                    if status == "released" and h.get("opens") and VERSION.fullmatch(str(h["opens"])) and parsed < vkey(h["opens"]):
                        errors.append(f"{where}: highway {lane} has not opened")
        if "kind" in r and (kind != "patch" or r["kind"] != "fix"): errors.append(f"{where}: kind is only 'fix' on patches")
        text = r.get("text", "")
        if kind == "patch":
            if not isinstance(text, str) or "\n" in text or len(sentences(text)) != 1 or len(text.split()) > 50 or not text.rstrip().endswith((".", "!", "?")):
                errors.append(f"{where}: patch writing needs one sentence, at most 50 words")
        elif kind == "minor":
            if not isinstance(text, str) or "\n" in text or not 50 <= len(text.split()) <= 220 or len(sentences(text)) < 2:
                errors.append(f"{where}: minor writing needs one paragraph, 50–220 words")
        else:
            essay = r.get("essay", [])
            if not isinstance(essay, list) or len(essay) < 4 or not all(isinstance(p, str) and p.strip() for p in essay) or sum(len(p.split()) for p in essay) < 450:
                errors.append(f"{where}: major writing needs four paragraphs and at least 450 words")
    if next_count > 1: errors.append("at most one release can be next")

    plans = data.get("releasePlans", {})
    if not isinstance(plans, dict): errors.append("releasePlans must be an object"); plans = {}
    for version, plan in plans.items():
        where = f"release plan {version}"
        if version not in by_version: errors.append(f"{where}: unknown release")
        if not isinstance(plan, dict): errors.append(f"{where}: needs an object"); continue
        for key in ("title", "goal", "done_when"): need(plan, key, where)
        if not isinstance(plan.get("scope"), str) or plan["scope"] not in {"contained", "medium", "large"}: errors.append(f"{where}: scope must be contained, medium or large")
    active = data.get("activeRelease")
    if active is not None and (not isinstance(active, str) or active not in by_version or by_version[active].get("status") == "released" or active not in plans):
        errors.append("activeRelease must name an unreleased destination with a plan")
    if any(r.get("status") == "next" and r.get("version") != active for r in releases):
        errors.append("the next release must match activeRelease")
    if active is None and any(r.get("status") != "released" for r in releases):
        errors.append("activeRelease is required while future releases remain")

    stories = data.get("stories", {})
    if not isinstance(stories, dict) or not stories: errors.append("stories must be a non-empty object"); stories = {}
    ids: set[str] = set(); stages: set[tuple[str, int]] = set()
    for t in tasks:
        tid = t.get("id", "?"); where = f"task {tid}"
        for key in ("id", "title", "area", "targetRelease"): need(t, key, where)
        if not isinstance(tid, str): continue
        if tid in ids: errors.append(f"{where}: duplicate id")
        ids.add(tid)
        if "version" in t: errors.append(f"{where}: task version is obsolete; use targetRelease and shippedIn")
        if not isinstance(t.get("area"), str) or t["area"] not in owners: errors.append(f"{where}: area belongs to no highway")
        status = t.get("status")
        if not isinstance(status, str) or status not in TASK_STATUSES: errors.append(f"{where}: invalid status"); status = None
        target = t.get("targetRelease"); target_record = by_version.get(target) if isinstance(target, str) else None
        if not target_record: errors.append(f"{where}: unknown targetRelease {target}")
        elif target_record.get("status") == "released" and status not in {"done", "dropped"}:
            errors.append(f"{where}: released destination has unfinished work")
        if "shippedIn" in t:
            shipped = by_version.get(t["shippedIn"]) if isinstance(t["shippedIn"], str) else None
            if t.get("status") != "done" or not shipped or shipped.get("status") != "released":
                errors.append(f"{where}: shippedIn requires a done task and a released version")
        if target_record and target_record.get("status") == "released" and t.get("status") == "done" and "shippedIn" not in t:
            errors.append(f"{where}: a done task in a released destination needs shippedIn")
        if "stage" in t:
            stage = t["stage"]
            if type(stage) is not int or stage < 1: errors.append(f"{where}: stage must be a positive integer")
            elif isinstance(target, str):
                if (target, stage) in stages: errors.append(f"{where}: duplicate stage for targetRelease")
                else: stages.add((target, stage))
        for key in ("refs", "stories"):
            values = t.get(key, [])
            if not isinstance(values, list): errors.append(f"{where}: {key} must be a list"); continue
            for value in values:
                if key == "refs" and not isinstance(value, str): errors.append(f"{where}: refs must contain strings")
                if key == "stories" and str(value) not in stories: errors.append(f"{where}: unknown story {value}")
        if "completed" in t and not valid_date(t["completed"]): errors.append(f"{where}: completed must be a valid date")
    for d in objects("decisions", False):
        if not valid_date(d.get("date")) or not d.get("text"): errors.append("decision needs a date and text")
    for risk in objects("risks", False):
        if not isinstance(risk.get("status"), str) or risk["status"] not in RISK_STATUSES or not risk.get("text"): errors.append("risk needs status and text")
    log = objects("log")
    for entry in log:
        if not valid_date(entry.get("date")) or not entry.get("text"): errors.append("log needs a date and text")
    if all(isinstance(e.get("date"), str) for e in log) and [e["date"] for e in log] != sorted((e["date"] for e in log), reverse=True):
        errors.append("log must be newest first")
    if log and data.get("updated") != log[0].get("date"): errors.append("updated must match the newest log date")
    return errors


def main() -> int:
    try: data = load(); errors = check(data)
    except (OSError, ValueError) as exc:
        print(f"roadmap: cannot read source: {exc}"); return 1
    if errors:
        for error in errors: print(f"roadmap: {error}")
        return 1
    tasks = [t for t in data["tasks"] if t["status"] != "dropped"]
    released = [r["version"] for r in data["releases"] if r["status"] == "released"]
    print(f"roadmap ok: {sum(t['status'] == 'done' for t in tasks)}/{len(tasks)} tasks done, latest release {released[-1] if released else 'none'}, target {data.get('activeRelease')}, updated {data['updated']}")
    return 0


if __name__ == "__main__": sys.exit(main())
