import gzip

import pytest

from completionist_engine.ngrams import NgramTable, build_ngrams

CORPUS = """\
I want to know the answer. I want to know more about it. We want to see the answer.
They said the answer is here. Please let me know the answer. He wants to know.
Bill Updike met Bill Updike. Then Updike left. Updike wrote it.
"""


@pytest.fixture
def table(tmp_path):
    source = tmp_path / "corpus.txt"
    source.write_text(CORPUS, encoding="utf-8")
    out = tmp_path / "ngrams.sqlite"
    build_ngrams([source], out, vocab_size=1000, min_unigram=1, min_bigram=1, min_trigram=1)
    table = NgramTable(out)
    yield table
    table.close()


def test_bigram_counts_give_what_follows_a_word(table):
    counts = table.counts(("to",), "kn")
    assert dict(counts.words) == {"know": 3}


def test_the_total_is_how_often_the_context_word_was_seen(table):
    assert table.counts(("to",), "kn").total == 4  # "to" appears 4 times
    assert table.counts(("to",), "se").words == {"see": 1}


def test_trigram_counts_give_what_follows_two_words(table):
    counts = table.counts(("want", "to"), "kn")
    assert dict(counts.words) == {"know": 2}
    assert counts.total == 3  # "want to" was seen three times


def test_the_prefix_filters_continuations(table):
    everything = table.counts(("the",), "")
    assert {"answer", "answer"} <= set(everything.words)
    assert dict(table.counts(("the",), "an").words) == {"answer": 4}
    assert dict(table.counts(("the",), "zz").words) == {}


def test_an_unseen_context_has_no_counts(table):
    counts = table.counts(("zebra",), "a")
    assert dict(counts.words) == {} and counts.total == 0
    assert table.counts(("nope", "never"), "a").total == 0


def test_contexts_are_not_carried_across_sentences(table):
    # "...about it. We want" must not count "it" before "we".
    assert dict(table.counts(("it",), "we").words) == {}


def test_case_is_ignored_when_counting(table):
    assert table.counts(("i",), "wa").words == {"want": 2}


def test_unigram_stats_record_how_often_a_word_is_written_lowercase(table):
    stats = table.unigram_stats()
    assert stats["answer"].total == 4 and stats["answer"].lower == 4
    assert stats["updike"].total == 4 and stats["updike"].lower == 0  # only ever capitalised: a name
    assert stats["bill"].total == 2 and stats["bill"].lower == 0


def test_the_pronoun_i_counts_as_lowercase_usage(table):
    assert table.unigram_stats()["i"].lower == table.unigram_stats()["i"].total


def test_min_counts_drop_rare_ngrams(tmp_path):
    source = tmp_path / "c.txt"
    source.write_text(CORPUS, encoding="utf-8")
    out = tmp_path / "n.sqlite"
    build_ngrams([source], out, vocab_size=1000, min_unigram=1, min_bigram=3, min_trigram=2)
    with NgramTable(out) as t:
        assert dict(t.counts(("to",), "kn").words) == {"know": 3}
        assert dict(t.counts(("to",), "se").words) == {}  # "to see" happened once
        assert dict(t.counts(("want", "to"), "kn").words) == {"know": 2}
        assert dict(t.counts(("we", "want"), "to").words) == {}  # once


def test_words_outside_the_vocabulary_are_left_out_and_break_the_chain(tmp_path):
    source = tmp_path / "c.txt"
    source.write_text("alpha beta gamma. alpha beta gamma. alpha beta gamma. alpha zeta gamma.", encoding="utf-8")
    out = tmp_path / "n.sqlite"
    # Only the top 3 words are kept: alpha, gamma and one of beta/zeta.
    build_ngrams([source], out, vocab_size=3, min_unigram=1, min_bigram=1, min_trigram=1)
    with NgramTable(out) as t:
        assert "zeta" not in t.unigram_stats()
        assert dict(t.counts(("alpha",), "").words) == {"beta": 3}


def test_numbers_break_the_chain(tmp_path):
    source = tmp_path / "c.txt"
    source.write_text("we paid 20 dollars. we paid 20 dollars.", encoding="utf-8")
    out = tmp_path / "n.sqlite"
    build_ngrams([source], out, vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1)
    with NgramTable(out) as t:
        assert dict(t.counts(("paid",), "").words) == {}
        assert dict(t.counts(("we",), "").words) == {"paid": 2}


def test_only_the_most_frequent_continuations_of_a_context_are_kept(tmp_path):
    source = tmp_path / "c.txt"
    lines = []
    for i, word in enumerate(["aa", "bb", "cc", "dd", "ee"]):
        lines += [f"go {word}."] * (5 - i)
    source.write_text("\n".join(lines), encoding="utf-8")
    out = tmp_path / "n.sqlite"
    build_ngrams([source], out, vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1, per_context=3)
    with NgramTable(out) as t:
        assert dict(t.counts(("go",), "").words) == {"aa": 5, "bb": 4, "cc": 3}


def test_gzip_files_and_directories_are_read(tmp_path):
    folder = tmp_path / "texts"
    folder.mkdir()
    (folder / "a.txt").write_text("we want to know.", encoding="utf-8")
    with gzip.open(folder / "b.txt.gz", "wt", encoding="utf-8") as f:
        f.write("we want to know.")
    out = tmp_path / "n.sqlite"
    build_ngrams([folder], out, vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1)
    with NgramTable(out) as t:
        assert dict(t.counts(("want", "to"), "kn").words) == {"know": 2}


def test_pruning_while_counting_keeps_the_frequent_ngrams(tmp_path):
    source = tmp_path / "c.txt"
    frequent = "we want to know. " * 30
    rare = " ".join(f"w{chr(97 + i % 26)}{chr(97 + i // 26)} x." for i in range(200))
    source.write_text(frequent + rare, encoding="utf-8")
    out = tmp_path / "n.sqlite"
    build_ngrams([source], out, vocab_size=1000, min_unigram=1, min_bigram=2, min_trigram=2, max_entries=50)
    with NgramTable(out) as t:
        assert dict(t.counts(("want", "to"), "kn").words) == {"know": 30}


def test_building_reports_what_it_made(tmp_path):
    source = tmp_path / "c.txt"
    source.write_text(CORPUS, encoding="utf-8")
    report = build_ngrams([source], tmp_path / "n.sqlite", vocab_size=1000, min_unigram=1, min_bigram=1, min_trigram=1)
    assert report.tokens > 30 and report.vocabulary > 10 and report.bigrams > 10 and report.trigrams > 5
    assert report.size_bytes > 0


def test_opening_a_missing_or_non_ngram_file_fails_clearly(tmp_path):
    with pytest.raises(FileNotFoundError):
        NgramTable(tmp_path / "missing.sqlite")
    bad = tmp_path / "bad.sqlite"
    bad.write_bytes(b"nope" * 100)
    with pytest.raises(ValueError):
        NgramTable(bad)


def test_split_contractions_are_joined_back(tmp_path):
    source = tmp_path / "c.txt"
    source.write_text("we do n't know . it 's here . we do n't know . it 's here .\n", encoding="utf-8")
    out = tmp_path / "n.sqlite"
    build_ngrams([source], out, vocab_size=100, min_unigram=1, min_bigram=1, min_trigram=1)
    with NgramTable(out) as t:
        stats = t.unigram_stats()
        assert "don't" in stats and "it's" in stats
        assert "n't" not in stats and "s" not in stats
        assert dict(t.counts(("we",), "").words) == {"don't": 2}
