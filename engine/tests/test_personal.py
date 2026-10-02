import sqlite3

import pytest

from completionist_engine.personal import PersonalStore


@pytest.fixture
def store():
    return PersonalStore()


def test_a_new_store_knows_nothing(store):
    counts = store.counts((), "a")
    assert dict(counts.words) == {} and counts.total == 0


def test_typed_words_are_counted_and_found_by_prefix(store):
    for _ in range(3):
        store.record_typed("linqi", ())
    store.record_typed("linux", ())
    counts = store.counts((), "li")
    assert dict(counts.words) == {"linqi": 3, "linux": 1}
    assert counts.total == 4  # every word ever recorded, not just those matching the prefix


def test_prefix_matching_is_exact_not_fuzzy(store):
    store.record_typed("linqi", ())
    assert dict(store.counts((), "lq").words) == {}
    assert dict(store.counts((), "linqi").words) == {"linqi": 1}


def test_an_accepted_word_counts_too(store):
    store.record_typed("linqi", ())
    store.record_accepted("linqi", ())
    assert store.counts((), "linq").words["linqi"] == 2


def test_the_word_after_the_previous_word_is_counted_as_a_bigram(store):
    store.record_typed("linqi", ("hey",))
    store.record_typed("linqi", ("hey",))
    store.record_typed("lin", ("hey",))
    store.record_typed("linqi", ("dear",))
    counts = store.counts(("hey",), "lin")
    assert dict(counts.words) == {"linqi": 2, "lin": 1}
    assert counts.total == 3  # everything that followed "hey"
    assert store.counts(("dear",), "lin").total == 1


def test_only_the_last_context_word_is_used_for_bigrams(store):
    store.record_typed("world", ("hello", "big"))
    assert store.counts(("big",), "wor").words["world"] == 1
    assert store.counts(("hello",), "wor").total == 0


def test_words_are_stored_lowercase(store):
    store.record_typed("Linqi", ("Hey",))
    assert "linqi" in store.counts((), "lin").words
    assert "linqi" in store.counts(("hey",), "lin").words


@pytest.mark.parametrize("junk", ["", "a1b2", "hello123", "x" * 40, "under_score", "hi-there", "naïve", "http://x", "password!"])
def test_only_plain_english_words_are_recorded(store, junk):
    store.record_typed(junk, ())
    assert store.counts((), "").total == 0


def test_single_letter_words_are_not_learned(store):
    store.record_typed("I", ())
    store.record_accepted("a", ())
    store.record_typed("hello", ("I",))
    assert store.words() == [("hello", 1)]
    assert store.counts((), "").total == 1


def test_existing_single_letter_words_are_removed_on_open(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.close()
    with sqlite3.connect(path) as db:
        db.execute("INSERT INTO words (word, n) VALUES ('i', 3)")
    reopened = PersonalStore(path)
    assert reopened.words() == []
    reopened.close()
    with sqlite3.connect(path) as db:
        assert db.execute("SELECT COUNT(*) FROM words WHERE word = 'i'").fetchone()[0] == 0


def test_words_with_inner_apostrophes_are_kept(store):
    store.record_typed("don't", ())
    assert "don't" in store.counts((), "don").words


def test_results_are_capped_to_the_most_used_words(store):
    for i in range(200):
        for _ in range(i + 1):
            store.record_typed(f"a{chr(97 + i // 26)}{chr(97 + i % 26)}", ())
    counts = store.counts((), "a")
    assert len(counts.words) == 50
    assert max(counts.words.values()) == 200


def test_counts_survive_a_flush_and_reopen(tmp_path):
    path = tmp_path / "personal.sqlite"
    first = PersonalStore(path)
    first.record_typed("linqi", ("hey",))
    first.record_typed("linqi", ("hey",))
    first.record_accepted("completionist", ())
    first.flush()
    first.close()

    second = PersonalStore(path)
    assert second.counts((), "lin").words["linqi"] == 2
    assert second.counts(("hey",), "lin").words["linqi"] == 2
    assert second.counts((), "comp").words["completionist"] == 1
    assert second.counts((), "").total == 3
    second.close()


def test_unflushed_counts_are_written_on_close(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("linqi", ())
    store.close()
    assert PersonalStore(path).counts((), "lin").words["linqi"] == 1


def test_flushing_twice_does_not_double_count(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("linqi", ())
    store.flush()
    store.flush()
    store.record_typed("linqi", ())
    store.close()
    assert PersonalStore(path).counts((), "lin").words["linqi"] == 2


def test_the_file_holds_only_counts_of_single_words_never_text(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    for word in ["hello", "world"]:
        store.record_typed(word, ("hi",))
    store.close()
    with sqlite3.connect(path) as db:
        tables = {row[0] for row in db.execute("select name from sqlite_master where type='table'")}
        words = {tuple(row) for row in db.execute("select word, n from words")}
        bigrams = {tuple(row) for row in db.execute("select prev, word, n from bigrams")}
    assert tables == {"words", "bigrams", "trigrams"}
    assert words == {("hello", 1), ("world", 1)}
    assert bigrams == {("hi", "hello", 1), ("hi", "world", 1)}


def test_a_corrupt_file_is_set_aside_and_the_store_starts_empty(tmp_path):
    path = tmp_path / "personal.sqlite"
    path.write_bytes(b"this is not a sqlite database" * 100)
    store = PersonalStore(path)
    assert store.counts((), "").total == 0
    store.record_typed("linqi", ())
    store.close()
    assert PersonalStore(path).counts((), "lin").words["linqi"] == 1
    assert list(tmp_path.glob("personal.sqlite.corrupt*"))


def test_clear_forgets_everything_including_the_file(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("linqi", ("hey",))
    store.flush()
    store.clear()
    assert store.counts((), "").total == 0
    store.close()
    assert PersonalStore(path).counts((), "").total == 0


def test_rare_bigrams_are_pruned_when_the_table_grows_too_large(tmp_path):
    store = PersonalStore(tmp_path / "personal.sqlite", max_bigrams=10)
    for i in range(30):
        store.record_typed("word", (f"prev{chr(97 + i)}",))
    for _ in range(3):
        store.record_typed("keeper", ("prevz",))
    store.flush()
    assert dict(store.counts(("prevz",), "keep").words) == {"keeper": 3}  # repeated pairs survive
    assert store.counts(("preva",), "word").total == 0  # one-off pairs went


def test_three_words_typed_in_a_row_are_counted_as_a_trigram(store):
    for _ in range(2):
        store.record_typed("linqi", ("hey", "there"))
    store.record_typed("lin", ("hey", "there"))
    store.record_typed("linqi", ("oh", "there"))
    counts = store.counts(("hey", "there"), "lin")
    assert dict(counts.words) == {"linqi": 2, "lin": 1}
    assert counts.total == 3  # everything that followed "hey there"
    assert dict(store.counts(("oh", "there"), "lin").words) == {"linqi": 1}


def test_a_trigram_needs_both_words_of_context(store):
    store.record_typed("world", ("hello",))
    assert store.counts(("hello", "big"), "wor").total == 0
    assert store.counts(("hello",), "wor").words["world"] == 1


def test_trigrams_come_from_the_last_two_context_words_only(store):
    store.record_typed("world", ("so", "hello", "big"))
    assert store.counts(("hello", "big"), "wor").words["world"] == 1
    assert store.counts(("so", "hello"), "wor").total == 0


def test_a_trigram_with_an_unusable_word_is_not_recorded(store):
    store.record_typed("world", ("hello", "b1g"))
    assert store.counts(("hello", "b1g"), "wor").total == 0


def test_trigram_counts_survive_a_reopen(tmp_path):
    path = tmp_path / "personal.sqlite"
    first = PersonalStore(path)
    first.record_typed("world", ("hello", "big"))
    first.record_typed("world", ("hello", "big"))
    first.close()
    second = PersonalStore(path)
    assert second.counts(("hello", "big"), "wor").words["world"] == 2
    with sqlite3.connect(path) as db:
        rows = {tuple(r) for r in db.execute("select prev2, prev1, word, n from trigrams")}
    assert rows == {("hello", "big", "world", 2)}
    second.close()


def test_clear_forgets_trigrams_too(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("world", ("hello", "big"))
    store.flush()
    store.clear()
    assert store.counts(("hello", "big"), "wor").total == 0
    store.close()
    assert PersonalStore(path).counts(("hello", "big"), "wor").total == 0


def test_pruning_drops_single_use_trigrams_when_there_are_too_many(tmp_path):
    store = PersonalStore(tmp_path / "p.sqlite", max_bigrams=3)
    for i in range(6):
        store.record_typed(f"w{chr(97 + i)}", ("hello", "big"))
    store.record_typed("keep", ("hello", "big"))
    store.record_typed("keep", ("hello", "big"))
    store.flush()
    assert dict(store.counts(("hello", "big"), "").words) == {"keep": 2}
    store.close()


# -- looking at what was learned, and forgetting it ---------------------------------------------------


def test_words_are_listed_most_used_first_and_can_be_searched(store):
    for word, times in [("linqi", 3), ("linux", 1), ("hello", 5)]:
        for _ in range(times):
            store.record_typed(word, ())
    assert store.words() == [("hello", 5), ("linqi", 3), ("linux", 1)]
    assert store.words("lin") == [("linqi", 3), ("linux", 1)]
    assert store.words("inq") == [("linqi", 3)]  # a search matches anywhere in the word
    assert store.words(limit=2) == [("hello", 5), ("linqi", 3)]
    assert store.words("LIN") == [("linqi", 3), ("linux", 1)]


def test_forgetting_a_word_removes_it_and_every_pair_and_triple_it_is_in(store):
    store.record_typed("linqi", ("hey", "there"))
    store.record_typed("said", ("linqi", "hey"))
    store.record_typed("keep", ("hey", "there"))
    assert store.forget("Linqi") is True
    assert store.words() == [("keep", 1), ("said", 1)]
    assert dict(store.counts(("hey", "there"), "").words) == {"keep": 1}
    assert store.counts(("hey", "there"), "").total == 1
    assert store.counts(("linqi",), "").total == 0
    assert store.counts(("linqi", "hey"), "").total == 0
    assert store.counts((), "").total == 2  # the running total follows


def test_forgetting_an_unknown_word_says_so(store):
    assert store.forget("nothing") is False


def test_a_forgotten_word_can_be_learned_again_from_scratch(store):
    for _ in range(3):
        store.record_typed("linqi", ("hey",))
    store.forget("linqi")
    store.record_typed("linqi", ("hey",))
    assert store.words() == [("linqi", 1)]
    assert dict(store.counts(("hey",), "").words) == {"linqi": 1}


def test_forgetting_survives_a_reopen_and_leaves_nothing_in_the_file(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("linqi", ("hey", "there"))
    store.record_typed("keep", ("hey", "there"))
    store.close()
    store = PersonalStore(path)
    store.forget("linqi")
    store.close()
    again = PersonalStore(path)
    assert again.words() == [("keep", 1)]
    assert dict(again.counts(("hey", "there"), "").words) == {"keep": 1}
    again.close()
    with sqlite3.connect(path) as db:
        for table, column in [("words", "word"), ("bigrams", "word"), ("trigrams", "word")]:
            assert db.execute(f"select count(*) from {table} where {column} = 'linqi'").fetchone() == (0,)
        assert db.execute("select count(*) from bigrams where prev = 'linqi'").fetchone() == (0,)


def test_unsaved_counts_for_a_forgotten_word_are_not_written_later(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("linqi", ("hey",))  # not flushed yet
    store.forget("linqi")
    store.close()
    assert PersonalStore(path).words() == []


def test_trigrams_are_listed_most_used_first_and_can_be_searched(store):
    for words, times in [(("thank", "you", "so"), 4), (("thank", "you", "very"), 2), (("see", "you", "soon"), 3)]:
        for _ in range(times):
            store.record_typed(words[2], words[:2])
    assert store.trigrams() == [(("thank", "you", "so"), 4), (("see", "you", "soon"), 3), (("thank", "you", "very"), 2)]
    assert [t for t, _ in store.trigrams("thank you")] == [("thank", "you", "so"), ("thank", "you", "very")]
    assert store.trigrams("SOON") == [(("see", "you", "soon"), 3)]
    assert store.trigrams(limit=1) == [(("thank", "you", "so"), 4)]


def test_forgetting_a_trigram_leaves_its_words_and_pairs(store):
    for _ in range(3):
        store.record_typed("so", ("thank", "you"))
    store.record_typed("very", ("thank", "you"))
    assert store.forget_trigram("Thank", "you", "so") is True
    assert store.trigrams() == [(("thank", "you", "very"), 1)]
    assert store.counts(("thank", "you"), "").total == 1  # the context total follows
    assert dict(store.counts(("you",), "").words) == {"so": 3, "very": 1}  # the two-word pair keeps its counts
    assert ("so", 3) in store.words()
    assert store.forget_trigram("thank", "you", "so") is False


def test_a_forgotten_trigram_stays_forgotten_in_the_file(tmp_path):
    path = tmp_path / "personal.sqlite"
    store = PersonalStore(path)
    store.record_typed("so", ("thank", "you"))
    store.record_typed("very", ("thank", "you"))
    store.flush()
    store.record_typed("much", ("thank", "you"))  # not saved yet when it is forgotten
    assert store.forget_trigram("thank", "you", "so") and store.forget_trigram("thank", "you", "much")
    store.close()
    assert [t for t, _ in PersonalStore(path).trigrams()] == [("thank", "you", "very")]
