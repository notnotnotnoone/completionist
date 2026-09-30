import asyncio

import pytest

from tests.fake_provider import Script, fake_provider
from typer_engine.config import PhraseConfig
from typer_engine.context import INSTRUCTIONS
from typer_engine.phrase_provider import ProviderSettings
from typer_engine.phrases import PhraseService
from typer_engine.protocol import PhraseUpdate, Request


def req(request_id: int, before: str, after: str = "", event: str = "keystroke", title: str = "Notes") -> Request:
    return Request(id=request_id, event=event, app="notepad.exe", title=title, before=before, after=after)


def make_service(url: str, *, key: str | None = "k", **overrides) -> PhraseService:
    config = PhraseConfig(
        provider=ProviderSettings(base_url=url, models=("m",), timeout=1.0, fim=True),
        debounce=overrides.pop("debounce", 0.05),
        context_before=overrides.pop("context_before", 8000),
    )
    return PhraseService(config, key)


async def settle(pushes: list[PhraseUpdate], until=lambda p: p and p[-1].done, timeout=3.0):
    end = asyncio.get_running_loop().time() + timeout
    while asyncio.get_running_loop().time() < end:
        if until(pushes):
            return
        await asyncio.sleep(0.01)


def test_the_hotkey_streams_a_phrase_to_the_text_service():
    async def scenario():
        async with fake_provider(Script(chunks=["ld is", " big"])) as (url, _):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(5, "Hello wor", event="hotkey"), "hotkey")
            await settle(pushes)
            session.close()
            await service.aclose()
            return pushes

    pushes = asyncio.run(scenario())
    assert [p.text for p in pushes] == ["ld is", "ld is big", "ld is big"]
    assert [p.done for p in pushes] == [False, False, True]
    assert {p.id for p in pushes} == {5}



def test_in_auto_mode_a_pause_starts_the_request():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url, debounce=0.05)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            first = session.on_request(req(1, "Hello wor"), "auto")
            assert first.phrase == ""
            await asyncio.sleep(0.01)
            assert received == []  # still inside the pause
            await settle(pushes)
            session.close()
            return received, pushes

    received, pushes = asyncio.run(scenario())
    assert len(received) == 1
    assert pushes[-1].text == "hello world"


def test_typing_keeps_postponing_the_request():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url, debounce=0.1)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            for i, text in enumerate(["Hello w", "Hello wo", "Hello wor"], start=1):
                session.on_request(req(i, text), "auto")
                await asyncio.sleep(0.05)
            assert received == []
            await settle(pushes)
            session.close()
            return received

    received = asyncio.run(scenario())
    assert len(received) == 1
    assert received[0].body["prompt"].endswith("Hello wor")  # asked about the latest text only


def test_hotkey_mode_does_not_ask_on_its_own():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url, debounce=0.02)
            session = service.open_session(lambda _u: None)
            session.on_request(req(1, "Hello wor"), "hotkey")
            await asyncio.sleep(0.15)
            session.close()
            return received

    assert asyncio.run(scenario()) == []


def test_the_prompt_has_a_header_the_anchored_window_and_the_text_after_the_caret():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Dear Sam, thanks for th", after=" meeting.", event="hotkey", title="Inbox"), "hotkey")
            await settle(pushes)
            session.close()
            return received[0].body

    body = asyncio.run(scenario())
    header = next(line for line in body["prompt"].splitlines() if line.startswith("[Text typed in"))
    assert "Inbox" in header and "notepad" in header
    assert body["prompt"].endswith("Dear Sam, thanks for th")
    assert body["suffix"] == " meeting."


def test_a_long_text_is_windowed_to_the_configured_cap():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url, context_before=300)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "word " * 1000 + "end", event="hotkey"), "hotkey")
            await settle(pushes)
            session.close()
            return received[0].body["prompt"]

    assert len(asyncio.run(scenario())) < 450 + len(INSTRUCTIONS)  # the window, a short header, and the fixed instructions


def test_typing_along_trims_the_phrase_and_later_pushes_use_the_newest_request_id():
    async def scenario():
        async with fake_provider(Script(chunks=["ld", " is", " big"], delay=0.08)) as (url, received):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await settle(pushes, until=lambda p: bool(p))  # "ld" has arrived
            update = session.on_request(req(2, "Hello worl"), "auto")
            assert update.phrase == "d" and update.done is False
            await settle(pushes)
            session.close()
            return pushes, received

    pushes, received = asyncio.run(scenario())
    assert len(received) == 1  # no second request
    assert pushes[-1].id == 2 and pushes[-1].text == "d is big" and pushes[-1].done


def test_typing_something_else_cancels_the_request_and_stops_the_pushes():
    async def scenario():
        async with fake_provider(Script(chunks=["a"] * 40, delay=0.05)) as (url, _):
            service = make_service(url, debounce=5.0)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await asyncio.sleep(0.15)
            session.on_request(req(2, "Goodbye"), "auto")  # not what the phrase continues from
            count = len(pushes)
            await asyncio.sleep(0.3)
            session.close()
            return count, len(pushes)

    before, after = asyncio.run(scenario())
    assert after == before  # nothing more was pushed for the abandoned request


def test_a_provider_error_ends_quietly_with_no_phrase():
    async def scenario():
        async with fake_provider(Script(status=500)) as (url, _):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await settle(pushes)
            session.close()
            return pushes

    pushes = asyncio.run(scenario())
    assert pushes[-1].text == "" and pushes[-1].done


def test_repeated_failures_pause_requests_for_a_while():
    async def scenario():
        async with fake_provider(Script(status=500)) as (url, received):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            for i in range(1, 6):
                session.on_hotkey(req(i, f"Text number {i}", event="hotkey"), "hotkey")
                if i <= 3:
                    await settle(pushes, until=lambda p, i=i: bool(p) and p[-1].done and p[-1].id == i)
                else:
                    await asyncio.sleep(0.1)  # paused: nothing is sent, so nothing comes back
            session.close()
            return len(received)

    assert asyncio.run(scenario()) == 3  # three failures, then it stops hammering the provider



def test_without_an_api_key_phrases_are_unavailable():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url, key=None)
            assert not service.available
            session = service.open_session(lambda _u: None)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            session.on_request(req(2, "Hello wor"), "auto")
            await asyncio.sleep(0.15)
            session.close()
            return received

    assert asyncio.run(scenario()) == []


def test_dismissing_cancels_the_request():
    async def scenario():
        async with fake_provider(Script(chunks=["a"] * 40, delay=0.05)) as (url, _):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await asyncio.sleep(0.12)
            session.on_dismiss()
            count = len(pushes)
            await asyncio.sleep(0.25)
            session.close()
            return count, len(pushes)

    before, after = asyncio.run(scenario())
    assert after == before


def test_sessions_are_independent():
    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url)
            a: list[PhraseUpdate] = []
            b: list[PhraseUpdate] = []
            sa, sb = service.open_session(a.append), service.open_session(b.append)
            sa.on_hotkey(req(1, "First app text", event="hotkey"), "hotkey")
            await settle(a)
            sa.close()
            sb.close()
            return a, b

    a, b = asyncio.run(scenario())
    assert a and not b


def test_reloading_the_config_switches_provider_without_a_restart(monkeypatch):
    from dataclasses import replace

    async def scenario():
        async with fake_provider(Script(chunks=["one"])) as (url_a, received_a), fake_provider(Script(chunks=["two"])) as (url_b, received_b):
            service = make_service(url_a)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await settle(pushes)
            new_config = replace(service.config, provider=replace(service.config.provider, base_url=url_b))
            service.reconfigure(new_config)
            session.on_hotkey(req(2, "Another sentence th", event="hotkey"), "hotkey")
            await settle(pushes, until=lambda p: bool(p) and p[-1].id == 2 and p[-1].done)
            session.close()
            await service.aclose()
            return len(received_a), len(received_b), pushes[-1].text

    a, b, text = asyncio.run(scenario())
    assert (a, b, text) == (1, 1, "two")


def test_reloading_can_switch_phrases_off_and_on():
    from dataclasses import replace

    async def scenario():
        async with fake_provider() as (url, received):
            service = make_service(url)
            session = service.open_session(lambda _u: None)
            service.reconfigure(replace(service.config, enabled=False))
            assert not service.available
            session.on_hotkey(req(1, "Hello wor", event="hotkey"), "hotkey")
            await asyncio.sleep(0.1)
            assert received == []
            service.reconfigure(replace(service.config, enabled=True))
            assert service.available
            session.on_hotkey(req(2, "Hello wor", event="hotkey"), "hotkey")
            await asyncio.sleep(0.3)
            session.close()
            return len(received)

    assert asyncio.run(scenario()) == 1



def test_naming_another_key_variable_picks_up_that_variable(monkeypatch):
    from dataclasses import replace

    monkeypatch.setenv("KEY_TWO", "second")
    config = PhraseConfig(provider=ProviderSettings(api_key_env="KEY_ONE"))
    monkeypatch.delenv("KEY_ONE", raising=False)
    service = PhraseService(config, None, key_from_env=True)
    assert not service.available
    service.reconfigure(replace(config, provider=replace(config.provider, api_key_env="KEY_TWO")))
    assert service.available


def make_prefix_only_service(url: str) -> PhraseService:
    config = PhraseConfig(provider=ProviderSettings(base_url=url, models=("m",), timeout=1.0, fim=False), debounce=0.05)
    return PhraseService(config, "k")


def test_without_fim_text_after_the_caret_on_the_same_line_holds_the_phrase_back():
    async def scenario():
        async with fake_provider(Script(chunks=["ld"])) as (url, received):
            service = make_prefix_only_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            update = session.on_hotkey(req(1, "Hello wor", after=" and more text", event="hotkey"), "hotkey")
            await asyncio.sleep(0.2)
            session.close()
            await service.aclose()
            return update, pushes, received

    update, pushes, received = asyncio.run(scenario())
    assert update.phrase == ""
    assert pushes == []
    assert received == []


def test_without_fim_text_on_a_later_line_does_not_hold_the_phrase_back():
    async def scenario():
        async with fake_provider(Script(chunks=["ld"])) as (url, _):
            service = make_prefix_only_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", after="\nNext line here", event="hotkey"), "hotkey")
            await settle(pushes)
            session.close()
            await service.aclose()
            return pushes

    assert asyncio.run(scenario())[-1].text == "ld"


def test_with_fim_text_after_the_caret_is_sent_and_does_not_hold_the_phrase_back():
    async def scenario():
        async with fake_provider(Script(chunks=["ld"])) as (url, received):
            service = make_service(url)
            pushes: list[PhraseUpdate] = []
            session = service.open_session(pushes.append)
            session.on_hotkey(req(1, "Hello wor", after=" and more text", event="hotkey"), "hotkey")
            await settle(pushes)
            session.close()
            await service.aclose()
            return pushes, received

    pushes, received = asyncio.run(scenario())
    assert pushes[-1].text == "ld"
    assert received[0].body["suffix"] == " and more text"
