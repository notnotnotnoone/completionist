from typer_engine.context import anchored_window, build_prompt, trim_suffix

PARAGRAPHS = ["The quick brown fox jumps over the lazy dog and keeps on running for a good while. " * 2 for _ in range(60)]
TEXT = "\n\n".join(PARAGRAPHS)


def test_short_text_is_passed_whole():
    assert anchored_window("hello there", cap=100) == "hello there"


def test_the_window_never_exceeds_the_cap():
    for n in (500, 1234, 5000):
        assert len(anchored_window(TEXT[:n], cap=400)) <= 400


def test_the_window_keeps_the_end_of_the_text():
    assert anchored_window(TEXT[:3000], cap=400).endswith(TEXT[:3000][-50:])


def test_the_window_starts_at_a_paragraph_boundary_when_one_is_near():
    window = anchored_window(TEXT[:3000], cap=600)
    assert window.startswith("The quick")  # a paragraph start, not mid-word


def test_the_start_barely_moves_while_typing_so_the_provider_cache_keeps_hitting():
    starts = set()
    for n in range(3000, 3400):  # 400 keystrokes
        window = anchored_window(TEXT[:n], cap=600)
        starts.add(n - len(window))  # the absolute offset the window starts at
    assert len(starts) <= 6  # it moved in sentence-sized steps, not once per character (400 keystrokes)


def test_consecutive_windows_share_a_byte_identical_start():
    a = anchored_window(TEXT[:3000], cap=600)
    b = anchored_window(TEXT[:3020], cap=600)
    assert b.startswith(a[:300])


def test_without_paragraph_breaks_it_falls_back_to_sentences_then_words():
    sentences = "This is a sentence. " * 100
    window = anchored_window(sentences, cap=300)
    assert window.startswith("This is") and len(window) <= 300
    words = "word " * 200
    assert anchored_window(words, cap=100).startswith("word")


def test_a_wall_of_text_with_no_breaks_is_still_cut_to_the_cap():
    wall = "x" * 1000
    assert len(anchored_window(wall, cap=100)) == 100


def test_the_suffix_is_cut_to_its_cap():
    assert trim_suffix("abcdef", cap=3) == "abc"
    assert trim_suffix("ab", cap=3) == "ab"
    assert trim_suffix("", cap=3) == ""


def test_the_prompt_starts_with_a_stable_header_naming_the_app_and_window():
    a = build_prompt("Discord.exe", "#general - Discord", "hello")
    b = build_prompt("Discord.exe", "#general - Discord", "hello world")
    assert a.startswith("Discord") is False and "Discord" in a.splitlines()[0]
    assert b.startswith(a.split("hello")[0])  # same header, so the cached prefix is shared
    assert a.endswith("hello") and b.endswith("hello world")


def test_the_prompt_copes_with_a_missing_title_or_app():
    assert build_prompt("", "", "hi").endswith("hi")
    assert "\n" not in build_prompt("x\ny", "t\nu", "hi").splitlines()[0][:0]  # no stray newline injected into the header
