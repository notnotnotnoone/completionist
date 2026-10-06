"""Summarize glass.log: per app, which build ran, what it tried, what the renderer answered.

python scripts/glass_report.py          print the report
python scripts/glass_report.py --clear  empty the log before a test
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path

LOG = Path(os.environ.get("LOCALAPPDATA", "")) / "Completionist" / "glass.log"
LINE = re.compile(r"^(\d\d:\d\d:\d\d\.\d{3}) glass (\S+)\[(\d+)\] (.*)$")
PAIR = re.compile(r"(\w+)=(\S+)")
RENDERER = "CompletionistRenderer.exe"


def _app(apps: dict[int, dict], pid: int, exe: str | None = None) -> dict:
    app = apps.setdefault(pid, {"exe": exe, "build": None, "steps": Counter(), "refused": Counter(),
                                "host": Counter(), "connect_failures": Counter(), "drawn": 0, "connected": False,
                                "last": "", "path": None, "eligibility": Counter(),
                                "renderer_failures": Counter()})
    if exe and not app["exe"]:
        app["exe"] = exe
    return app


def summarize(lines: list[str]) -> dict[int, dict]:
    apps: dict[int, dict] = {}
    for raw in lines:
        match = LINE.match(raw.strip())
        if not match:
            continue
        time, exe, pid, rest = match.group(1), match.group(2), int(match.group(3)), match.group(4)
        fields = dict(PAIR.findall(rest))
        step = fields.get("step", "?")
        if exe.casefold() == RENDERER.casefold():
            if "app" not in fields:
                continue
            if not fields["app"].isdigit() or int(fields["app"]) == 0:
                continue
            app = _app(apps, int(fields["app"]))
            if step == "refused":
                app["refused"][fields.get("why", "?")] += 1
            elif step == "drawn" and fields.get("drawn") == "1" and fields.get("current") == "1":
                app["drawn"] += 1
            if step in {"opaque", "present-skipped", "glass-failed"}:
                app["renderer_failures"][step + "/" + fields.get("why", "draw-failed")] += 1
            app["steps"]["renderer:" + step] += 1
        else:
            app = _app(apps, pid, exe)
            app["steps"][step] += 1
            if step == "loaded":
                app["build"] = fields.get("build")
                app["path"] = fields.get("path")
            elif step == "eligible":
                app["eligibility"][fields.get("reason", "?")] += 1
            elif step == "connect" and fields.get("result") == "ok":
                app["connected"] = True
            elif step == "connect" and fields.get("result") == "fail":
                app["connect_failures"][f"{fields.get('reason', '?')}/{fields.get('error', '?')}"] += 1
            elif step == "render" and "host" in fields:
                app["host"][fields["host"]] += 1
        app["last"] = time
    for app in apps.values():
        app["steps"] = dict(app["steps"])
        app["refused"] = dict(app["refused"])
        app["host"] = dict(app["host"])
        app["connect_failures"] = dict(app["connect_failures"])
        app["eligibility"] = dict(app["eligibility"])
        app["renderer_failures"] = dict(app["renderer_failures"])
    return apps


def diagnose(app: dict) -> str:
    steps = app["steps"]
    if not app["build"]:
        return "no banner: this process never logged a DLL load (old build, or the DLL never activated here)"
    if not app["connected"]:
        failures = ", ".join(app["connect_failures"]) or "no attempt logged"
        return "never connected to the renderer: " + failures
    if not steps.get("eligible"):
        return "never checked eligibility (no focus change logged since load)"
    if not steps.get("publish"):
        if steps.get("no-caret"):
            return "nothing published: caret could not be converted to screen pixels"
        reasons = ", ".join(f"{k} x{v}" for k, v in app.get("eligibility", {}).items())
        if reasons:
            return "nothing published; eligibility: " + reasons
        return "eligible checks ran but nothing was published to the renderer (see step=eligible reasons)"
    if app["refused"] and not app["drawn"]:
        return "renderer refused requests; no confirmed draw: " + ", ".join(f"{k} x{v}" for k, v in app["refused"].items())
    failures = ", ".join(f"{k} x{v}" for k, v in app.get("renderer_failures", {}).items())
    if not app["drawn"]:
        if failures:
            return "renderer did not confirm a popup draw: " + failures
        if steps.get("renderer:drawn"):
            return "renderer did not present the popup or the request was no longer current"
        if steps.get("timeout"):
            return "published, but renderer did not answer before the fallback timeout"
        return "published, but no renderer draw was logged"
    result = f"renderer popup drawn {app['drawn']} times; flat popup shown {app['host'].get('shown', 0)} times"
    if failures:
        result += "; material/presentation events: " + failures
    return result


def main(argv: list[str] | None = None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clear", action="store_true", help="empty the log before a test")
    parser.add_argument("--log", type=Path, default=LOG)
    args = parser.parse_args(argv)
    try:
        if args.clear:
            args.log.parent.mkdir(parents=True, exist_ok=True)
            args.log.write_text("", encoding="utf-8")
            for extra in args.log.parent.glob("glass-*.log"):
                if extra != args.log:
                    extra.unlink()
            print(f"cleared {args.log}")
            return 0
        lines: list[str] = []
        for path in dict.fromkeys([args.log, *sorted(args.log.parent.glob("glass-*.log"))]):
            if path.exists():
                lines += path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as error:
        print(f"glass log could not be read or cleared: {error}", file=sys.stderr)
        return 2
    if not lines:
        print(f"no glass log lines in {args.log}")
        return 1
    for raw in lines:
        match = LINE.match(raw.strip())
        if not match or match[2].casefold() != RENDERER.casefold():
            continue
        fields = dict(PAIR.findall(match[4]))
        if fields.get("step") == "loaded":
            print(f"Renderer [{match[3]}] build={fields.get('build', '-')} path={fields.get('path', '-')}")
    apps = summarize(lines)
    if not apps:
        print("no app DLL entries: no app has logged activation since the log was cleared; restart the app process and type")
        return 1
    for pid, app in sorted(apps.items(), key=lambda item: item[1]["last"]):
        print(f"{app['exe'] or '?'} [{pid}] build={app['build'] or '-'} last={app['last']}")
        print(f"  DLL path: {app['path'] or '-'}")
        print(f"  connected at least once: {'yes' if app['connected'] else 'no'}; "
              f"requests published: {app['steps'].get('publish', 0)}; "
              f"fallback timeouts: {app['steps'].get('timeout', 0)}; "
              f"disconnects: {app['steps'].get('disconnect', 0)}")
        print(f"  steps: {app['steps']}")
        if app["refused"]:
            print(f"  refused: {app['refused']}")
        if app["connect_failures"]:
            print(f"  connect failures: {app['connect_failures']}")
        if app["eligibility"]:
            print(f"  eligibility: {app['eligibility']}")
        if app["renderer_failures"]:
            print(f"  material/presentation: {app['renderer_failures']}")
        print(f"  host popup: {app['host'] or '-'}   renderer popup drawn: {app['drawn']}")
        print(f"  => {diagnose(app)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
