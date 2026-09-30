from typer_engine.config import Config
from typer_engine.ngrams import build_ngrams
from typer_engine.protocol import Request
from typer_engine.assemble import assemble_engine

VOCAB = [("knowledge", 6.0), ("know", 5.0), ("knew", 4.0), ("the", 9.0)]


def ask(assembled, before):
    session = assembled.engine.open_session()
    return session.handle(Request(id=1, event="keystroke", app="discord.exe", before=before)).words


def test_without_data_files_the_engine_ranks_by_frequency(tmp_path):
    assembled = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    assert ask(assembled, "kn")[:2] == ("knowledge", "know")
    assert assembled.ngrams is None
    assembled.close()


def test_an_ngram_file_in_the_data_folder_is_used(tmp_path):
    corpus = tmp_path / "c.txt"
    corpus.write_text("we want to know. " * 10, encoding="utf-8")
    build_ngrams([corpus], tmp_path / "ngrams.sqlite", vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1)
    assembled = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    assert assembled.ngrams is not None
    assert ask(assembled, "I want to kn")[0] == "know"
    assembled.close()


def test_a_broken_ngram_file_is_ignored(tmp_path):
    (tmp_path / "ngrams.sqlite").write_bytes(b"garbage" * 100)
    assembled = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    assert assembled.ngrams is None
    assert ask(assembled, "kn")[0] == "knowledge"
    assembled.close()


def test_ngram_stats_drop_misspellings_beyond_the_core(tmp_path):
    corpus = tmp_path / "c.txt"
    corpus.write_text("the know the know the know knowledge knowledge knowledge. ", encoding="utf-8")
    build_ngrams([corpus], tmp_path / "ngrams.sqlite", vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1)
    vocab = [*VOCAB, ("knowe", 1.0)]  # a typo the corpus never saw
    assembled = assemble_engine(Config(data_dir=tmp_path), vocab, core_rank=4)
    assert "knowe" not in ask(assembled, "kn")
    assembled.close()


def test_learned_words_are_saved_when_the_engine_closes(tmp_path):
    first = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    session = first.engine.open_session()
    for text in ["hey", "hey ", "hey l", "hey li", "hey lin", "hey linq", "hey linqi", "hey linqi "]:
        session.handle(Request(id=1, event="keystroke", app="discord.exe", before=text))
    first.close()
    second = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    assert dict(second.personal.counts((), "lin").words) == {"linqi": 1}
    second.close()


def test_learning_off_creates_no_personal_file(tmp_path):
    assembled = assemble_engine(Config(data_dir=tmp_path, learning=False), VOCAB)
    assembled.close()
    assert not (tmp_path / "personal.sqlite").exists()
    assert assembled.personal is None


def test_build_cli_makes_a_file_the_engine_can_use(tmp_path, capsys):
    from typer_engine.build_ngrams import main

    corpus = tmp_path / "c.txt"
    corpus.write_text("we want to know. " * 10, encoding="utf-8")
    out = tmp_path / "ngrams.sqlite"
    assert main([str(corpus), "--out", str(out), "--min-bigram", "1", "--min-trigram", "1"]) == 0
    assert "wrote" in capsys.readouterr().out
    assembled = assemble_engine(Config(data_dir=tmp_path), VOCAB)
    assert ask(assembled, "I want to kn")[0] == "know"
    assembled.close()


def test_build_cli_reports_a_missing_source_without_a_traceback(tmp_path, capsys):
    import pytest

    from typer_engine.build_ngrams import main

    with pytest.raises(SystemExit) as exit_info:
        main([str(tmp_path / "nope.txt"), "--out", str(tmp_path / "o.sqlite")])
    assert exit_info.value.code == 2
    assert "not found" in capsys.readouterr().err
