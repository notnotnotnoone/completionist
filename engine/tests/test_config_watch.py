import asyncio
import os

from completionist_engine.config import Config
from completionist_engine.config_watch import ConfigWatcher


def write(path, text):
    path.write_text(text, encoding="utf-8")
    stat = path.stat()
    os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns + 5_000_000_000))  # make the change visible to mtime checks


def watcher(tmp_path, initial=None):
    applied = []
    path = tmp_path / "config.toml"
    if initial is not None:
        path.write_text(initial, encoding="utf-8")
    w = ConfigWatcher(path, Config(), applied.append)
    return w, path, applied


def test_nothing_happens_while_the_file_is_unchanged(tmp_path):
    w, path, applied = watcher(tmp_path, "[words]\nlimit = 3\n")
    assert w.check() is False and applied == []


def test_a_changed_file_is_reloaded_and_applied(tmp_path):
    w, path, applied = watcher(tmp_path, "[words]\nlimit = 3\n")
    write(path, "[words]\nlimit = 7\n")
    assert w.check() is True
    assert applied[-1].word_limit == 7
    assert w.check() is False  # and only once


def test_a_file_created_after_start_is_picked_up(tmp_path):
    w, path, applied = watcher(tmp_path)
    write(path, "[words]\nlimit = 2\n")
    assert w.check() and applied[-1].word_limit == 2


def test_an_invalid_change_keeps_the_running_config_and_reports_the_error(tmp_path):
    w, path, applied = watcher(tmp_path, "[words]\nlimit = 3\n")
    write(path, "[words]\nlimit = 'lots'\n")
    assert w.check() is False
    assert applied == []
    assert "words.limit" in w.last_error
    write(path, "[words]\nlimit = 4\n")  # fixing it works, and clears the error
    assert w.check() is True and applied[-1].word_limit == 4 and w.last_error is None


def test_a_broken_edit_is_not_retried_until_the_file_changes_again(tmp_path):
    w, path, applied = watcher(tmp_path, "")
    write(path, "not toml = = =")
    w.check()
    first = w.last_error
    assert w.check() is False and w.last_error == first


def test_deleting_the_file_goes_back_to_defaults(tmp_path):
    w, path, applied = watcher(tmp_path, "[words]\nlimit = 3\n")
    path.unlink()
    assert w.check() is True
    assert applied[-1] == Config()


def test_a_moved_data_folder_is_flagged_as_needing_a_restart(tmp_path):
    w, path, applied = watcher(tmp_path, "")
    write(path, f"[data]\ndir = '{(tmp_path / 'elsewhere').as_posix()}'\n")
    w.check()
    assert w.restart_needed is True
    assert applied[-1].data_dir == Config().data_dir  # the running data folder is kept


def test_the_background_task_applies_changes(tmp_path):
    async def scenario():
        w, path, applied = watcher(tmp_path, "[words]\nlimit = 3\n")
        task = asyncio.create_task(w.run(interval=0.02))
        await asyncio.sleep(0.05)
        write(path, "[words]\nlimit = 9\n")
        for _ in range(100):
            if applied:
                break
            await asyncio.sleep(0.02)
        task.cancel()
        return applied

    applied = asyncio.run(scenario())
    assert applied and applied[-1].word_limit == 9
