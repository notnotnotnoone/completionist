import asyncio

import pytest

from tests.fake_provider import Script, fake_provider
from typer_engine.phrase_provider import PhraseProvider, PhraseRequest, ProviderError, ProviderSettings


def settings(url: str, **overrides) -> ProviderSettings:
    overrides.setdefault("models", ("test-model",))
    return ProviderSettings(base_url=url, timeout=1.0, **overrides)


async def collect(provider: PhraseProvider, request: PhraseRequest):
    return [event async for event in provider.stream(request)]


def run(coro):
    return asyncio.run(coro)


def test_a_completion_streams_text_chunks():
    async def scenario():
        async with fake_provider() as (url, _):
            return await collect(PhraseProvider(settings(url), "key"), PhraseRequest(prompt="Hi"))

    events = run(scenario())
    assert events == ["hello", " world"]


def test_the_request_is_an_openai_style_completion_with_a_bearer_key():
    async def scenario():
        async with fake_provider() as (url, received):
            await collect(PhraseProvider(settings(url, fim=True), "sk-secret"), PhraseRequest(prompt="Hello wor", suffix="ld"))
            return received[0]

    request = run(scenario())
    assert request.path == "/completions"
    assert request.headers["authorization"] == "Bearer sk-secret"
    assert request.body["model"] == "test-model"
    assert request.body["prompt"] == "Hello wor"
    assert request.body["suffix"] == "ld"
    assert request.body["stream"] is True
    assert request.body["max_tokens"] == 40
    assert request.body["stop"] == ["\n"]


def test_without_fim_the_suffix_is_not_sent():
    async def scenario():
        async with fake_provider() as (url, received):
            await collect(PhraseProvider(settings(url, fim=False), "k"), PhraseRequest(prompt="Hi", suffix="there"))
            return received[0].body

    assert "suffix" not in run(scenario())


def test_an_empty_suffix_is_not_sent_either():
    async def scenario():
        async with fake_provider() as (url, received):
            await collect(PhraseProvider(settings(url), "k"), PhraseRequest(prompt="Hi", suffix=""))
            return received[0].body

    assert "suffix" not in run(scenario())



def test_a_reply_without_usage_ends_without_a_usage_event():
    async def scenario():
        async with fake_provider(Script(usage=None)) as (url, _):
            return await collect(PhraseProvider(settings(url), "k"), PhraseRequest(prompt="Hi"))

    assert run(scenario()) == ["hello", " world"]


@pytest.mark.parametrize("status", [401, 429, 500])
def test_http_errors_raise_a_provider_error(status):
    async def scenario():
        async with fake_provider(Script(status=status)) as (url, _):
            await collect(PhraseProvider(settings(url), "k"), PhraseRequest(prompt="Hi"))

    with pytest.raises(ProviderError, match=str(status)):
        run(scenario())


def test_the_api_key_never_appears_in_an_error():
    async def scenario():
        async with fake_provider(Script(status=401)) as (url, _):
            await collect(PhraseProvider(settings(url), "sk-very-secret"), PhraseRequest(prompt="Hi"))

    with pytest.raises(ProviderError) as info:
        run(scenario())
    assert "sk-very-secret" not in str(info.value)


def test_a_server_that_never_answers_times_out():
    async def scenario():
        async with fake_provider(Script(hang=True)) as (url, _):
            await collect(PhraseProvider(settings(url), "k"), PhraseRequest(prompt="Hi"))

    with pytest.raises(ProviderError, match="timed out"):
        run(scenario())


def test_an_unreachable_server_raises_a_provider_error():
    async def scenario():
        await collect(PhraseProvider(settings("http://127.0.0.1:1"), "k"), PhraseRequest(prompt="Hi"))

    with pytest.raises(ProviderError):
        run(scenario())


def test_a_connection_dropped_mid_stream_keeps_what_arrived():
    seen = []

    async def scenario():
        async with fake_provider(Script(chunks=["a", "b", "c"], finish_after=2)) as (url, _):
            async for event in PhraseProvider(settings(url), "k").stream(PhraseRequest(prompt="Hi")):
                seen.append(event)

    try:
        run(scenario())
    except ProviderError:
        pass
    assert seen[:2] == ["a", "b"]


def test_cancelling_the_stream_stops_reading():
    async def scenario():
        async with fake_provider(Script(chunks=["a"] * 50, delay=0.05)) as (url, _):
            provider = PhraseProvider(settings(url), "k")
            got = []

            async def read():
                async for event in provider.stream(PhraseRequest(prompt="Hi")):
                    got.append(event)

            task = asyncio.create_task(read())
            await asyncio.sleep(0.25)
            task.cancel()
            with pytest.raises(asyncio.CancelledError):
                await task
            return got

    got = run(scenario())
    assert 1 <= len(got) < 20


def test_a_failing_model_falls_back_to_the_next_one_in_the_list():
    def script(received):
        return Script(status=500) if received.body["model"] == "bad" else Script()

    async def scenario():
        async with fake_provider(script) as (url, received):
            events = await collect(PhraseProvider(settings(url, models=("bad", "good")), "k"), PhraseRequest(prompt="Hi"))
            return events, [r.body["model"] for r in received]

    events, tried = run(scenario())
    assert events[:2] == ["hello", " world"]
    assert tried == ["bad", "good"]


def test_every_model_failing_raises_a_provider_error():
    async def scenario():
        async with fake_provider(Script(status=500)) as (url, received):
            with pytest.raises(ProviderError):
                await collect(PhraseProvider(settings(url, models=("a", "b")), "k"), PhraseRequest(prompt="Hi"))
            return [r.body["model"] for r in received]

    assert run(scenario()) == ["a", "b"]


def test_the_defaults_point_at_openrouter_without_fim():
    defaults = ProviderSettings()
    assert defaults.base_url == "https://openrouter.ai/api/v1"
    assert defaults.api_key_env == "OPENROUTER_API_KEY"
    assert defaults.fim is False
    assert defaults.models
