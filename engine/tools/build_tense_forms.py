"""Regenerate src/completionist_engine/data/tense_forms.tsv from UniMorph English.

UniMorph (https://github.com/unimorph/eng, CC BY-SA 3.0) is a public inflection table derived from
Wiktionary. Usage: python tools/build_tense_forms.py PATH_TO_UNIMORPH_ENG_FILE

Each output line is `word<TAB>code`:
  P  past tense verb form (walked, went, was)
  S  present tense third-person form that is only ever a verb (is, goes)
  s  present third-person form that is also a plural noun (walks, works); only ever boosts, never demotes
Forms that are also a base or -ing form (read, put, cut, found) are left out: they say nothing about tense.
"""

import sys
from pathlib import Path

from wordfreq import top_n_list

OUT = Path(__file__).resolve().parent.parent / "src" / "completionist_engine" / "data" / "tense_forms.tsv"
EXTRA = {  # forms UniMorph lacks, or contractions
    "am": "S", "does": "S", "are": "S", "is": "S", "was": "P", "were": "P", "isn't": "S", "aren't": "S", "doesn't": "S",
    "wasn't": "P", "weren't": "P", "didn't": "P", "hadn't": "P", "hasn't": "S",
}


STOP = {"its"}  # UniMorph lists it as a verb form of "it"


def main(path: str) -> None:
    past, third, neutral, plural = set(), set(), set(), set()
    verbs, has_past, nouns = set(), set(), set()  # lemmas
    common = set(top_n_list("en", 150_000, wordlist="large"))
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        lemma, form, tags = parts
        if tags.startswith("V;"):
            verbs.add(lemma)
        if tags == "V;PST":
            has_past.add(lemma)
        if tags.startswith("N;"):
            nouns.add(lemma)
        if tags == "V;PST":
            past.add(form)
        elif tags == "V;PRS;3;SG":
            third.add(form)
        elif tags.startswith("V;") and tags != "V;V.PTCP;PST":  # a past participle is the same word as the past form
            neutral.add(form)
        elif tags == "N;PL":
            plural.add(form)
    # UniMorph lists no past forms for some regular verbs (want, play); spell those out by the usual rules.
    for lemma in sorted(verbs - has_past):
        if len(lemma) < 3 or not lemma.isalpha():
            continue
        past_forms = {lemma + "ed", lemma + "d", lemma + lemma[-1] + "ed"}
        third_forms = {lemma + "s", lemma + "es"}
        if lemma.endswith("y") and lemma[-2] not in "aeiou":
            past_forms.add(lemma[:-1] + "ied")
            third_forms.add(lemma[:-1] + "ies")
        past.update(f for f in past_forms if f in common)
        third.update(f for f in third_forms if f in common)
        if lemma in nouns:
            plural.update(third_forms)
    table: dict[str, str] = {}
    for form in sorted(common):
        if form in neutral or form in STOP or not form.isalpha():
            continue
        if form in past and form not in third:
            table[form] = "P"
        elif form in third and form not in past:
            table[form] = "s" if form in plural else "S"
    table.update(EXTRA)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("".join(f"{w}\t{c}\n" for w, c in sorted(table.items())), encoding="utf-8")
    print(f"wrote {len(table)} forms to {OUT}")


if __name__ == "__main__":
    main(sys.argv[1])
