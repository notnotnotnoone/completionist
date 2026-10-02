"""Tests for scripts/roadmap.py, run against a temp copy of the real roadmap. Run: python -m unittest scripts/test_roadmap_cli.py"""
import io
import shutil
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import roadmap  # noqa: E402
from check_roadmap import ROADMAP, check, load  # noqa: E402


class RoadmapCli(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.path = self.tmp / "roadmap.js"
        shutil.copy(ROADMAP, self.path)
        self.before = self.path.read_text(encoding="utf-8")

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def run_cli(self, *argv):
        with redirect_stdout(io.StringIO()) as out:
            code = roadmap.main(list(argv), path=self.path, today="2026-10-02")
        return code, out.getvalue()

    def task(self, task_id):
        return roadmap._find(load(self.path), task_id)[1]

    def test_brief_is_short_and_leaves_out_essays(self):
        code, out = self.run_cli("brief")
        self.assertEqual(code, 0)
        self.assertLess(len(out.splitlines()), 150)
        self.assertNotIn("Completionist 2.0 is a redesign", out)
        self.assertIn("Avoid opening `docs/roadmap/roadmap.js`", out)
        self.assertTrue((self.tmp / "roadmap-brief.md").exists())

    def test_start_sets_doing_and_logs(self):
        code, _ = self.run_cli("start", "M10.1")
        self.assertEqual(code, 0)
        self.assertEqual(self.task("M10.1")["status"], "doing")
        data = load(self.path)
        self.assertEqual(data["updated"], "2026-10-02")
        self.assertEqual(data["log"][0]["text"], "Started M10.1.")
        self.assertEqual(check(data), [])

    def test_done_adds_release_refs_and_promotes_next_todo(self):
        self.run_cli("start", "M10.3")
        code, _ = self.run_cli("done", "M10.3", "--title", "Dashboard", "--text", "The dashboard was rebuilt.",
                               "--refs", "engine/a.py,engine/b.py", "--note", "Rebuilt it.", "--highways", "engine")
        self.assertEqual(code, 0)
        t = self.task("M10.3")
        self.assertEqual((t["status"], t["refs"], t["notes"]), ("done", ["engine/a.py", "engine/b.py"], "Rebuilt it."))
        data = load(self.path)
        rel = next(r for r in data["releases"] if r["version"] == t["version"])
        self.assertEqual((rel["status"], rel["highways"], rel["date"]), ("released", ["engine"], "2026-10-02"))
        versions = [r["version"] for r in data["releases"]]
        self.assertEqual(versions, sorted(versions, key=roadmap.vkey))
        self.assertEqual(self.task("M10.1")["status"], "next")

    def test_a_rejected_edit_is_undone(self):
        code, out = self.run_cli("done", "M10.3", "--title", "x", "--text", " ".join(["word"] * 80))
        self.assertEqual(code, 1)
        self.assertIn("rejected", out)
        self.assertEqual(self.path.read_text(encoding="utf-8"), self.before)

    def test_log_and_add_task(self):
        self.assertEqual(self.run_cli("log", "Did a thing.")[0], 0)
        self.assertEqual(load(self.path)["log"][0]["text"], "Did a thing.")
        code, out = self.run_cli("add-task", "M10", "--area", "engine", "--title", "New work", "--note", "Why.")
        self.assertEqual(code, 0, out)
        new = self.task(out.split()[1])  # "added M10.11 at version ..."
        self.assertEqual((new["status"], new["area"], new["title"]), ("todo", "engine", "New work"))

    def test_unknown_task_leaves_file_alone(self):
        with self.assertRaises(SystemExit):
            self.run_cli("start", "M99.9")
        self.assertEqual(self.path.read_text(encoding="utf-8"), self.before)


if __name__ == "__main__":
    unittest.main()
