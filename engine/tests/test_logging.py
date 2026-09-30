import asyncio
import logging
from uuid import uuid4

from completionist_engine.client import EngineClient
from completionist_engine.logsetup import setup_logging
from completionist_engine.server import SLOW_REQUEST_SECONDS, start_server


def test_logs_go_to_a_file_in_the_data_folder(tmp_path):
    setup_logging("INFO", tmp_path)
    logging.getLogger("completionist_engine.test").info("hello from the engine")
    for handler in logging.getLogger().handlers:
        handler.flush()
    assert "hello from the engine" in (tmp_path / "engine.log").read_text(encoding="utf-8")
    teardown()


def test_the_log_file_rotates_instead_of_growing_forever(tmp_path):
    setup_logging("INFO", tmp_path, max_bytes=2000, backups=2)
    logger = logging.getLogger("completionist_engine.test")
    for i in range(200):
        logger.info("line %d %s", i, "x" * 50)
    for handler in logging.getLogger().handlers:
        handler.flush()
    assert (tmp_path / "engine.log.1").exists()
    assert not (tmp_path / "engine.log.3").exists()
    teardown()


def test_calling_setup_twice_does_not_duplicate_lines(tmp_path):
    setup_logging("INFO", tmp_path)
    setup_logging("INFO", tmp_path)
    logging.getLogger("completionist_engine.test").info("only once")
    for handler in logging.getLogger().handlers:
        handler.flush()
    assert (tmp_path / "engine.log").read_text(encoding="utf-8").count("only once") == 1
    teardown()


def test_an_unwritable_log_folder_falls_back_to_the_console(tmp_path):
    blocker = tmp_path / "file"
    blocker.write_text("x")
    setup_logging("INFO", blocker / "sub")  # can't be created: a file is in the way
    logging.getLogger("completionist_engine.test").info("still works")
    teardown()


class SlowEngine:
    def open_session(self, push=None):
        return SlowSession()


class SlowSession:
    def handle(self, request):
        import time

        time.sleep(SLOW_REQUEST_SECONDS * 2)
        return None

    def close(self):
        pass


def test_a_slow_request_is_logged_with_its_time(caplog):
    async def scenario():
        name = rf"\\.\pipe\completionist-test-{uuid4().hex}"
        server = await start_server(SlowEngine(), name)
        try:
            client = await EngineClient.connect(name)
            await client.send({"id": 1, "event": "dismiss", "app": "x.exe"})
            await asyncio.sleep(0.3)
            await client.close()
        finally:
            server.close()

    with caplog.at_level(logging.WARNING, logger="completionist_engine.server"):
        asyncio.run(scenario())
    assert any("slow request" in r.message and "dismiss" in r.message for r in caplog.records)


def teardown():
    root = logging.getLogger()
    for handler in list(root.handlers):
        if getattr(handler, "_completionist", False):
            handler.close()
            root.removeHandler(handler)
