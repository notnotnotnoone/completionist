"""Contracts for releases as destinations and safe historical migration."""
from copy import deepcopy
import importlib
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_roadmap import check


def fixture():
    return {
        "schema": 4, "project": "Completionist", "updated": "2026-10-02",
        "now": "Building words.", "prd": "https://example.com/prd", "repo": "https://example.com/repo",
        "activeRelease": "0.1.0",
        "highways": [{"id": "engine", "title": "Engine", "areas": ["engine"]},
                     {"id": "tooling", "title": "Tooling", "areas": ["docs"]}],
        "releasePlans": {"0.1.0": {"title": "Words", "goal": "Complete words.",
                         "done_when": "Words pass validation.", "scope": "contained"}},
        "tasks": [{"id": "first", "targetRelease": "0.1.0", "title": "Rank words",
                   "status": "next", "area": "engine", "stories": [1], "notes": "Full first note."},
                  {"id": "second", "targetRelease": "0.1.0", "title": "Document words",
                   "status": "todo", "area": "docs", "stories": [1], "notes": "Full second note."}],
        "releases": [{"version": "0.0.1", "status": "released", "date": "2026-09-29",
                      "title": "Groundwork", "highways": ["tooling"], "text": "The groundwork shipped."},
                     {"version": "0.1.0", "status": "next", "title": "Words", "text":
                      "The release adds useful word suggestions. " + "It keeps word ranking predictable and makes the first typing experience easier to follow. " * 4}],
        "stories": {"1": "Useful words"}, "decisions": [], "risks": [],
        "log": [{"date": "2026-10-02", "text": "Planned words."}]
    }


class RoadmapModel(unittest.TestCase):
    def test_several_tasks_can_share_a_destination_without_versions(self):
        self.assertEqual(check(fixture()), [])

    def test_fake_task_versions_are_rejected(self):
        d = fixture(); d["tasks"][0]["version"] = "0.0.2"
        self.assertTrue(any("task version" in e for e in check(d)))

    def test_shipping_reference_requires_done_task_and_released_version(self):
        for status, shipped in [("next", "0.0.1"), ("done", "0.1.0")]:
            with self.subTest(status=status):
                d = fixture(); d["tasks"][0].update(status=status, shippedIn=shipped)
                self.assertTrue(any("shippedIn" in e for e in check(d)))

    def test_released_destination_cannot_have_unfinished_work(self):
        d = fixture(); d["activeRelease"] = None
        d["releases"][1].update(status="released", date="2026-10-02")
        self.assertTrue(any("unfinished" in e for e in check(d)))

    def test_active_destination_must_exist_and_be_unreleased(self):
        for active in ["9.0.0", "0.0.1"]:
            d = fixture(); d["activeRelease"] = active
            self.assertTrue(any("activeRelease" in e for e in check(d)))

    def test_bad_area_unknown_target_and_duplicate_stage_are_rejected(self):
        d = fixture(); d["tasks"][0].update(area="missing", targetRelease="9.0.0")
        errors = check(d)
        self.assertTrue(any("area" in e for e in errors))
        self.assertTrue(any("targetRelease" in e for e in errors))
        d = fixture()
        for t in d["tasks"]: t["stage"] = 1
        self.assertTrue(any("stage" in e for e in check(d)))

    def test_migration_preserves_history_and_is_idempotent(self):
        spec = importlib.util.find_spec("migrate_roadmap")
        self.assertIsNotNone(spec, "A tested migration is required")
        migrate = importlib.import_module("migrate_roadmap").migrate
        old = fixture(); old["schema"] = 3
        old.pop("activeRelease"); old.pop("releasePlans"); old.pop("tasks")
        old["releases"][1]["milestone"] = "M1"
        old["releases"].insert(1, {"version": "0.0.2", "status": "planned", "title": "Rank words"})
        old["milestones"] = [{"id": "M1", "title": "Words", "status": "active", "goal": "Words.",
                              "done_when": "Validated.", "tasks": [
            {"id": "M1.1", "version": "0.0.1", "title": "Historical docs", "status": "done", "area": "docs", "stories": [1], "refs": ["old.py"], "notes": "History."},
            {"id": "M1.2", "version": "0.0.2", "title": "Stage 1/9: Rank words", "status": "next", "area": "engine", "stories": [1]},
            {"id": "M1.3", "version": "0.0.3", "title": "Abandoned", "status": "dropped", "area": "docs", "stories": []}]}]
        original = deepcopy(old); new = migrate(old)
        self.assertEqual(old, original)
        self.assertEqual([r for r in new["releases"] if r["status"] == "released"], [original["releases"][0]])
        self.assertEqual(new["log"], old["log"])
        self.assertEqual(new["tasks"][0]["shippedIn"], "0.0.1")
        self.assertEqual(new["tasks"][1]["targetRelease"], "0.1.0")
        self.assertEqual(new["tasks"][1]["stage"], 1)
        self.assertEqual(new["tasks"][2]["status"], "dropped")
        self.assertEqual(new, migrate(new))
        self.assertEqual(check(new), [])

    def test_malformed_references_report_errors_instead_of_crashing(self):
        for key, value in [("targetRelease", {}), ("shippedIn", []), ("id", [])]:
            with self.subTest(key=key):
                d = fixture(); d["tasks"][0][key] = value
                self.assertTrue(check(d))
        d = fixture(); d["releases"][0]["highways"] = [{}]
        self.assertTrue(check(d))


if __name__ == "__main__": unittest.main()
