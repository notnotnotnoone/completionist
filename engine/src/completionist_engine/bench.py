"""`completionist-bench`: compare phrase providers on speed, cost and cleanliness before choosing a default.

    completionist-bench providers.toml [--json results.json] [--contexts 500 2000 8000]

`providers.toml` lists candidates; each names the environment variable holding its API key:

    [[provider]]
    name = "deepseek-fim"
    base_url = "https://api.deepseek.com/beta"
    model = "deepseek-chat"
    api_key_env = "DEEPSEEK_API_KEY"
    fim = true                      # optional, default true
    price_input_per_m = 0.30        # optional, dollars per million tokens
    price_cached_per_m = 0.006
    price_output_per_m = 1.20

Each provider completes a set of short samples, then the same request with growing amounts of context,
so the default context cap can be chosen by measurement. Providers without a key are skipped.
"""

import argparse
import asyncio
import json
import os
import statistics
import time
import tomllib
from dataclasses import asdict, dataclass
from pathlib import Path

from completionist_engine.budget import Prices, Usage, cost
from completionist_engine.context import build_prompt
from completionist_engine.phrase_provider import PhraseProvider, PhraseRequest, ProviderError, ProviderSettings


@dataclass(frozen=True)
class BenchProvider:
    name: str
    base_url: str
    model: str
    api_key_env: str
    fim: bool = True
    max_tokens: int = 40
    price_input_per_m: float = 0.30
    price_cached_per_m: float = 0.006
    price_output_per_m: float = 1.20


@dataclass(frozen=True)
class Sample:
    name: str
    before: str
    after: str = ""


@dataclass(frozen=True)
class Result:
    provider: str
    sample: str
    first_token: float | None
    total: float | None
    text: str
    cost: float
    clean: bool
    error: str | None = None


SAMPLES = [
    Sample("chat", "hey are you free to grab lunch tomorrow or is it too"),
    Sample("chat-mid", "did you see the game last night, that fourth quarter was ", " honestly"),
    Sample("email", "Hi Sam,\n\nThank you for sending over the proposal. I had a chance to review it and"),
    Sample("email-mid", "Thanks again for your help with the move. I'll bring the ", " on Saturday."),
    Sample("docs", "The scheduler wakes each worker after a short delay. If the queue is empty, it"),
    Sample("notes", "Meeting notes: agreed to ship the beta on Friday. Open questions:"),
    Sample("story", "The rain had not stopped for three days, and the old bridge"),
    Sample("reply", "Sounds good to me! I can do Thursday afternoon, but"),
]

_FILLER = (
    "We spent the morning going through the quarterly numbers and talking about what to do next. "
    "The team agreed that the new onboarding flow needs another round of testing before launch. "
    "Priya is going to draft the announcement while Marcus checks the remaining edge cases. "
)
_CHATTY = ("sure", "here is", "here's", "certainly", "of course", "```", '"', "continuation", "the completed")


def _clean(text: str) -> bool:
    """A usable continuation: something came back, on one line, without an assistant-style preamble."""
    stripped = text.strip().lower()
    return bool(stripped) and "\n" not in text and not stripped.startswith(_CHATTY)


def _context_sample(chars: int) -> Sample:
    body = (_FILLER * (chars // len(_FILLER) + 1))[:chars]
    return Sample(f"context {chars} chars", body + " And so the plan for next week is to")


async def _time_one(provider: BenchProvider, api_key: str, sample: Sample) -> Result:
    settings = ProviderSettings(
        base_url=provider.base_url, model=provider.model, api_key_env=provider.api_key_env, fim=provider.fim, max_tokens=provider.max_tokens, timeout=15.0
    )
    client = PhraseProvider(settings, api_key)
    prices = Prices(provider.price_input_per_m, provider.price_cached_per_m, provider.price_output_per_m)
    request = PhraseRequest(prompt=build_prompt("bench.exe", "benchmark", sample.before), suffix=sample.after)
    started = time.perf_counter()
    first: float | None = None
    text = ""
    usage = Usage()
    try:
        async for event in client.stream(request):
            if isinstance(event, Usage):
                usage = event
            else:
                if first is None:
                    first = time.perf_counter() - started
                text += event
        return Result(provider.name, sample.name, first, time.perf_counter() - started, text, cost(usage, prices), _clean(text))
    except ProviderError as err:
        return Result(provider.name, sample.name, first, None, text, 0.0, False, str(err))
    finally:
        await client.aclose()


async def run_bench(providers: list[BenchProvider], samples: list[Sample], context_sizes: list[int]) -> list[Result]:
    everything = [*samples, *(_context_sample(n) for n in context_sizes)]
    results: list[Result] = []
    for provider in providers:
        key = os.environ.get(provider.api_key_env)
        for sample in everything:
            if not key:
                results.append(Result(provider.name, sample.name, None, None, "", 0.0, False, f"no API key (set {provider.api_key_env})"))
            else:
                results.append(await _time_one(provider, key, sample))
    return results


def format_report(results: list[Result]) -> str:
    lines = [f"{'provider':<18}{'sample':<24}{'first token':>12}{'total':>9}{'cost':>10}  completion"]
    for r in results:
        if r.error:
            lines.append(f"{r.provider:<18}{r.sample:<24}{'-':>12}{'-':>9}{'-':>10}  ERROR {r.error}")
            continue
        first = f"{r.first_token * 1000:.0f} ms" if r.first_token is not None else "-"
        total = f"{r.total * 1000:.0f} ms" if r.total is not None else "-"
        flag = "" if r.clean else " (not clean)"
        lines.append(f"{r.provider:<18}{r.sample:<24}{first:>12}{total:>9}{r.cost * 1000:>9.4f}m  {r.text.strip()[:60]!r}{flag}")
    lines += ["", "summary (successful requests)"]
    lines.append(f"{'provider':<18}{'first token p50':>16}{'p95':>9}{'clean':>8}{'$/1000 requests':>18}")
    for name in dict.fromkeys(r.provider for r in results):
        ok = [r for r in results if r.provider == name and not r.error and r.first_token is not None]
        if not ok:
            lines.append(f"{name:<18}{'no results':>16}")
            continue
        firsts = sorted(r.first_token for r in ok if r.first_token is not None)
        p95 = firsts[min(len(firsts) - 1, int(len(firsts) * 0.95))]
        clean = sum(r.clean for r in ok) / len(ok)
        per_1000 = statistics.fmean(r.cost for r in ok) * 1000
        lines.append(f"{name:<18}{statistics.median(firsts) * 1000:>13.0f} ms{p95 * 1000:>6.0f} ms{clean:>7.0%}{per_1000:>17.4f}$")
    return "\n".join(lines)


def load_providers(path: Path) -> list[BenchProvider]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    return [BenchProvider(**entry) for entry in data.get("provider", [])]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="completionist-bench", description="Compare phrase providers on speed, cost and cleanliness.")
    parser.add_argument("providers", type=Path, help="TOML file listing the providers to test")
    parser.add_argument("--json", type=Path, help="also write the raw results here")
    parser.add_argument("--contexts", type=int, nargs="*", default=[500, 2000, 8000], help="context sizes (characters) to sweep")
    args = parser.parse_args(argv)

    providers = load_providers(args.providers)
    if not providers:
        parser.error("no [[provider]] entries found")
    results = asyncio.run(run_bench(providers, SAMPLES, args.contexts))
    print(format_report(results))
    if args.json:
        args.json.write_text(json.dumps([asdict(r) for r in results], indent=2), encoding="utf-8")
    return 0
