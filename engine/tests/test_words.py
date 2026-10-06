import pytest

from completionist_engine.words import Completion, WordCompleter, current_word

VOCAB = [
    ("the", 7.7),
    ("there", 6.3),
    ("then", 6.1),
    ("they", 6.6),
    ("theory", 4.9),
    ("recommend", 4.6),
    ("recommendation", 4.4),
    ("recommended", 4.7),
    ("world", 5.8),
    ("work", 6.0),
    ("i", 7.0),
    ("i'm", 6.2),
    ("i'll", 5.5),
    ("don't", 6.2),
    ("done", 5.7),
    ("nasa", 4.0),
]


@pytest.fixture
def completer():
    return WordCompleter(VOCAB)


@pytest.mark.parametrize(
    ("before", "expected"),
    [
        ("hello wor", "wor"),
        ("hello ", ""),
        ("", ""),
        ("(recom", "recom"),
        ("I don'", "don'"),
        ("x = abc123", ""),
        ("end.", ""),
        ("café", "café"),
    ],
)
def test_current_word_is_the_trailing_run_of_letters(before, expected):
    assert current_word(before) == expected


def test_completions_are_ranked_by_frequency(completer):
    assert completer.complete("the").words == ("they", "there", "then", "theory")


def test_replace_is_the_length_of_the_typed_prefix(completer):
    assert completer.complete("hello wor").replace == 3


def test_limit_caps_the_number_of_words(completer):
    assert completer.complete("the", limit=2).words == ("they", "there")


def test_the_exact_typed_word_is_not_suggested_back(completer):
    assert "done" not in completer.complete("done").words


def test_no_prefix_means_no_completion(completer):
    assert completer.complete("hello ") == Completion(replace=0, words=())


def test_unknown_prefix_gives_no_words(completer):
    assert completer.complete("zzq") == Completion(replace=3, words=())


def test_capitalised_prefix_capitalises_suggestions(completer):
    assert completer.complete("Recomm").words == ("Recommended", "Recommend", "Recommendation")


def test_all_caps_prefix_gives_all_caps_suggestions(completer):
    assert completer.complete("NA").words == ("NASA",)


def test_matching_ignores_the_case_of_the_prefix(completer):
    assert completer.complete("WOR").words == ("WORK", "WORLD")


def test_first_person_i_is_always_capitalised(completer):
    assert completer.complete("so i").words == ("I'm", "I'll")


def test_contractions_complete_after_an_apostrophe(completer):
    assert completer.complete("I don'").words == ("don't",)


def test_is_word_knows_whole_dictionary_words_only():
    completer = WordCompleter([("is", 5.0), ("island", 2.0)])
    assert completer.is_word("is") and completer.is_word("Is")
    assert not completer.is_word("isl")
