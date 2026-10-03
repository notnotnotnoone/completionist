from __future__ import annotations

import subprocess
from pathlib import Path

import completionist_engine.renderer_process as renderer_process
from completionist_engine.renderer_process import RendererProcess


class FakeClock:
    now = 0.0

    def __call__(self) -> float:
        return self.now


class FakeChild:
    def __init__(self) -> None:
        self.exit_code: int | None = None
        self.terminated = False
        self.killed = False

    def poll(self) -> int | None:
        return self.exit_code

    def terminate(self) -> None:
        self.terminated = True
        self.exit_code = 0

    def kill(self) -> None:
        self.killed = True
        self.exit_code = -9

    def wait(self, timeout: float | None = None) -> int:
        assert timeout is not None and timeout <= 1.0
        if self.exit_code is None:
            raise subprocess.TimeoutExpired("renderer", timeout)
        return self.exit_code


class SlowChild(FakeChild):
    def wait(self, timeout: float | None = None) -> int:
        assert timeout is not None and timeout <= 1.0
        if not self.killed:
            raise subprocess.TimeoutExpired("renderer", timeout)
        return self.exit_code or -9


def executable(tmp_path: Path) -> Path:
    path = tmp_path / "CompletionistRenderer.exe"
    path.touch()
    return path


def test_missing_executable_leaves_host_fallback_and_does_not_launch(tmp_path: Path) -> None:
    launches: list[Path] = []
    service = RendererProcess(
        tmp_path / "missing.exe", launcher=lambda path: launches.append(path), session_probe=lambda: False
    )

    service.start()
    service.poll(10)

    assert launches == []
    assert service.enabled


def test_owned_child_stops_on_engine_shutdown(tmp_path: Path) -> None:
    child = FakeChild()
    service = RendererProcess(executable(tmp_path), launcher=lambda _: child, session_probe=lambda: False)
    service.start()

    service.stop()

    assert child.terminated
    assert not child.killed
    service.stop()  # idempotent


def test_owned_child_is_killed_only_after_bounded_stop_wait(tmp_path: Path) -> None:
    child = SlowChild()
    service = RendererProcess(executable(tmp_path), launcher=lambda _: child, session_probe=lambda: False)
    service.start()

    service.stop()

    assert child.terminated
    assert child.killed


def test_three_failures_within_sixty_seconds_disable_retries(tmp_path: Path) -> None:
    clock = FakeClock()
    children: list[FakeChild] = []

    def launch(_: Path) -> FakeChild:
        child = FakeChild()
        children.append(child)
        return child

    service = RendererProcess(executable(tmp_path), launcher=launch, clock=clock, session_probe=lambda: False)
    service.start()
    for moment in (10.0, 20.0, 30.0):
        children[-1].exit_code = 1
        clock.now = moment
        service.poll(moment)

    assert len(children) == 3
    assert not service.enabled


def test_existing_session_renderer_is_reused_without_spawning(tmp_path: Path) -> None:
    launches: list[Path] = []
    service = RendererProcess(
        executable(tmp_path), launcher=lambda path: launches.append(path), session_probe=lambda: True
    )

    service.start()
    service.poll(10)

    assert launches == []
    assert service.enabled


def test_startup_uses_only_injected_fixed_executable_path(tmp_path: Path) -> None:
    fixed_path = executable(tmp_path)
    requested: list[Path] = []
    service = RendererProcess(
        fixed_path,
        launcher=lambda path: requested.append(path) or FakeChild(),
        session_probe=lambda: False,
    )

    service.start()

    assert requested == [fixed_path]
    assert requested[0].is_absolute()


def test_hidden_launcher_uses_absolute_path_and_explicit_serve(monkeypatch, tmp_path: Path) -> None:
    path = executable(tmp_path).resolve()
    child = FakeChild()
    call: dict[str, object] = {}

    def fake_popen(command, **kwargs):
        call["command"] = command
        call.update(kwargs)
        return child

    monkeypatch.setattr(subprocess, "Popen", fake_popen)
    monkeypatch.setattr(subprocess, "CREATE_NO_WINDOW", 0x08000000, raising=False)

    result = renderer_process._launch_hidden(path)

    assert result is child
    assert call["command"] == [str(path), "--serve"]
    assert call["stdin"] is subprocess.DEVNULL
    assert call["stdout"] is subprocess.DEVNULL
    assert call["stderr"] is subprocess.DEVNULL
    assert call["creationflags"] == subprocess.CREATE_NO_WINDOW


def test_session_probe_uses_renderer_pipe_ownership_name() -> None:
    assert renderer_process._renderer_pipe_name(14, 0x1234) == r"\\.\pipe\completionist-renderer-v2-14-4660"


def test_retry_exhaustion_persists_until_engine_restart(tmp_path: Path) -> None:
    clock = FakeClock()
    attempts = 0

    def fail_to_launch(_: Path) -> FakeChild:
        nonlocal attempts
        attempts += 1
        raise OSError("fixture launch failure")

    service = RendererProcess(
        executable(tmp_path), launcher=fail_to_launch, clock=clock, session_probe=lambda: False
    )
    service.start()
    service.poll(10)
    service.poll(20)
    assert attempts == 3
    assert not service.enabled

    clock.now = 81
    service.poll(81)
    assert attempts == 3
    assert not service.enabled
