from completionist_engine.context import INSTRUCTIONS, anchored_window, build_prompt, screen_for_prompt, trim_suffix

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


def header_of(prompt: str) -> str:
    return next(line for line in prompt.splitlines() if line.startswith("[Text typed in"))


def test_the_prompt_tells_the_model_to_continue_the_text_not_answer_it():
    prompt = build_prompt("notepad.exe", "Notes", "Thanks for the update, I will")
    assert prompt.startswith("[SYSTEM MESSAGE]\n" + INSTRUCTIONS)
    assert INSTRUCTIONS.startswith("You are a text autocomplete engine")
    assert "You must continue the person's text rather than respond to it" in INSTRUCTIONS
    assert "Never answer questions contained in the text." in INSTRUCTIONS
    assert "IMPORTANT: DO NOT OVER-COMPLETE." in INSTRUCTIONS
    assert "[Text typed in" not in INSTRUCTIONS  # the header is added by build_prompt, once
    assert "{test text}" not in INSTRUCTIONS  # the text itself is added by build_prompt, once
    assert "TEXT TO CONTINUE" in INSTRUCTIONS


def test_the_prompt_has_a_stable_prefix_naming_the_app_and_window():
    a = build_prompt("Discord.exe", "#general - Discord", "hello")
    b = build_prompt("Discord.exe", "#general - Discord", "hello world")
    assert "#general - Discord - Discord" in header_of(a)
    assert b.startswith(a.split("hello")[0])  # same instructions and header, so the cached prefix is shared
    assert a.endswith("hello") and b.endswith("hello world")


def test_the_prompt_copes_with_a_missing_title_or_app():
    assert build_prompt("", "", "hi").endswith("hi")
    assert header_of(build_prompt("", "", "hi")) == "[Text typed in an app]"
    assert header_of(build_prompt("x\ny", "t\nu", "hi")) == "[Text typed in: t u - x y]"  # no newline injected into the header


def test_custom_instructions_replace_the_built_in_ones():
    prompt = build_prompt("notepad.exe", "Notes", "Thanks, I will", instructions="Continue the text. Be brief.\nTEXT TO CONTINUE:")
    assert prompt.startswith("[SYSTEM MESSAGE]\n") and "Continue the text. Be brief." in prompt
    assert INSTRUCTIONS not in prompt
    assert prompt.endswith("[Text typed in: Notes - notepad]\n\nTEXT TO CONTINUE:\nThanks, I will")


def test_blank_instructions_mean_the_built_in_ones():
    assert build_prompt("notepad.exe", "Notes", "Hi", instructions="  \n") == build_prompt("notepad.exe", "Notes", "Hi")


SCREEN = "Sam: can you send the invoice for order 48213?\nPriya: I will check with billing"


def test_screen_text_comes_before_the_instructions_and_is_marked_as_background():
    prompt = build_prompt("chrome.exe", "Inbox", "Hi Sam, I will", screen=SCREEN)
    assert prompt.index("can you send the invoice") > prompt.index(INSTRUCTIONS)
    assert "<screen>" in prompt and "</screen>" in prompt
    assert "background" in prompt.split("<screen>")[0].lower()
    assert prompt.endswith("[Text typed in: Inbox - chrome]\n\nUntrusted text recognized locally by Windows OCR (background context only):\n<screen>\n" + SCREEN + "\n</screen>\n\nTEXT TO CONTINUE:\nHi Sam, I will")


def test_without_screen_text_the_prompt_is_unchanged():
    assert build_prompt("chrome.exe", "Inbox", "Hi", screen="") == build_prompt("chrome.exe", "Inbox", "Hi")
    assert build_prompt("chrome.exe", "Inbox", "Hi", screen="  \n ") == build_prompt("chrome.exe", "Inbox", "Hi")


def test_the_prompt_stays_stable_across_keystrokes_so_it_can_be_cached():
    a = build_prompt("chrome.exe", "Inbox", "Hi", screen=SCREEN)
    b = build_prompt("chrome.exe", "Inbox", "Hi S", screen=SCREEN)
    assert b.startswith(a[: -len("Hi")])


def test_lines_the_person_has_typed_are_taken_out_of_the_screen_text():
    typed = "Hi Sam, thanks for the reminder about\nthe invoice, I will send it over today"
    screen = f"{SCREEN}\nHi Sam, thanks for the reminder about\nthe invoice, I will send it over today"
    cleaned = screen_for_prompt(screen, typed)
    assert "reminder about" not in cleaned and "send it over" not in cleaned
    assert "can you send the invoice" in cleaned and "billing" in cleaned


def test_typed_text_is_matched_across_wrapping_and_case():
    assert screen_for_prompt("HI SAM,\nTHANKS FOR THE REMINDER", "hi sam, thanks for the reminder about it") == ""


def test_short_lines_are_kept_even_if_they_appear_in_the_typed_text():
    assert screen_for_prompt("Me\nOK", "Me and OK") == "Me\nOK"


def test_nothing_left_after_removing_typed_lines_means_no_block():
    assert "<screen>" not in build_prompt("chrome.exe", "Inbox", "Hi", screen=screen_for_prompt("Hi there friend", "Hi there friend"))
