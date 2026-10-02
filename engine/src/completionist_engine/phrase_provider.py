"""Phrase completions from an OpenAI-compatible chat-completions endpoint, streamed."""

import asyncio
import json
import time
from collections.abc import AsyncIterator, Callable
from dataclasses import dataclass, field

import httpx


class ProviderError(Exception):
    """The provider was unreachable, slow, or answered with an error. Never carries the API key."""


@dataclass(frozen=True)
class Attempt:
    """One model tried for a request: whether it worked, why not, and how long it took."""

    model: str
    ok: bool
    error: str
    ms: int


@dataclass(frozen=True)
class ProviderSettings:
    base_url: str = "https://openrouter.ai/api/v1"
    models: tuple[str, ...] = ("meta-llama/llama-3.3-70b-instruct",)  # tried in order: the next only if the one before fails
    provider_order: tuple[str, ...] = ("Groq",)  # OpenRouter providers to try first; others stay as fallbacks
    api_key: str = field(default="", repr=False)  # from the config file; empty means phrases are off
    fim: bool = False  # send the text after the caret as `suffix` (fill-in-the-middle); OpenRouter documents none
    max_tokens: int = 40
    temperature: float = 0.2
    timeout: float = 4.0  # seconds to wait for the connection, and between chunks
    stop: tuple[str, ...] = ("\n",)
    provider_sort: str = ""
    allow_fallbacks: bool = True
    provider_ignore: tuple[str, ...] = ()
    max_price_input: float = 0
    max_price_output: float = 0
    require_parameters: bool = False
    zdr: bool = False
    reasoning_effort: str = "none"


@dataclass(frozen=True)
class PhraseRequest:
    prompt: str
    suffix: str = ""
    system_prompt: str = ""

    @property
    def display_prompt(self) -> str:
        """Both roles in the labeled form shown in context previews and request logs."""
        if not self.system_prompt:
            return self.prompt
        return f"[SYSTEM MESSAGE]\n{self.system_prompt}\n\n[USER MESSAGE]\n{self.prompt}"


class PhraseProvider:
    def __init__(self, settings: ProviderSettings, api_key: str, client: httpx.AsyncClient | None = None) -> None:
        self._settings = settings
        self._api_key = api_key
        self._client = client or httpx.AsyncClient()

    async def aclose(self) -> None:
        await self._client.aclose()

    async def stream(self, request: PhraseRequest, attempts: list[Attempt] | None = None,
                     on_event: Callable[..., None] | None = None) -> AsyncIterator[str]:
        """Yields text chunks as they arrive.

        Tries each configured model in turn; a model that fails before producing any text hands over to
        the next one. The error of the last model is raised if all fail. If `attempts` is given, each model
        tried is added to it (a model cut off by cancelling is not)."""
        last_error: ProviderError | None = None
        for model in self._settings.models:
            produced = False
            began = time.monotonic()
            if on_event is not None:
                on_event("model_started", model=model)
            try:
                async for event in self._stream_one(model, request, on_event=on_event):
                    produced = True
                    yield event
                if attempts is not None:
                    attempts.append(Attempt(model, True, "", round((time.monotonic() - began) * 1000)))
                if on_event is not None:
                    on_event("model_finished", model=model, duration_ms=round((time.monotonic() - began) * 1000))
                return
            except ProviderError as err:
                if attempts is not None:
                    attempts.append(Attempt(model, False, str(err), round((time.monotonic() - began) * 1000)))
                if on_event is not None:
                    on_event("model_failed", model=model, error=str(err), duration_ms=round((time.monotonic() - began) * 1000))
                if produced:
                    raise
                last_error = err
            except asyncio.CancelledError:
                if on_event is not None:
                    on_event("model_cancelled", model=model, duration_ms=round((time.monotonic() - began) * 1000))
                raise
        raise last_error or ProviderError("no phrase model configured")

    async def _stream_one(self, model: str, request: PhraseRequest,
                          on_event: Callable[..., None] | None = None) -> AsyncIterator[str]:
        s = self._settings
        payload: dict = {
            "model": model,
            "messages": ([{"role": "system", "content": request.system_prompt}] if request.system_prompt else [])
            + [{"role": "user", "content": request.prompt}],
            "max_tokens": s.max_tokens,
            "temperature": s.temperature,
            "stream": True,
            "stop": list(s.stop),
            "reasoning": {"effort": s.reasoning_effort},
        }
        routing: dict = {}
        if s.provider_sort:
            routing["sort"] = s.provider_sort
        elif s.provider_order:
            routing["order"] = list(s.provider_order)
        if not s.allow_fallbacks:
            routing["allow_fallbacks"] = False
        if s.provider_ignore:
            routing["ignore"] = list(s.provider_ignore)
        caps = {key: value for key, value in (("input", s.max_price_input), ("output", s.max_price_output)) if value > 0}
        if caps:
            routing["max_price"] = caps
        if s.require_parameters:
            routing["require_parameters"] = True
        if s.zdr:
            routing["zdr"] = True
        if routing:
            payload["provider"] = routing
        if s.fim and request.suffix:
            payload["suffix"] = request.suffix
        url = s.base_url.rstrip("/") + "/chat/completions"
        headers = {"Authorization": f"Bearer {self._api_key}", "Content-Type": "application/json"}
        if on_event is not None:
            on_event("request_sent", url=url, body=payload)
        try:
            async with self._client.stream(
                "POST", url, json=payload, headers=headers, timeout=httpx.Timeout(s.timeout)
            ) as response:
                if on_event is not None:
                    on_event("response_started", status_code=response.status_code)
                if response.status_code >= 400:
                    await response.aread()
                    raise ProviderError(f"provider answered HTTP {response.status_code}")
                async for line in response.aiter_lines():
                    event = _parse_line(line)
                    if event is not None:
                        yield event
        except httpx.TimeoutException as err:
            raise ProviderError("provider timed out") from err
        except (httpx.HTTPError, asyncio.TimeoutError) as err:
            raise ProviderError(f"provider request failed: {type(err).__name__}") from err


def _parse_line(line: str) -> str | None:
    if not line.startswith("data:"):
        return None
    data = line[5:].strip()
    if not data or data == "[DONE]":
        return None
    try:
        message = json.loads(data)
    except ValueError:
        return None
    if not isinstance(message, dict):
        return None
    choices = message.get("choices")
    if isinstance(choices, list) and choices and isinstance(choices[0], dict):
        delta = choices[0].get("delta")
        text = delta.get("content") if isinstance(delta, dict) else choices[0].get("text")
        if isinstance(text, str) and text:
            return text
    return None
