"""Idempotent schema-3 to schema-4 migration; preserve all released records."""
from __future__ import annotations

from copy import deepcopy
import json
import re
from check_roadmap import ROADMAP, load, check


def migrate(source: dict) -> dict:
    data = deepcopy(source)
    if data.get("schema") == 4: return data
    if data.get("schema") != 3: raise ValueError("migration expects schema 3 or 4")
    groups = data.pop("milestones")
    destinations = {r["milestone"]: r["version"] for r in data["releases"] if r.get("milestone")}
    released = {r["version"]: r for r in data["releases"] if r["status"] == "released"}
    tasks = []; plans = {}; pseudo = set(); active = None
    labels = ["Live glass proof", "AI lifecycle", "Renderer IPC", "Native layout", "Glass & motion", "Host fallback", "TSF integration", "Packaging", "Validation"]
    for group in groups:
        target = destinations.get(group["id"])
        if target is None: raise ValueError(f"no destination for {group['id']}")
        plans[target] = {k: group[k] for k in ("title", "goal", "done_when")}
        plans[target]["scope"] = "large" if target in {"2.0.0", "3.0.0"} else "contained" if target == "2.1.0" else "medium"
        if target == "2.0.0": plans[target]["title"] = "Desktop overhaul"
        if target == "3.0.0": plans[target]["title"] = "Browser autocomplete"
        if group["status"] == "active": active = target
        for task in group["tasks"]:
            version = task.pop("version")
            task["targetRelease"] = target
            if task["status"] == "done" and version in released:
                task["shippedIn"] = version
            elif task["status"] == "done":
                raise ValueError(f"done task {task['id']} has no released version {version}")
            if version not in released: pseudo.add(version)
            stage = re.fullmatch(r"Stage (\d+)/(\d+): (.*)", task["title"])
            if stage:
                task["stage"] = int(stage[1]); task["title"] = stage[3]
                if target == "2.0.0" and 1 <= task["stage"] <= 9: task["label"] = labels[task["stage"] - 1]
            tasks.append(task)
    data["releases"] = [r for r in data["releases"] if r["status"] == "released" or r["version"] not in pseudo or r.get("milestone")]
    data["schema"] = 4
    data["activeRelease"] = active
    data["releasePlans"] = plans
    data["tasks"] = tasks
    return data


def main() -> int:
    before = load(); after = migrate(before)
    errors = check(after)
    if errors: raise ValueError("migration failed validation: " + "; ".join(errors))
    old_released = [r for r in before["releases"] if r["status"] == "released"]
    assert old_released == [r for r in after["releases"] if r["status"] == "released"]
    if before.get("schema") == 3:
        old_tasks = [t for group in before["milestones"] for t in group["tasks"]]
        assert [t["id"] for t in old_tasks] == [t["id"] for t in after["tasks"]]
        for old, new in zip(old_tasks, after["tasks"]):
            for key, value in old.items():
                if key not in {"version", "title"}: assert new[key] == value
            expected_title = re.sub(r"^Stage \d+/\d+: ", "", old["title"])
            assert new["title"] == expected_title
        assert before["log"] == after["log"]
    if after != before:
        ROADMAP.write_text("window.COMPLETIONIST_ROADMAP = " + json.dumps(after, ensure_ascii=False, indent=2) + ";\n", encoding="utf-8")
    print(f"migration ok: {len(old_released)} released records preserved, {len(after['tasks'])} task identities preserved")
    return 0


if __name__ == "__main__": main()
