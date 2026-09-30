import pytest

from completionist_engine.learning import TypingLearner


def type_text(learner: TypingLearner, text: str):
    """Feed the learner one character at a time, as the text service reports them."""
    learned = []
    for i in range(1, len(text) + 1):
        result = learner.observe(text[:i])
        if result:
            learned.append(result)
    return learned


def test_a_word_is_learned_when_a_delimiter_is_typed_after_it():
    assert type_text(TypingLearner(), "hello world ") == [("hello", ()), ("world", ("hello",))]


def test_punctuation_also_completes_a_word():
    assert type_text(TypingLearner(), "so, well.") == [("so", ()), ("well", ("so",))]


def test_a_word_is_not_learned_before_it_is_finished():
    assert type_text(TypingLearner(), "hello wor") == [("hello", ())]


def test_the_last_two_words_are_the_context_for_a_bigram():
    learned = type_text(TypingLearner(), "we want to know ")
    assert learned[-1] == ("know", ("want", "to"))  # two words of context; the store keeps the last


def test_pasted_text_is_not_learned():
    learner = TypingLearner()
    learner.observe("hello ")
    assert learner.observe("hello a whole pasted sentence here ") is None


def test_jumping_to_other_text_is_not_learned():
    learner = TypingLearner()
    learner.observe("some earlier text about cats")
    assert learner.observe("a different document about dogs ") is None


def test_backspacing_does_not_learn():
    learner = TypingLearner()
    learner.observe("hello world")
    assert learner.observe("hello worl") is None
    assert learner.observe("hello wor") is None


def test_retyping_after_backspace_learns_the_word_again_once_finished():
    learner = TypingLearner()
    learner.observe("hello world")
    learner.observe("hello worl")
    learner.observe("hello world")
    assert learner.observe("hello world ") == ("world", ("hello",))


def test_a_delimiter_after_a_delimiter_learns_nothing():
    learner = TypingLearner()
    learner.observe("hello ")
    assert learner.observe("hello  ") is None


def test_digits_and_symbols_are_not_words():
    assert type_text(TypingLearner(), "abc123 ") == []
    assert type_text(TypingLearner(), "x = 5; ") == [("x", ())]


def test_works_when_the_text_service_only_sends_the_last_characters():
    # The DLL caps `before` at a fixed length, so once full it slides instead of growing.
    text = "word " * 500 + "hello"
    learner = TypingLearner()
    learner.observe(text[-2000:])
    assert learner.observe((text + " ")[-2000:]) == ("hello", ("word", "word"))


def test_reset_forgets_the_previous_text():
    learner = TypingLearner()
    learner.observe("hello")
    learner.reset()
    assert learner.observe("hello ") is None


@pytest.mark.parametrize("word", ["a", "I", "x"])
def test_single_letters_are_passed_through_for_the_store_to_judge(word):
    assert type_text(TypingLearner(), word + " ") == [(word.lower(), ())]
