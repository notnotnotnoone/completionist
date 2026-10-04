from types import SimpleNamespace

import pytest

from completionist_engine import app


@pytest.mark.parametrize(('arguments', 'enabled'), [([], True), (['--serve-renderer'], True), (['--no-serve-renderer'], False)])
def test_startup_selects_renderer_without_launching_desktop(monkeypatch, tmp_path, arguments, enabled):
    closed = []
    assembled = SimpleNamespace(close=lambda: closed.append(True))
    monkeypatch.setattr(app, 'load_config', lambda _: SimpleNamespace(data_dir=tmp_path))
    monkeypatch.setattr(app, 'setup_logging', lambda *_: None)
    monkeypatch.setattr(app, 'load_wordfreq_vocabulary', lambda: [])
    monkeypatch.setattr(app, 'assemble_engine', lambda *_: assembled)

    async def capture_startup(actual, args, config):
        assert actual is assembled
        assert args.serve_renderer is enabled

    monkeypatch.setattr(app, '_serve', capture_startup)
    assert app.main(arguments) == 0
    assert closed == [True]
