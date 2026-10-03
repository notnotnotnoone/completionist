"""Exercise roadmap commands against small files, never the live roadmap."""
import io
import sys
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import roadmap
from check_roadmap import load, check
from test_roadmap_model import fixture


class RoadmapCli(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "roadmap.js"
        self.path.write_text("window.COMPLETIONIST_ROADMAP = " + json.dumps(fixture(), indent=2) + ";\n", encoding="utf-8")

    def tearDown(self): self.tmp.cleanup()

    def run_cli(self, *args):
        with redirect_stdout(io.StringIO()) as output:
            try: code = roadmap.main(list(args), path=self.path, today="2026-10-02")
            except SystemExit as e: code = e.code if isinstance(e.code, int) else 1
        return code, output.getvalue()

    def task(self, id): return next(t for t in load(self.path)["tasks"] if t["id"] == id)

    def test_done_records_progress_without_creating_a_release(self):
        before = load(self.path)["releases"]
        code, out = self.run_cli("done", "first", "--refs", "engine/rank.py", "--note", "Validated ranking.")
        self.assertEqual(code, 0, out)
        self.assertEqual(load(self.path)["releases"], before)
        self.assertEqual(self.task("first")["status"], "done")
        self.assertNotIn("shippedIn", self.task("first"))
        self.assertEqual(self.task("second")["status"], "next")
        self.assertEqual(check(load(self.path)), [])

    def test_add_and_reorder_never_allocate_versions_or_change_ids(self):
        before = load(self.path)["releases"]
        code, out = self.run_cli("add-task", "0.1.0", "--id", "extra", "--area", "docs", "--title", "Explain words", "--before", "second")
        self.assertEqual(code, 0, out)
        self.assertEqual([t["id"] for t in load(self.path)["tasks"]], ["first", "extra", "second"])
        self.assertNotIn("version", self.task("extra"))
        self.assertEqual(self.run_cli("move", "second", "--before", "first")[0], 0)
        self.assertEqual([t["id"] for t in load(self.path)["tasks"]], ["second", "first", "extra"])
        self.assertEqual(load(self.path)["releases"], before)

    def test_explicit_patch_release_can_include_multiple_completed_tasks(self):
        for id in ["first", "second"]: self.assertEqual(self.run_cli("done", id)[0], 0)
        code, out = self.run_cli("release", "0.0.2", "--tasks", "first,second", "--title", "Word fixes", "--text", "Word ranking and its documentation were improved.", "--validated", "Ranking and docs reviewed.")
        self.assertEqual(code, 0, out)
        self.assertEqual(self.task("first")["shippedIn"], "0.0.2")
        self.assertEqual(self.task("second")["shippedIn"], "0.0.2")
        self.assertEqual(load(self.path)["activeRelease"], "0.1.0")
        self.assertEqual(load(self.path)["releases"][1]["highways"], ["engine", "tooling"])

    def test_release_rejects_unfinished_work_without_changing_file(self):
        before = self.path.read_bytes()
        code, _ = self.run_cli("release", "0.1.0", "--validated", "Reviewed.")
        self.assertNotEqual(code, 0)
        self.assertEqual(self.path.read_bytes(), before)

    def test_retarget_preserves_identity_and_notes_and_removes_local_stage(self):
        data = load(self.path)
        data["releases"].append({**data["releases"][-1], "version": "0.2.0", "status": "planned"})
        data["releasePlans"]["0.2.0"] = dict(data["releasePlans"]["0.1.0"])
        data["tasks"][0]["stage"] = 1
        self.path.write_text("window.COMPLETIONIST_ROADMAP = " + json.dumps(data) + ";\n", encoding="utf-8")
        original = self.task("first")
        code, out = self.run_cli("retarget", "first", "0.2.0")
        self.assertEqual(code, 0, out)
        self.assertEqual(self.task("first")["id"], original["id"])
        self.assertEqual(self.task("first")["notes"], original["notes"])
        self.assertEqual(self.task("first")["targetRelease"], "0.2.0")
        self.assertNotIn("stage", self.task("first"))
        self.assertIn("from 0.1.0 to 0.2.0", load(self.path)["log"][0]["text"])

    def test_dropped_work_keeps_its_history_and_cannot_be_retargeted(self):
        self.assertEqual(self.run_cli("drop", "first", "--reason", "Superseded.")[0], 0)
        self.assertIn("Full first note.", self.task("first")["notes"])
        self.assertIn("Superseded.", self.task("first")["notes"])
        before = self.path.read_bytes()
        self.assertNotEqual(self.run_cli("retarget", "first", "0.1.0")[0], 0)
        self.assertEqual(self.path.read_bytes(), before)

    def test_destination_shipping_preserves_earlier_patch_shipping(self):
        self.run_cli("done", "first")
        self.assertEqual(self.run_cli("release", "0.0.2", "--tasks", "first", "--title", "Ranking fix", "--text", "Word ranking was improved.", "--validated", "Ranking checked.")[0], 0)
        self.run_cli("done", "second")
        self.assertEqual(self.run_cli("release", "0.1.0", "--validated", "Integration passed.")[0], 0)
        self.assertEqual(self.task("first")["shippedIn"], "0.0.2")
        self.assertEqual(self.task("second")["shippedIn"], "0.1.0")

    def test_ready_destination_is_not_automatically_shipped(self):
        for id in ["first", "second"]: self.run_cli("done", id)
        self.assertEqual(load(self.path)["releases"][1]["status"], "next")
        self.assertEqual(self.run_cli("release", "0.1.0", "--validated", "Integration passed.")[0], 0)
        self.assertEqual(load(self.path)["releases"][1]["status"], "released")
        self.assertIsNone(load(self.path)["activeRelease"])

    def test_invalid_task_and_unknown_id_leave_bytes_unchanged(self):
        before = self.path.read_bytes()
        self.assertNotEqual(self.run_cli("add-task", "0.1.0", "--area", "unknown", "--title", "Wrong")[0], 0)
        self.assertEqual(self.path.read_bytes(), before)
        self.assertNotEqual(self.run_cli("start", "missing")[0], 0)
        self.assertEqual(self.path.read_bytes(), before)

    def test_briefs_distinguish_completed_unshipped_work_and_shipped_releases(self):
        self.run_cli("done", "first"); before = self.path.read_bytes()
        for level in roadmap.BRIEF_LEVELS:
            code, out = self.run_cli("brief", "--level", level)
            self.assertEqual(code, 0, out)
            self.assertEqual(out.rstrip(), self.path.with_name(f"roadmap-brief-{level}.md").read_text(encoding="utf-8").rstrip())
        minimal = self.path.with_name("roadmap-brief-minimal.md").read_text(encoding="utf-8")
        medium = self.path.with_name("roadmap-brief-medium.md").read_text(encoding="utf-8")
        self.assertIn("0.1.0", minimal)
        for value in ["Recently completed work", "not yet shipped", "Recently shipped releases", "Full first note."]:
            self.assertIn(value, medium)
        self.assertEqual(self.path.read_bytes(), before)

    def test_brief_rejects_invalid_source_before_generating_files(self):
        d = fixture(); d["tasks"][0]["targetRelease"] = "9.0.0"
        self.path.write_text("window.COMPLETIONIST_ROADMAP = " + json.dumps(d) + ";\n", encoding="utf-8")
        self.assertNotEqual(self.run_cli("brief")[0], 0)
        self.assertFalse(self.path.with_name("roadmap-brief.md").exists())


if __name__ == "__main__": unittest.main()
