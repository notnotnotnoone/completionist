"""Puts the engine together from the config and whatever data files exist."""

import logging
from dataclasses import dataclass

from typer_engine.config import Config
from typer_engine.engine import Engine
from typer_engine.ngrams import NgramTable
from typer_engine.personal import PersonalStore
from typer_engine.vocabulary import filter_vocabulary
from typer_engine.words import WordCompleter

logger = logging.getLogger("typer_engine")

NGRAMS_FILE = "ngrams.sqlite"
PERSONAL_FILE = "personal.sqlite"


@dataclass
class Assembled:
    engine: Engine
    personal: PersonalStore | None
    ngrams: NgramTable | None

    def close(self) -> None:
        if self.personal is not None:
            self.personal.close()
        if self.ngrams is not None:
            self.ngrams.close()


def assemble_engine(config: Config, vocabulary: list[tuple[str, float]], *, core_rank: int = 20_000) -> Assembled:
    """The engine with n-gram ranking (if `ngrams.sqlite` is in the data folder) and personal learning."""
    ngrams = _open_ngrams(config)
    if ngrams is not None:
        vocabulary = filter_vocabulary(vocabulary, ngrams.unigram_stats(), core_rank=core_rank)
    personal = PersonalStore(config.data_dir / PERSONAL_FILE) if config.learning else None
    completer = WordCompleter(vocabulary, ngrams=ngrams, personal=personal, promote_after=config.promote_after)
    return Assembled(Engine(completer, config, personal=personal), personal, ngrams)


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
