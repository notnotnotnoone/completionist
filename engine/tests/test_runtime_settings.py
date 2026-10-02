import asyncio
from dataclasses import replace

import pytest

from completionist_engine.config import Config, PhraseConfig
from completionist_engine.engine import Engine
from completionist_engine.fuzzy import FuzzyIndex
from completionist_engine.personal import PersonalStore
from completionist_engine.phrase_provider import ProviderSettings
from completionist_engine.phrases import PhraseService
from completionist_engine.protocol import Request
from completionist_engine.request_log import RequestLog
from completionist_engine.words import WordCompleter
from tests.fake_provider import Script, fake_provider


async def wait_for(predicate, timeout=3):
    deadline = asyncio.get_running_loop().time() + timeout
    while not predicate():
        if asyncio.get_running_loop().time() >= deadline:
            raise AssertionError("Expected request state did not arrive")
        await asyncio.sleep(0.01)


def request(before="Hello wor", **kwargs):
    return Request(id=1, event="keystroke", app="notepad.exe", before=before, **kwargs)


def test_private_mode_keeps_local_words_but_never_learns():
    store = PersonalStore()
    engine = Engine(WordCompleter([("world", 5)]), Config(private_mode=True), personal=store)
    session = engine.open_session()
    for i in range(1, 6):
        session.handle(request("name "[:i]))
    assert store.counts((), "").total == 0
    assert session.handle(request()).words == ("world",)


def test_timed_pause_resumes_and_private_mode_is_independent():
    now = [100.0]
    engine = Engine(WordCompleter([("world", 5)]), Config(private_mode=True), clock=lambda: now[0])
    engine.set_paused(True, minutes=1)
    assert engine.handle(request()).words == ()
    now[0] += 61
    assert not engine.paused
    assert engine.handle(request()).words == ("world",)
    assert engine.private_mode


def test_typo_correction_can_be_disabled_without_losing_exact_matches():
    words = WordCompleter([("world", 5)], fuzzy=FuzzyIndex({"world"}))
    assert words.complete("wrold").words == ("world",)
    assert words.complete("wrold", typo_correction=False).words == ()
    assert words.complete("wor", typo_correction=False).words == ("world",)


def test_request_log_switching_to_timings_scrubs_old_text_and_captures():
    log = RequestLog()
    entry = log.add(app="notes", prompt="secret", reply="reply", outcome="ok", attempts=[], ttft_ms=10, total_ms=20)
    log.configure("timings", 0)
    assert log.get(entry.id).prompt == log.get(entry.id).reply == ""
    log.configure("off", 0)
    log.add(app="notes", prompt="new", reply="reply", outcome="ok", attempts=[], ttft_ms=10, total_ms=20)
    assert log.recent()[1] == 0


def test_request_log_retention_expires_without_new_requests():
    now = [100.0]
    log = RequestLog(clock=lambda: now[0])
    log.configure("full", 1)
    item = log.add(app="notes", prompt="secret", reply="reply", outcome="ok", attempts=[], ttft_ms=None, total_ms=20)
    now[0] += 61
    assert log.get(item.id) is None
    assert log.recent()[1] == 0


@pytest.mark.parametrize("options,before", [({"min_chars": 30}, "Hello"), ({"trigger": "sentence"}, "Hello"), ({"preview_context": True}, "Hello.")])
def test_automatic_requests_respect_timing_and_preview_gates(options, before):
    async def scenario():
        async with fake_provider() as (url, received):
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k"), debounce=0.01, **options))
            session = service.open_session(lambda update: None)
            session.on_request(request(before), "auto")
            await asyncio.sleep(0.07)
            assert received == []
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_dismiss_cooldown_delays_automatic_requests_but_hotkey_bypasses_it():
    async def scenario():
        async with fake_provider() as (url, received):
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k"), debounce=0.01, dismiss_cooldown=10))
            session = service.open_session(lambda update: None)
            session.on_dismiss()
            session.on_request(request(), "auto")
            await asyncio.sleep(0.04)
            assert not received
            session.on_hotkey(request(), "hotkey")
            await wait_for(lambda: received)
            assert len(received) == 1
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_manual_preview_sends_nothing_until_approved_and_typing_invalidates_it():
    async def scenario():
        async with fake_provider() as (url, received):
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k"), preview_context=True))
            session = service.open_session(lambda update: None)
            session.on_hotkey(request(), "hotkey")
            await asyncio.sleep(0.04)
            preview = service.context_preview()
            assert "Hello wor" in preview["prompt"] and not received
            assert service.send_preview(preview["id"])
            await wait_for(lambda: received)
            assert received[0].body["messages"][0]["content"] in preview["prompt"]
            assert received[0].body["messages"][1]["content"] in preview["prompt"]
            session.on_dismiss()
            session.on_hotkey(request("Different"), "hotkey")
            await asyncio.sleep(0.02)
            pending = service.context_preview()
            session.on_request(request("Different now"), "hotkey")
            assert service.context_preview() is None
            assert not service.send_preview(pending["id"])
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_writing_preferences_reach_prompt_and_excluded_phrases_never_stream():
    async def scenario():
        async with fake_provider(Script(chunks=["banned", " phrase"])) as (url, received):
            pushes = []
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k"), spelling="canadian", writing_style="professional", avoid_phrases=("banned phrase",)))
            session = service.open_session(pushes.append)
            session.on_hotkey(request(), "hotkey")
            await wait_for(lambda: service.log.recent()[1])
            assert "Canadian" in received[0].body["messages"][0]["content"]
            assert "professional" in received[0].body["messages"][0]["content"]
            assert all("banned" not in push.text for push in pushes)
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_failure_backoff_is_configurable():
    now = [100.0]
    service = PhraseService(PhraseConfig(provider=ProviderSettings(api_key="k"), failure_limit=1, failure_pause=12), clock=lambda: now[0])
    service.note_failure()
    assert not service.usable()
    now[0] += 13
    assert service.usable()


@pytest.mark.parametrize("options,chunks,expected", [
    ({"multiline": True}, ["first\n", "second"], "first\nsecond"),
    ({"completion_length": "sentence"}, ["One sentence. ", "Another."], "One sentence."),
    ({"completion_length": "short"}, ["one two three four five six seven eight nine"], "one two three four five six seven eight"),
])
def test_output_preferences_change_the_sent_request_and_visible_continuation(options, chunks, expected):
    async def scenario():
        async with fake_provider(Script(chunks=chunks)) as (url, received):
            pushes = []
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k"), **options))
            session = service.open_session(pushes.append)
            session.on_hotkey(request(), "hotkey")
            await wait_for(lambda: pushes and pushes[-1].done)
            assert pushes[-1].text == expected
            if options.get("multiline"):
                assert received[0].body["stop"] == []
            if options.get("completion_length") == "short":
                assert received[0].body["max_tokens"] <= 24
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_private_mode_cancels_streaming_and_never_repopulates_the_log():
    async def scenario():
        async with fake_provider(Script(chunks=["secret", " continuation"], delay=0.05)) as (url, received):
            service = PhraseService(PhraseConfig(provider=ProviderSettings(base_url=url, api_key="k")))
            engine = Engine(WordCompleter([("world", 5)]), Config(phrase=service.config), phrases=service)
            pushes = []
            session = engine.open_session(pushes.append)
            session.handle(replace(request(), event="hotkey"))
            await wait_for(lambda: received)
            engine.set_private_mode(True)
            engine.set_private_mode(False)
            await asyncio.sleep(0.15)
            assert service.log.recent()[1] == 0
            assert all(not push.text for push in pushes)
            session.close()
            await service.aclose()
    asyncio.run(scenario())


def test_app_profiles_control_phrase_mode_and_word_limit_but_never_override_protected_fields():
    from completionist_engine.config import AppProfile
    from completionist_engine.policy import decide
    config = Config(app_profiles=(AppProfile("notepad.exe", phrase_mode="hotkey", word_limit=1),))
    engine = Engine(WordCompleter([("world", 5), ("work", 4)]), config)
    assert len(engine.handle(request()).words) == 1
    assert decide("notepad.exe", (), config).phrase == "hotkey"
    assert not decide("notepad.exe", ("IS_PASSWORD",), config).words


def test_window_context_app_permissions_are_checked_before_capture():
    from completionist_engine.app import screen_wanted
    config = Config(phrase=PhraseConfig(context_apps=frozenset({"notepad.exe"})))
    service = PhraseService(replace(config.phrase, provider=ProviderSettings(api_key="k")))
    engine = Engine(WordCompleter([]), config, phrases=service)
    assert screen_wanted(engine, service)("notepad.exe")
    assert not screen_wanted(engine, service)("chrome.exe")
    engine.set_private_mode(True)
    assert not screen_wanted(engine, service)("notepad.exe")
