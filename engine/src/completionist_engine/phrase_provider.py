"""Phrase completions from an OpenAI-compatible /completions endpoint, streamed."""

import asyncio
import json
from collections.abc import AsyncIterator
from dataclasses import dataclass

import httpx


class ProviderError(Exception):
    """The provider was unreachable, slow, or answered with an error. Never carries the API key."""


@dataclass(frozen=True)
class ProviderSettings:
    base_url: str = "https://openrouter.ai/api/v1"
    models: tuple[str, ...] = ("mistralai/codestral-2508",)  # tried in order: the next only if the one before fails
    api_key_env: str = "OPENROUTER_API_KEY"
    fim: bool = False  # send the text after the caret as `suffix` (fill-in-the-middle); OpenRouter documents none
    max_tokens: int = 40
    temperature: float = 0.2
    timeout: float = 4.0  # seconds to wait for the connection, and between chunks
    stop: tuple[str, ...] = ("\n",)


@dataclass(frozen=True)
class PhraseRequest:
    prompt: str
    suffix: str = ""


class PhraseProvider:
    def __init__(self, settings: ProviderSettings, api_key: str, client: httpx.AsyncClient | None = None) -> None:
        self._settings = settings
        self._api_key = api_key
        self._client = client or httpx.AsyncClient()

    async def aclose(self) -> None:
        await self._client.aclose()

    async def stream(self, request: PhraseRequest) -> AsyncIterator[str]:
        """Yields text chunks as they arrive.

        Tries each configured model in turn; a model that fails before producing any text hands over to
        the next one. The error of the last model is raised if all fail."""
        last_error: ProviderError | None = None
        for model in self._settings.models:
            produced = False
            try:
                async for event in self._stream_one(model, request):
                    produced = True
                    yield event
                return
            except ProviderError as err:
                if produced:
                    raise
                last_error = err
        raise last_error or ProviderError("no phrase model configured")

    async def _stream_one(self, model: str, request: PhraseRequest) -> AsyncIterator[str]:
        s = self._settings
        payload: dict = {
            "model": model,
            "prompt": request.prompt,
            "max_tokens": s.max_tokens,
            "temperature": s.temperature,
            "stream": True,
            "stop": list(s.stop),
        }
        if s.fim and request.suffix:
            payload["suffix"] = request.suffix
        url = s.base_url.rstrip("/") + "/completions"
        headers = {"Authorization": f"Bearer {self._api_key}", "Content-Type": "application/json"}
        try:
            async with self._client.stream(
                "POST", url, json=payload, headers=headers, timeout=httpx.Timeout(s.timeout)
            ) as response:
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
        text = choices[0].get("text")
        if isinstance(text, str) and text:
            return text
    return None
