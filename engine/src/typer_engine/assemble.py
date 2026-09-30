"""Puts the engine together from the config and whatever data files exist."""

import logging
import os
from dataclasses import dataclass

from typer_engine.budget import DailyBudget
from typer_engine.config import Config
from typer_engine.engine import Engine
from typer_engine.metrics import Metrics
from typer_engine.ngrams import NgramTable
from typer_engine.personal import PersonalStore
from typer_engine.phrases import PhraseService
from typer_engine.vocabulary import filter_vocabulary
from typer_engine.words import WordCompleter

logger = logging.getLogger("typer_engine")

NGRAMS_FILE = "ngrams.sqlite"
PERSONAL_FILE = "personal.sqlite"
SPEND_FILE = "spend.json"
METRICS_FILE = "metrics.sqlite"


@dataclass
class Assembled:
    engine: Engine
    personal: PersonalStore | None
    ngrams: NgramTable | None
    phrases: PhraseService | None = None
    metrics: Metrics | None = None

    def flush(self) -> None:
        """Save what's been learned and counted (called every few seconds and on exit)."""
        if self.personal is not None:
            self.personal.flush()
        if self.metrics is not None:
            self.metrics.flush()

    async def aclose(self) -> None:
        """Release network resources (call before `close`)."""
        if self.phrases is not None:
            await self.phrases.aclose()

    def close(self) -> None:
        if self.personal is not None:
            self.personal.close()
        if self.metrics is not None:
            self.metrics.close()
        if self.ngrams is not None:
            self.ngrams.close()


def assemble_engine(
    config: Config,
    vocabulary: list[tuple[str, float]],
    *,
    core_rank: int = 20_000,
    phrase_api_key: str | None = None,
) -> Assembled:
    """The engine with n-gram ranking (if `ngrams.sqlite` is in the data folder), personal learning and phrases.

    Phrases need an API key: pass `phrase_api_key`, or set the environment variable the config names.
    """
    ngrams = _open_ngrams(config)
    if ngrams is not None:
        vocabulary = filter_vocabulary(vocabulary, ngrams.unigram_stats(), core_rank=core_rank)
    personal = PersonalStore(config.data_dir / PERSONAL_FILE) if config.learning else None
    completer = WordCompleter(vocabulary, ngrams=ngrams, personal=personal, promote_after=config.promote_after)
    metrics = Metrics(config.data_dir / METRICS_FILE)
    phrases = _phrase_service(config, phrase_api_key, metrics)
    engine = Engine(completer, config, personal=personal, phrases=phrases, metrics=metrics)
    return Assembled(engine, personal, ngrams, phrases, metrics)


def _open_ngrams(config: Config) -> NgramTable | None:
    path = config.data_dir / NGRAMS_FILE
    if not path.exists():
        logger.info("no n-gram file at %s: ranking by word frequency only (typer-build-ngrams makes one)", path)
        return None
    try:
        table = NgramTable(path)
    except (ValueError, OSError) as err:
        logger.warning("ignoring n-gram file %s: %s", path, err)
        return None
    logger.info("using n-gram file %s", path)
    return table


def _phrase_service(config: Config, api_key: str | None, metrics: Metrics) -> PhraseService | None:
    phrase = config.phrase
    key = api_key if api_key is not None else os.environ.get(phrase.provider.api_key_env)
    budget = DailyBudget(phrase.daily_budget_usd, phrase.prices, path=config.data_dir / SPEND_FILE)
    service = PhraseService(phrase, key, budget, metrics=metrics, key_from_env=api_key is None)
    if service.available:
        logger.info(
            "phrases on: %s at %s, budget $%.2f/day ($%.4f spent today)",
            ", ".join(phrase.provider.models),
            phrase.provider.base_url,
            phrase.daily_budget_usd,
            budget.spent_today,
        )
    else:
        logger.info("phrases off: set %s to turn them on", phrase.provider.api_key_env)
    return service
