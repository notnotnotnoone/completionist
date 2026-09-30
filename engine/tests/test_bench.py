import asyncio
import json

from tests.fake_provider import Script, fake_provider
from typer_engine.bench import BenchProvider, Sample, format_report, load_providers, main, run_bench


def provider(url: str, name: str = "fake", key_env: str = "TYPER_TEST_KEY", **kwargs) -> BenchProvider:
    return BenchProvider(name=name, base_url=url, model="m", api_key_env=key_env, **kwargs)


SAMPLES = [Sample("chat", "hey are you free to grab", " later?"), Sample("email", "Thank you for your", "")]


def test_each_provider_is_timed_on_each_sample(monkeypatch):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def scenario():
        async with fake_provider(Script(chunks=["a", "b", "c"], delay=0.02)) as (url, received):
            results = await run_bench([provider(url)], SAMPLES, context_sizes=[])
            return results, received

    results, received = asyncio.run(scenario())
    assert len(results) == 2 and len(received) == 2
    r = results[0]
    assert r.provider == "fake" and r.sample == "chat" and r.text == "abc"
    assert 0.015 < r.first_token < r.total < 2.0
    assert r.cost > 0
    assert r.error is None


def test_a_provider_without_a_key_is_skipped_and_reported(monkeypatch):
    monkeypatch.delenv("TYPER_TEST_KEY", raising=False)

    async def scenario():
        async with fake_provider() as (url, received):
            return await run_bench([provider(url)], SAMPLES, context_sizes=[]), received

    results, received = asyncio.run(scenario())
    assert received == []
    assert all(r.error == "no API key (set TYPER_TEST_KEY)" for r in results)


def test_a_failing_provider_is_recorded_and_the_others_still_run(monkeypatch):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def scenario():
        async with fake_provider(Script(status=500)) as (bad, _), fake_provider() as (good, _):
            return await run_bench([provider(bad, "bad"), provider(good, "good")], SAMPLES[:1], context_sizes=[])

    results = asyncio.run(scenario())
    assert [r.provider for r in results] == ["bad", "good"]
    assert "500" in results[0].error and results[1].error is None


def test_a_context_sweep_sends_prompts_of_growing_size(monkeypatch):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def scenario():
        async with fake_provider() as (url, received):
            results = await run_bench([provider(url)], [], context_sizes=[200, 2000])
            return results, [len(r.body["prompt"]) for r in received]

    results, sizes = asyncio.run(scenario())
    assert [r.sample for r in results] == ["context 200 chars", "context 2000 chars"]
    assert sizes[0] < sizes[1] and 200 <= sizes[0] < 400 and sizes[1] >= 2000


def test_a_clean_completion_is_flagged_and_a_chatty_one_is_not(monkeypatch):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def scenario(chunks):
        async with fake_provider(Script(chunks=chunks)) as (url, _):
            return (await run_bench([provider(url)], SAMPLES[:1], context_sizes=[]))[0]

    assert asyncio.run(scenario([" tomorrow", " evening?"])).clean is True
    assert asyncio.run(scenario(["Sure! Here is a continuation:", " tomorrow"])).clean is False
    assert asyncio.run(scenario([""])).clean is False  # nothing came back


def test_the_report_summarises_each_provider(monkeypatch):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def scenario():
        async with fake_provider(Script(chunks=["a", "b"], delay=0.01)) as (url, _):
            return await run_bench([provider(url, "alpha")], SAMPLES, context_sizes=[100])

    report = format_report(asyncio.run(scenario()))
    assert "alpha" in report and "first token" in report and "$/1000" in report
    assert "context 100 chars" in report


def test_providers_load_from_a_toml_file(tmp_path):
    path = tmp_path / "providers.toml"
    path.write_text(
        '[[provider]]\nname = "ds"\nbase_url = "https://api.deepseek.com/beta"\nmodel = "deepseek-chat"\napi_key_env = "DEEPSEEK_API_KEY"\n\n'
        '[[provider]]\nname = "mini"\nbase_url = "https://x/v1"\nmodel = "m"\napi_key_env = "K"\nfim = false\n',
        encoding="utf-8",
    )
    loaded = load_providers(path)
    assert [p.name for p in loaded] == ["ds", "mini"]
    assert loaded[0].fim is True and loaded[1].fim is False


def test_the_command_line_runs_and_can_write_json(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("TYPER_TEST_KEY", "secret")

    async def serve_and_run():
        async with fake_provider() as (url, _):
            path = tmp_path / "p.toml"
            path.write_text(f'[[provider]]\nname = "f"\nbase_url = "{url}"\nmodel = "m"\napi_key_env = "TYPER_TEST_KEY"\n', encoding="utf-8")
            out = tmp_path / "out.json"
            code = await asyncio.to_thread(main, [str(path), "--json", str(out), "--contexts", "150"])
            return code, out

    code, out = asyncio.run(serve_and_run())
    assert code == 0
    data = json.loads(out.read_text())
    assert data and {"provider", "sample", "first_token", "total", "text"} <= set(data[0])
    assert "f" in capsys.readouterr().out
