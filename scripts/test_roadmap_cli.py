"""Integration checks for briefs generated from the real schema-4 roadmap."""
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import roadmap
from check_roadmap import ROADMAP, check, load


class RoadmapBriefs(unittest.TestCase):
    def test_real_source_is_valid(self):
        self.assertEqual(check(load()), [])

    def test_generated_views_do_not_mutate_source(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "roadmap.js"
            original = ROADMAP.read_bytes(); path.write_bytes(original)
            for level in roadmap.BRIEF_LEVELS:
                with redirect_stdout(io.StringIO()) as output:
                    code = roadmap.main(["brief", "--level", level], path=path)
                self.assertEqual(code, 0)
                self.assertEqual(output.getvalue().rstrip(), path.with_name(f"roadmap-brief-{level}.md").read_text(encoding="utf-8").rstrip())
            self.assertEqual(path.read_bytes(), original)
            self.assertEqual(path.with_name("roadmap-brief.md").read_bytes(), path.with_name("roadmap-brief-minimal.md").read_bytes())

    def test_maximum_is_lossless_and_minimal_is_focused(self):
        data = load(); maximum = roadmap.build_full_brief(data, "maximum")
        self.assertEqual(json.loads(maximum.split("```json\n", 1)[1].split("\n```", 1)[0]), data)
        minimal = roadmap.build_full_brief(data, "minimal")
        self.assertLess(len(minimal.splitlines()), 100)
        self.assertIn("target 2.0.0", minimal)
        self.assertNotIn("1.1.15", minimal)

    def test_recent_shipping_matches_actual_shipping_not_target_group(self):
        data = load(); medium = roadmap.build_full_brief(data, "medium")
        section = medium.split("## Recently shipped releases", 1)[1]
        self.assertEqual(section.count("### "), min(5, sum(r["status"] == "released" for r in data["releases"])))
        self.assertNotIn("Prove live soft glass", section)
        for t in data["tasks"]:
            if t["targetRelease"] == data["activeRelease"] and t["status"] == "todo":
                self.assertIn(t["title"], medium)
                if t.get("notes"): self.assertIn(" ".join(t["notes"].split()), medium)


if __name__ == "__main__": unittest.main()
