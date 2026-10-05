"""Past or present tense of the sentence being typed, from the verb forms and time words in it.

The verb forms come from `data/tense_forms.tsv` (built from UniMorph, which is derived from Wiktionary;
see tools/build_tense_forms.py). Each line is a word and a code: P past, S present, s present third-person
form that is also a plural noun (walks, works). The detector prefers `none` to a wrong guess.
"""

from collections.abc import Mapping, Sequence
from functools import cache
from importlib import resources
from typing import Literal

Tense = Literal["past", "present", "none"]

# A past-looking word after these is a participle (has walked, is closed), not a past tense.
_AUXILIARIES = frozenset({"have", "has", "had", "having", "be", "been", "being", "am", "is", "are", "'ve"})
_PAST_TIME = frozenset({"yesterday", "ago", "earlier", "previously", "formerly"})
_PRESENT_TIME = frozenset({"today", "now", "currently", "nowadays"})


@cache
def load_tense_forms() -> Mapping[str, str]:
    text = resources.files("completionist_engine").joinpath("data/tense_forms.tsv").read_text(encoding="utf-8")
    return dict(line.split("\t") for line in text.splitlines() if line)


def detect_tense(words: Sequence[str], forms: Mapping[str, str]) -> Tense:
    """`words` are the lowercase words of the current sentence so far. One tense only gives that tense;
    both tenses, or no sign of either, give `none`."""
    found: set[str] = set()
    previous = ""
    for word in words:
        code = forms.get(word)
        if word in _PAST_TIME or (code == "P" and previous not in _AUXILIARIES):
            found.add("past")
        elif word in _PRESENT_TIME or code == "S":
            found.add("present")
        previous = word
    if len(found) == 1:
        return "past" if "past" in found else "present"
    return "none"


def tense_of_word(word: str, forms: Mapping[str, str]) -> Tense:
    """The tense a candidate word is, or `none`. A word that is also a noun never counts as past."""
    code = forms.get(word)
    return {"P": "past", "S": "present", "s": "present"}.get(code or "", "none")
