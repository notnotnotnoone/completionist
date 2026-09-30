"""Phrases end to end over a real named pipe, with a fake provider."""

import asyncio
import contextlib
from uuid import uuid4

from tests.fake_provider import Script, fake_provider
from completionist_engine.budget import DailyBudget, Prices
from completionist_engine.client import EngineClient
from completionist_engine.config import Config, PhraseConfig
from completionist_engine.engine import Engine
from completionist_engine.phrase_provider import ProviderSettings
from completionist_engine.phrases import PhraseService
from completionist_engine.server import start_server
from completionist_engine.words import WordCompleter

VOCAB = [("world", 5.8), ("work", 6.0), ("worry", 5.0)]


@contextlib.asynccontextmanager
async def serving_phrases(url: str, allow: frozenset[str] = frozenset({"notepad.exe"})):
    phrase = PhraseConfig(provider=ProviderSettings(base_url=url, timeout=1.0), debounce=0.05)
    service = PhraseService(phrase, "key", DailyBudget(0.5, Prices()))
    engine = Engine(WordCompleter(VOCAB), Config(block=frozenset(), allow=allow, phrase=phrase), phrases=service)
    name = rf"\\.\pipe\completionist-test-{uuid4().hex}"
    server = await start_server(engine, name)
    try:
        yield name, service
    finally:
        server.close()
        await service.aclose()


def keystroke(request_id: int, before: str, app: str = "notepad.exe") -> dict:
    return {"id": request_id, "event": "keystroke", "app": app, "before": before}


def test_an_allow_listed_app_gets_a_phrase_pushed_after_a_pause():
    async def scenario():
        async with fake_provider(Script(chunks=["ld is", " big"])) as (url, _), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            reply = await client.request(keystroke(1, "Hello wor"))
            messages = []
            while not messages or not messages[-1].get("done"):
                messages.append(await client.receive(timeout=3.0))
            await client.close()
            return reply, messages

    reply, messages = asyncio.run(scenario())
    assert reply["type"] == "words" and reply["phrase_mode"] == "auto" and "phrase" not in reply
    assert [m["type"] for m in messages] == ["phrase"] * len(messages)
    assert messages[-1] == {"id": 1, "type": "phrase", "text": "ld is big", "done": True}


def test_other_apps_only_get_phrases_on_the_hotkey():
    async def scenario():
        async with fake_provider() as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            reply = await client.request(keystroke(1, "Hello wor", app="discord.exe"))
            await asyncio.sleep(0.3)
            asked_by_typing = len(received)
            await client.send({"id": 1, "event": "hotkey", "app": "discord.exe", "before": "Hello wor"})
            push = await client.receive(timeout=3.0)
            await client.close()
            return reply, asked_by_typing, push

    reply, asked_by_typing, push = asyncio.run(scenario())
    assert reply["phrase_mode"] == "hotkey"
    assert asked_by_typing == 0
    assert push["type"] == "phrase" and push["id"] == 1


def test_a_reply_after_typing_along_already_carries_the_trimmed_phrase():
    async def scenario():
        async with fake_provider(Script(chunks=["ld is big"])) as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            await client.request(keystroke(1, "Hello wor"))
            push = await client.receive(timeout=3.0)
            while not push["done"]:
                push = await client.receive(timeout=3.0)
            assert push["text"] == "ld is big"
            reply = await client.request(keystroke(2, "Hello worl"))
            await client.close()
            return reply, len(received)

    reply, asked = asyncio.run(scenario())
    assert reply["phrase"] == "d is big" and reply["phrase_done"] is True
    assert asked == 1


def test_password_fields_never_reach_the_provider():
    async def scenario():
        async with fake_provider() as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            reply = await client.request({**keystroke(1, "hunter2 is my pass"), "input_scope": ["IS_PASSWORD"]})
            await client.send({"id": 1, "event": "hotkey", "app": "notepad.exe", "before": "secret", "input_scope": ["IS_PASSWORD"]})
            await asyncio.sleep(0.3)
            await client.close()
            return reply, received

    reply, received = asyncio.run(scenario())
    assert received == []
    assert reply["words"] == [] and "phrase_mode" not in reply


def test_disconnecting_cancels_the_running_request():
    async def scenario():
        async with fake_provider(Script(chunks=["a"] * 100, delay=0.05)) as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            await client.request(keystroke(1, "Hello wor"))
            await client.receive(timeout=3.0)
            await client.close()
            await asyncio.sleep(0.3)
            return len(received)

    assert asyncio.run(scenario()) == 1  # and the engine is still healthy enough to finish the test cleanly


def test_a_quiet_request_asks_for_no_phrase_but_still_says_phrases_are_available():
    async def scenario():
        async with fake_provider() as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            reply = await client.request({**keystroke(1, "Hello wor"), "quiet": True})
            await asyncio.sleep(0.3)
            await client.close()
            return reply, len(received)

    reply, asked = asyncio.run(scenario())
    assert reply["phrase_mode"] == "auto"  # so Ctrl+Space still works
    assert asked == 0


def test_the_hotkey_ignores_quiet():
    async def scenario():
        async with fake_provider() as (url, received), serving_phrases(url) as (name, _service):
            client = await EngineClient.connect(name)
            await client.request({**keystroke(1, "Hello wor"), "quiet": True})
            await client.send({"id": 1, "event": "hotkey", "app": "notepad.exe", "before": "Hello wor"})
            push = await client.receive(timeout=3.0)
            await client.close()
            return push, len(received)

    push, asked = asyncio.run(scenario())
    assert push["type"] == "phrase" and asked == 1
