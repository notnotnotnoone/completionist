import pytest

from completionist_engine.hotkeys import MOD_ALT, MOD_CONTROL, MOD_NOREPEAT, MOD_SHIFT, MOD_WIN, GlobalHotkey, parse_hotkey


def test_letters_digits_and_function_keys_parse():
    assert parse_hotkey("ctrl+alt+p") == (MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, ord("P"))
    assert parse_hotkey("Ctrl+Shift+7") == (MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, ord("7"))
    assert parse_hotkey("alt+f9") == (MOD_ALT | MOD_NOREPEAT, 0x70 + 8)
    assert parse_hotkey("win+f24") == (MOD_WIN | MOD_NOREPEAT, 0x70 + 23)


def test_named_keys_parse():
    assert parse_hotkey("ctrl+alt+space")[1] == 0x20
    assert parse_hotkey("ctrl+alt+pause")[1] == 0x13


def test_spacing_and_case_do_not_matter():
    assert parse_hotkey(" CTRL + Alt + P ") == parse_hotkey("ctrl+alt+p")


@pytest.mark.parametrize("bad", ["", "p", "ctrl", "ctrl+", "ctrl+alt+", "ctrl+banana", "ctrl+p+q", "shift+a+b", "ctrl+ctrl+p", "+p"])
def test_bad_hotkeys_are_rejected(bad):
    with pytest.raises(ValueError):
        parse_hotkey(bad)


def test_a_hotkey_needs_a_modifier_so_it_cannot_swallow_plain_typing():
    with pytest.raises(ValueError, match="modifier"):
        parse_hotkey("p")
    with pytest.raises(ValueError, match="modifier"):
        parse_hotkey("f9")


def test_starting_and_stopping_a_global_hotkey_is_clean():
    hotkey = GlobalHotkey("ctrl+alt+shift+f23", lambda: None)
    assert hotkey.start() in (True, False)  # False only if another program already owns it
    hotkey.stop()
    hotkey.stop()  # stopping twice is fine


def test_a_hotkey_someone_else_owns_reports_failure_instead_of_crashing():
    first = GlobalHotkey("ctrl+alt+shift+f22", lambda: None)
    second = GlobalHotkey("ctrl+alt+shift+f22", lambda: None)
    started = first.start()
    try:
        if started:
            assert second.start() is False
    finally:
        second.stop()
        first.stop()
