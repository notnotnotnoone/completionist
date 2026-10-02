import asyncio
import json

from completionist_engine.config import Config
from completionist_engine.engine import Engine
from completionist_engine.viewer import ViewerServer
from completionist_engine.words import WordCompleter
from tests.test_viewer import api, http


def test_runtime_endpoints_are_authenticated_and_private_mode_persists(tmp_path):
    async def scenario():
        path = tmp_path / "config.toml"
        engine = Engine(WordCompleter([("world", 5)]), Config())
        server = ViewerServer(None, None, path, token="test-token-abc", engine=engine)
        await server.start()
        try:
            assert (await http(server, "GET", "/api/runtime"))[0] == 403
            assert (await api(server, "POST", "/api/runtime", {"action": "pause", "minutes": -1}))[0] == 400
            assert (await api(server, "POST", "/api/runtime", {"action": "pause", "minutes": 5}))[0] == 200
            assert engine.paused
            await api(server, "POST", "/api/runtime", {"action": "resume"})
            assert not engine.paused
            await api(server, "POST", "/api/runtime", {"action": "private_on"})
            assert engine.private_mode and "private_mode = true" in path.read_text()
            status, _, body = await api(server, "GET", "/api/context")
            assert status == 200 and json.loads(body)["preview"] is None
            assert (await api(server, "POST", "/api/context", {"action": "send", "id": 999}))[0] == 400
        finally:
            server.close()
    asyncio.run(scenario())
