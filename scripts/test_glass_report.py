"""The glass report turns glass.log lines into a per-app summary and names the first missing step."""
import sys
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parent))
import glass_report

LINES = [
    "19:00:00.000 glass Notepad.exe[100] step=loaded component=dll build=Oct__5_2026_19:00:00 path=C:\\x.dll",
    "19:00:00.010 glass Notepad.exe[100] step=client-start",
    "19:00:00.020 glass Notepad.exe[100] step=connect result=ok",
    "19:00:01.000 glass Notepad.exe[100] step=eligible value=1 reason=ok fg=1 fgpid=100 view=2 gen=3",
    "19:00:01.100 glass Notepad.exe[100] step=publish rev=1 gen=3 words=5 selection=0",
    "19:00:01.110 glass CompletionistRenderer.exe[200] step=refused app=100 rev=1 gen=3 why=stale fgpid=100 fg=1 host=1 words=5 selection=0",
    "19:00:01.120 glass Notepad.exe[100] step=ack rev=1 presented=0 accepted=1",
    "19:00:01.130 glass Notepad.exe[100] step=render external=1 eligible=1 published=1 healthy=0 host=shown",
    "19:00:01.900 glass Claude.exe[300] step=loaded component=dll build=b path=p",
    "19:00:02.000 glass Claude.exe[300] step=connect result=fail reason=wait error=2",
]


class GlassReport(unittest.TestCase):
    def test_renderer_connection_is_evidence_even_without_a_dll_banner(self):
        app = glass_report.summarize([
            "19:00:01.000 glass CompletionistRenderer.exe[200] step=client-connected app=100",
        ])[100]
        self.assertTrue(app["connected"])
        self.assertIn("no banner", glass_report.diagnose(app))

    def test_published_without_a_draw_is_not_reported_as_success(self):
        app = glass_report.summarize(LINES[:5])[100]
        self.assertIn("no renderer draw", glass_report.diagnose(app))

    def test_ineligible_and_missing_caret_name_the_reason(self):
        lines = LINES[:3] + [
            "19:00:01.000 glass Notepad.exe[100] step=eligible value=0 reason=different-root",
        ]
        app = glass_report.summarize(lines)[100]
        self.assertIn("different-root", glass_report.diagnose(app))
        lines.append("19:00:01.100 glass Notepad.exe[100] step=no-caret caret=0,0,0,0")
        self.assertIn("caret", glass_report.diagnose(glass_report.summarize(lines)[100]))

    def test_opaque_popup_is_not_called_glass(self):
        app = glass_report.summarize(LINES[:5] + [
            "19:00:01.110 glass CompletionistRenderer.exe[200] step=opaque app=100 why=material",
            "19:00:01.120 glass CompletionistRenderer.exe[200] step=drawn app=100 drawn=1 current=1",
        ])[100]
        self.assertEqual(app["renderer_failures"], {"opaque/material": 1})
        self.assertIn("opaque/material", glass_report.diagnose(app))
        self.assertNotIn("glass drawn", glass_report.diagnose(app))

    def test_invalid_app_pid_is_ignored(self):
        apps = glass_report.summarize(LINES + [
            "19:00:03.000 glass CompletionistRenderer.exe[200] step=refused app=bad why=stale",
        ])
        self.assertEqual(set(apps), {100, 300})

    def test_cli_reads_fallback_and_explains_renderer_only_log(self):
        with TemporaryDirectory() as temporary:
            log = Path(temporary) / "glass.log"
            log.write_text("19:00:00.000 glass CompletionistRenderer.exe[200] step=loaded component=renderer build=b path=p\n", encoding="utf-8")
            output = StringIO()
            with redirect_stdout(output):
                result = glass_report.main(["--log", str(log)])
            self.assertEqual(result, 1)
            self.assertIn("Renderer [200] build=b", output.getvalue())
            self.assertIn("no app DLL entries", output.getvalue())
            (log.parent / "glass-100.log").write_text("\n".join(LINES[:5]), encoding="utf-8")
            with redirect_stdout(StringIO()):
                self.assertEqual(glass_report.main(["--log", str(log)]), 0)

    def test_clear_creates_log_directory_and_removes_fallback_logs(self):
        with TemporaryDirectory() as temporary:
            log = Path(temporary) / "new" / "glass.log"
            with redirect_stdout(StringIO()):
                self.assertEqual(glass_report.main(["--clear", "--log", str(log)]), 0)
            fallback = log.parent / "glass-100.log"
            fallback.write_text("old log", encoding="utf-8")
            other = log.parent / "engine.log"
            other.write_text("keep", encoding="utf-8")
            with redirect_stdout(StringIO()):
                self.assertEqual(glass_report.main(["--clear", "--log", str(log)]), 0)
            self.assertEqual(log.read_text(encoding="utf-8"), "")
            self.assertFalse(fallback.exists())
            self.assertEqual(other.read_text(encoding="utf-8"), "keep")

    def test_summary_counts_steps_per_app_and_attaches_renderer_lines(self):
        apps = glass_report.summarize(LINES)
        notepad = apps[100]
        self.assertEqual(notepad["exe"], "Notepad.exe")
        self.assertEqual(notepad["build"], "Oct__5_2026_19:00:00")
        self.assertEqual(notepad["steps"]["publish"], 1)
        self.assertEqual(notepad["refused"], {"stale": 1})
        self.assertEqual(notepad["host"], {"shown": 1})
        self.assertEqual(apps[300]["connect_failures"], {"wait/2": 1})

    def test_diagnosis_names_the_first_missing_step(self):
        apps = glass_report.summarize(LINES)
        self.assertIn("refused", glass_report.diagnose(apps[100]))
        self.assertIn("never connected", glass_report.diagnose(apps[300]))
        self.assertIn("no banner", glass_report.diagnose({"exe": "x", "build": None, "steps": {}, "refused": {},
                                                          "host": {}, "connect_failures": {}, "drawn": 0,
                                                          "connected": False}))


if __name__ == "__main__":
    unittest.main()
