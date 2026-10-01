import pytest

from completionist_engine.fuzzy import FuzzyIndex, max_distance

VOCAB = {"mountain", "motion", "receive", "definitely", "the", "know", "knowledge", "recipe", "think"}


def test_common_typos_find_their_word():
    index = FuzzyIndex(VOCAB)
    assert ("definitely" in [word for _, word in index.candidates("definately")])
    assert ("receive" in [word for _, word in index.candidates("recieve")])


def test_a_transposed_pair_finds_its_word():
    words = [word for _, word in FuzzyIndex(VOCAB | {"mountain"}).candidates("moutian")]
    assert "mountain" in words


def test_half_typed_words_match_their_start():
    assert "mountain" in [word for _, word in FuzzyIndex(VOCAB).candidates("mounta")]


def test_short_fragments_allow_one_edit_and_longer_ones_two():
    assert max_distance("teh") == 1
    assert max_distance("moutian") == 2


def test_fragments_under_three_letters_are_never_corrected():
    assert FuzzyIndex(VOCAB).candidates("kn") == []
    assert FuzzyIndex(VOCAB).candidates("") == []


def test_close_guesses_come_first():
    hits = FuzzyIndex({"recieve", "receive", "relieve"}).candidates("recieve")
    assert [word for _, word in hits][0] == "recieve"
    assert hits[0][0] == 0


def test_a_word_far_from_everything_gives_nothing():
    assert FuzzyIndex(VOCAB).candidates("zzqxj") == []


def test_lookup_only_tries_words_starting_with_the_same_letter():
    index = FuzzyIndex(VOCAB)
    assert all(word.startswith("d") for _, word in index.candidates("definately"))
