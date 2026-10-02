#include "../src/popup_model.h"
#include "test_harness.h"

using completionist::Action;
using completionist::Key;
using completionist::Modifiers;
using completionist::PopupModel;

namespace {
constexpr Modifiers kNone{};
constexpr Modifiers kCtrl{true, false, false};
constexpr Modifiers kAlt{false, true, false};
constexpr Modifiers kShift{false, false, true};
constexpr Key kAllKeys[] = {Key::Tab, Key::Up, Key::Down, Key::Left, Key::Right, Key::Space, Key::Escape, Key::Enter, Key::Backspace, Key::Other};
constexpr auto kPhrase = PopupModel::kPhraseRow;

PopupModel OpenWith(std::size_t count) {
    PopupModel model;
    model.Open(count);
    return model;
}

// Words plus a phrase that appeared at t=0 (so it's highlighted from t=150).
PopupModel WithPhrase(std::size_t words) {
    PopupModel model = OpenWith(words);
    model.SetPhrase(true, 0);
    return model;
}
}  // namespace

// ---- words only -----------------------------------------------------------------------------

TEST(closed_popup_never_consumes_any_key) {
    PopupModel model;
    for (Key key : kAllKeys) {
        auto decision = model.OnKey(key, kNone);
        CHECK(!decision.consume);
        CHECK(decision.action == Action::None);
    }
}

TEST(opening_with_no_words_and_no_phrase_leaves_it_closed) {
    PopupModel model = OpenWith(0);
    CHECK(!model.visible());
    CHECK(!model.OnKey(Key::Tab, kNone).consume);
}

TEST(enter_is_never_consumed_in_any_state) {
    PopupModel model = WithPhrase(3);
    model.SetPhraseAvailable(true);
    CHECK(!model.OnKey(Key::Enter, kNone, 1000).consume);
    CHECK(model.visible());  // and it leaves the popup as it was

    model.MarkStale();
    CHECK(!model.OnKey(Key::Enter, kNone, 1000).consume);
    model.Close();
    CHECK(!model.OnKey(Key::Enter, kNone, 1000).consume);
}

TEST(tab_accepts_the_first_word_by_default_and_closes) {
    PopupModel model = OpenWith(4);
    auto decision = model.OnKey(Key::Tab, kNone);
    CHECK(decision.consume);
    CHECK(decision.action == Action::Accept);
    CHECK_EQ(decision.index, 0u);
    CHECK(!model.visible());
}

TEST(down_and_up_move_the_highlight_and_wrap) {
    PopupModel model = OpenWith(3);
    auto d = model.OnKey(Key::Down, kNone);
    CHECK(d.consume && d.action == Action::MoveHighlight);
    CHECK_EQ(model.selection(0), 1);
    model.OnKey(Key::Down, kNone);
    model.OnKey(Key::Down, kNone);
    CHECK_EQ(model.selection(0), 0);  // wrapped past the end

    model.OnKey(Key::Up, kNone);
    CHECK_EQ(model.selection(0), 2);  // wrapped past the start
}

TEST(tab_accepts_the_highlighted_word) {
    PopupModel model = OpenWith(3);
    model.OnKey(Key::Down, kNone);
    model.OnKey(Key::Down, kNone);
    auto decision = model.OnKey(Key::Tab, kNone);
    CHECK(decision.action == Action::Accept);
    CHECK_EQ(decision.index, 2u);
}

TEST(a_single_word_list_wraps_onto_itself) {
    PopupModel model = OpenWith(1);
    model.OnKey(Key::Down, kNone);
    CHECK_EQ(model.selection(0), 0);
    model.OnKey(Key::Up, kNone);
    CHECK_EQ(model.selection(0), 0);
}

TEST(escape_dismisses_and_closes) {
    PopupModel model = WithPhrase(3);
    auto decision = model.OnKey(Key::Escape, kNone);
    CHECK(decision.consume);
    CHECK(decision.action == Action::Dismiss);
    CHECK(!model.visible());
    CHECK(!model.has_phrase());
}

TEST(other_keys_pass_through_an_open_popup) {
    PopupModel model = OpenWith(3);
    for (Key key : {Key::Other, Key::Left, Key::Right, Key::Space}) {
        CHECK(!model.OnKey(key, kNone).consume);
    }
    CHECK(model.visible());
}

TEST(no_key_is_consumed_when_a_modifier_is_held) {
    for (Modifiers mods : {kCtrl, kAlt, kShift, Modifiers{true, true, true}}) {
        for (Key key : kAllKeys) {
            PopupModel model = OpenWith(3);  // no phrase, and the hotkey isn't available
            CHECK(!model.OnKey(key, mods).consume);
            CHECK(model.visible());
            CHECK_EQ(model.selection(0), 0);
        }
    }
}

TEST(a_stale_popup_consumes_nothing_until_reopened) {
    PopupModel model = WithPhrase(3);
    model.MarkStale();
    CHECK(model.stale());
    for (Key key : kAllKeys) CHECK(!model.OnKey(key, kNone, 1000).consume);
    CHECK(!model.OnKey(Key::Right, kCtrl, 1000).consume);

    model.Open(2);  // fresh words for the new text
    CHECK(!model.stale());
    CHECK(model.OnKey(Key::Tab, kNone).consume);
}

TEST(opening_again_resets_the_highlight) {
    PopupModel model = OpenWith(3);
    model.OnKey(Key::Down, kNone);
    model.Open(3);
    CHECK_EQ(model.selection(0), 0);
}

TEST(peek_reports_the_decision_without_changing_state) {
    PopupModel model = OpenWith(3);
    auto peek = model.Peek(Key::Down, kNone);
    CHECK(peek.consume && peek.action == Action::MoveHighlight);
    CHECK_EQ(model.selection(0), 0);

    CHECK(model.Peek(Key::Tab, kNone).consume);
    CHECK(model.visible());
    CHECK(!model.Peek(Key::Enter, kNone).consume);
}

TEST(peek_and_onkey_agree_for_every_key_and_state) {
    for (Key key : kAllKeys) {
        for (Modifiers mods : {kNone, kCtrl, kShift}) {
            for (int state = 0; state < 4; ++state) {  // closed, open, stale, open with phrase + hotkey
                PopupModel model;
                if (state >= 1) model.Open(3);
                if (state == 2) model.MarkStale();
                if (state == 3) {
                    model.SetPhrase(true, 0);
                    model.SetPhraseAvailable(true);
                }
                auto peek = model.Peek(key, mods, 500);
                auto real = model.OnKey(key, mods, 500);
                CHECK_EQ(peek.consume, real.consume);
                CHECK(peek.action == real.action);
            }
        }
    }
}

// ---- the phrase row ---------------------------------------------------------------------------

TEST(a_phrase_row_is_highlighted_by_default_once_it_has_been_there_150_ms) {
    PopupModel model = WithPhrase(3);  // appeared at t=0
    CHECK_EQ(model.selection(200), kPhrase);
    auto decision = model.OnKey(Key::Tab, kNone, 200);
    CHECK(decision.consume && decision.action == Action::AcceptPhrase);
    CHECK(!model.visible());
}

TEST(a_tab_within_150_ms_of_the_phrase_appearing_takes_the_word_not_the_phrase) {
    PopupModel model = WithPhrase(3);
    CHECK_EQ(model.selection(100), 0);  // still the first word
    auto decision = model.OnKey(Key::Tab, kNone, 149);
    CHECK(decision.action == Action::Accept);
    CHECK_EQ(decision.index, 0u);
}

TEST(the_no_steal_window_keeps_the_word_the_writer_had_moved_to) {
    PopupModel model = OpenWith(3);
    model.OnKey(Key::Down, kNone, 0);  // on the second word
    model.SetPhrase(true, 1000);       // a phrase arrives
    CHECK_EQ(model.selection(1100), 1);
    auto decision = model.OnKey(Key::Tab, kNone, 1100);
    CHECK(decision.action == Action::Accept && decision.index == 1);
}

TEST(a_phrase_that_arrives_after_the_writer_navigated_never_takes_the_highlight) {
    PopupModel model = OpenWith(3);
    model.OnKey(Key::Down, kNone, 0);
    model.SetPhrase(true, 1000);
    CHECK_EQ(model.selection(5000), 1);  // they chose a word; the phrase doesn't override that
}

TEST(a_phrase_that_updates_while_streaming_does_not_restart_the_150_ms) {
    PopupModel model = OpenWith(2);
    model.SetPhrase(true, 1000);
    model.SetPhrase(true, 1100);  // more text arrived
    model.SetPhrase(true, 1140);
    CHECK_EQ(model.selection(1150), kPhrase);  // 150 ms after the first appearance
}

TEST(a_phrase_that_goes_away_and_comes_back_is_armed_again_from_scratch) {
    PopupModel model = OpenWith(2);
    model.SetPhrase(true, 0);
    model.SetPhrase(false, 500);
    model.SetPhrase(true, 600);
    CHECK_EQ(model.selection(700), 0);
    CHECK_EQ(model.selection(760), kPhrase);
}

TEST(armed_at_says_when_the_highlight_will_move_to_the_phrase) {
    PopupModel model = OpenWith(2);
    model.SetPhrase(true, 1000);
    CHECK_EQ(model.armed_at(1050), 1150u);
    CHECK_EQ(model.armed_at(1200), 0u);  // already armed
    model.OnKey(Key::Down, kNone, 1060);
    CHECK_EQ(model.armed_at(1070), 0u);  // the writer chose; nothing pending
}

TEST(a_phrase_with_no_words_is_a_popup_on_its_own_and_is_highlighted_at_once) {
    PopupModel model = OpenWith(0);
    model.SetPhrase(true, 1000);
    CHECK(model.visible());
    CHECK_EQ(model.selection(1000), kPhrase);
    CHECK(model.OnKey(Key::Tab, kNone, 1000).action == Action::AcceptPhrase);
}

TEST(up_from_the_first_word_goes_to_the_phrase_and_down_from_the_phrase_to_the_first_word) {
    PopupModel model = WithPhrase(2);
    model.OnKey(Key::Down, kNone, 200);  // phrase -> first word
    CHECK_EQ(model.selection(200), 0);
    model.OnKey(Key::Up, kNone, 200);  // first word -> phrase
    CHECK_EQ(model.selection(200), kPhrase);
    model.OnKey(Key::Up, kNone, 200);  // phrase -> last word (wraps)
    CHECK_EQ(model.selection(200), 1);
    model.OnKey(Key::Down, kNone, 200);  // last word -> phrase (wraps)
    CHECK_EQ(model.selection(200), kPhrase);
}

TEST(after_navigating_tab_takes_whatever_is_highlighted) {
    PopupModel model = WithPhrase(3);
    model.OnKey(Key::Down, kNone, 200);
    model.OnKey(Key::Down, kNone, 200);  // second word
    auto decision = model.OnKey(Key::Tab, kNone, 200);
    CHECK(decision.action == Action::Accept && decision.index == 1);
}

TEST(losing_the_phrase_while_it_is_highlighted_falls_back_to_the_first_word) {
    PopupModel model = WithPhrase(3);
    model.OnKey(Key::Up, kNone, 200);    // phrase -> last word
    model.OnKey(Key::Down, kNone, 200);  // last word -> back to the phrase, chosen on purpose
    CHECK_EQ(model.selection(200), kPhrase);
    model.SetPhrase(false, 300);
    CHECK_EQ(model.selection(300), 0);
    CHECK(model.OnKey(Key::Tab, kNone, 300).action == Action::Accept);
}

TEST(ctrl_right_takes_the_next_phrase_word_and_keeps_the_popup_open) {
    PopupModel model = WithPhrase(3);
    auto decision = model.OnKey(Key::Right, kCtrl, 200);
    CHECK(decision.consume && decision.action == Action::AcceptPhraseWord);
    CHECK(model.visible());
}

TEST(ctrl_right_works_even_inside_the_no_steal_window) {
    PopupModel model = WithPhrase(3);
    CHECK(model.OnKey(Key::Right, kCtrl, 10).action == Action::AcceptPhraseWord);
}

TEST(ctrl_right_without_a_phrase_or_with_other_modifiers_belongs_to_the_app) {
    PopupModel plain = OpenWith(3);
    CHECK(!plain.OnKey(Key::Right, kCtrl).consume);
    PopupModel phrase = WithPhrase(3);
    CHECK(!phrase.OnKey(Key::Right, Modifiers{true, false, true}, 200).consume);  // Ctrl+Shift+Right selects words
    CHECK(!phrase.OnKey(Key::Right, kNone, 200).consume);  // plain Right just moves the caret
    CHECK(!phrase.OnKey(Key::Left, kCtrl, 200).consume);
}

TEST(ctrl_space_asks_for_a_phrase_even_when_the_popup_is_closed) {
    PopupModel model;
    model.SetPhraseAvailable(true);
    auto decision = model.OnKey(Key::Space, kCtrl);
    CHECK(decision.consume && decision.action == Action::RequestPhrase);
    CHECK(!model.visible());
}

TEST(ctrl_space_is_left_to_the_app_when_phrases_are_off_in_this_field) {
    PopupModel model;
    model.SetPhraseAvailable(false);
    CHECK(!model.OnKey(Key::Space, kCtrl).consume);
    PopupModel open = OpenWith(3);
    CHECK(!open.OnKey(Key::Space, kCtrl).consume);
}

TEST(ctrl_space_with_other_modifiers_or_plain_space_is_not_the_hotkey) {
    PopupModel model = OpenWith(2);
    model.SetPhraseAvailable(true);
    CHECK(!model.OnKey(Key::Space, kNone).consume);
    CHECK(!model.OnKey(Key::Space, Modifiers{true, false, true}).consume);
    CHECK(!model.OnKey(Key::Space, Modifiers{true, true, false}).consume);
    CHECK(!model.OnKey(Key::Space, kAlt).consume);
}

TEST(ctrl_space_works_while_the_popup_is_stale_because_it_asks_about_the_text_now) {
    PopupModel model = WithPhrase(3);
    model.SetPhraseAvailable(true);
    model.MarkStale();
    CHECK(model.OnKey(Key::Space, kCtrl, 200).action == Action::RequestPhrase);
}

TEST(close_forgets_the_phrase_but_keeps_the_hotkey_setting) {
    PopupModel model = WithPhrase(2);
    model.SetPhraseAvailable(true);
    model.Close();
    CHECK(!model.has_phrase());
    CHECK(model.OnKey(Key::Space, kCtrl).consume);
}

// ---- next words: offered after a space, with nothing highlighted --------------------------------

namespace {
PopupModel NextWords(std::size_t count) {
    PopupModel model;
    model.Open(count, /*highlightFirst=*/false);
    return model;
}
constexpr auto kNoRow = PopupModel::kNoRow;
}  // namespace

TEST(next_words_open_with_nothing_highlighted_so_tab_and_enter_pass_through) {
    PopupModel model = NextWords(3);
    CHECK(model.visible());
    CHECK_EQ(model.selection(1000), kNoRow);
    auto tab = model.OnKey(Key::Tab, kNone, 1000);
    CHECK(!tab.consume);
    CHECK(tab.action == Action::None);
    CHECK(!model.OnKey(Key::Enter, kNone, 1000).consume);
    CHECK(model.visible());  // the popup stays until the text changes
    CHECK_EQ(model.selection(1000), kNoRow);
}

TEST(down_highlights_the_first_next_word_and_then_tab_takes_it) {
    PopupModel model = NextWords(3);
    auto down = model.OnKey(Key::Down, kNone, 1000);
    CHECK(down.consume && down.action == Action::MoveHighlight);
    CHECK_EQ(model.selection(1000), 0);
    auto tab = model.OnKey(Key::Tab, kNone, 1000);
    CHECK(tab.consume && tab.action == Action::Accept);
    CHECK_EQ(tab.index, 0u);
    CHECK(!model.visible());
}

TEST(up_with_nothing_highlighted_goes_to_the_last_next_word) {
    PopupModel model = NextWords(3);
    CHECK(model.OnKey(Key::Up, kNone, 1000).consume);
    CHECK_EQ(model.selection(1000), 2);
    auto tab = model.OnKey(Key::Tab, kNone, 1000);
    CHECK(tab.consume && tab.index == 2u);
}

TEST(the_highlight_then_wraps_like_any_list) {
    PopupModel model = NextWords(2);
    model.OnKey(Key::Down, kNone, 1000);
    model.OnKey(Key::Down, kNone, 1000);
    CHECK_EQ(model.selection(1000), 1);
    model.OnKey(Key::Down, kNone, 1000);
    CHECK_EQ(model.selection(1000), 0);
}

TEST(enter_is_never_consumed_for_next_words_even_after_moving) {
    PopupModel model = NextWords(3);
    model.OnKey(Key::Down, kNone, 1000);
    CHECK(!model.OnKey(Key::Enter, kNone, 1000).consume);
    CHECK(model.visible());
}

TEST(escape_dismisses_next_words) {
    PopupModel model = NextWords(3);
    auto esc = model.OnKey(Key::Escape, kNone, 1000);
    CHECK(esc.consume && esc.action == Action::Dismiss);
    CHECK(!model.visible());
}

TEST(other_keys_and_modified_keys_pass_through_next_words) {
    PopupModel model = NextWords(3);
    for (Key key : {Key::Left, Key::Right, Key::Space, Key::Enter, Key::Other}) CHECK(!model.OnKey(key, kNone, 1000).consume);
    for (Key key : kAllKeys) {
        if (key == Key::Space) continue;  // Ctrl+Space is the phrase hotkey, tested elsewhere
        CHECK(!model.OnKey(key, kCtrl, 1000).consume);
        CHECK(!model.OnKey(key, kAlt, 1000).consume);
        CHECK(!model.OnKey(key, kShift, 1000).consume);
    }
}

TEST(a_stale_next_word_popup_consumes_nothing) {
    PopupModel model = NextWords(3);
    model.OnKey(Key::Down, kNone, 1000);
    model.MarkStale();
    for (Key key : kAllKeys) CHECK(!model.OnKey(key, kNone, 1000).consume);
}

TEST(a_phrase_above_next_words_takes_tab_only_once_it_is_armed) {
    PopupModel model = NextWords(3);
    model.SetPhrase(true, 0);
    CHECK(!model.OnKey(Key::Tab, kNone, 100).consume);  // inside the 150 ms no-steal window: Tab is the app's
    CHECK(model.visible());
    auto tab = model.OnKey(Key::Tab, kNone, 200);
    CHECK(tab.consume && tab.action == Action::AcceptPhrase);
}

TEST(a_next_word_popup_can_be_navigated_up_to_the_phrase) {
    PopupModel model = NextWords(2);
    model.SetPhrase(true, 0);
    model.OnKey(Key::Down, kNone, 100);  // nothing highlighted yet: Down goes to the top row, the phrase
    CHECK_EQ(model.selection(100), kPhrase);
    model.OnKey(Key::Down, kNone, 100);
    CHECK_EQ(model.selection(100), 0);
}

TEST(opening_normally_highlights_the_first_word_again) {
    PopupModel model = NextWords(3);
    model.Open(3);
    CHECK_EQ(model.selection(1000), 0);
    CHECK(model.OnKey(Key::Tab, kNone, 1000).consume);
}

TEST(peek_and_onkey_agree_for_next_words_with_and_without_a_phrase) {
    for (bool phrase : {false, true}) {
        for (bool moved : {false, true}) {
            for (Key key : kAllKeys) {
                for (std::uint64_t now : {std::uint64_t{50}, std::uint64_t{500}}) {
                    PopupModel model = NextWords(3);
                    if (phrase) model.SetPhrase(true, 0);
                    if (moved) model.OnKey(Key::Down, kNone, now);
                    auto peeked = model.Peek(key, kNone, now);
                    auto acted = model.OnKey(key, kNone, now);
                    CHECK_EQ(peeked.consume, acted.consume);
                    CHECK(peeked.action == acted.action);
                    CHECK_EQ(peeked.index, acted.index);
                }
            }
        }
    }
}

TEST(alternate_partial_shortcuts_preserve_normal_tab_and_hotkey) {
    for (auto shortcut : {completionist::PartialAccept::AltRight, completionist::PartialAccept::CtrlTab}) {
        auto model = WithPhrase(2);
        completionist::PopupSettings settings;
        settings.partial_accept = shortcut;
        model.SetSettings(settings);
        const Key key = shortcut == completionist::PartialAccept::AltRight ? Key::Right : Key::Tab;
        const Modifiers mods = shortcut == completionist::PartialAccept::AltRight ? kAlt : kCtrl;
        CHECK_EQ(model.Peek(key, mods, 200).action, Action::AcceptPhraseWord);
        CHECK(!model.Peek(Key::Right, kCtrl, 200).consume);
        CHECK(!model.Peek(key, {mods.ctrl, mods.alt, true}, 200).consume);
        CHECK_EQ(model.Peek(Key::Tab, kNone, 200).action, Action::AcceptPhrase);
        model.MarkStale();
        CHECK(!model.Peek(key, mods, 200).consume);
        model.Close();
        CHECK(!model.Peek(key, mods, 200).consume);
        model.SetPhraseAvailable(true);
        CHECK_EQ(model.Peek(Key::Space, kCtrl).action, Action::RequestPhrase);
        model.Open(2);
        CHECK(!model.Peek(key, mods).consume);
    }
}

TEST(alternate_dismiss_shortcuts_only_dismiss_live_popup) {
    for (auto shortcut : {completionist::DismissShortcut::CtrlBackspace, completionist::DismissShortcut::AltBackspace}) {
        auto model = OpenWith(2);
        completionist::PopupSettings settings;
        settings.dismiss = shortcut;
        model.SetSettings(settings);
        const Modifiers mods = shortcut == completionist::DismissShortcut::CtrlBackspace ? kCtrl : kAlt;
        CHECK(!model.Peek(Key::Escape, kNone).consume);
        CHECK(!model.Peek(Key::Backspace, kNone).consume);
        CHECK(!model.Peek(Key::Backspace, {mods.ctrl, mods.alt, true}).consume);
        model.MarkStale();
        CHECK(!model.Peek(Key::Backspace, mods).consume);
        model.Open(2);
        CHECK_EQ(model.OnKey(Key::Backspace, mods).action, Action::Dismiss);
        CHECK(!model.visible());
        CHECK(!model.Peek(Key::Backspace, mods).consume);
    }
}

TEST(shortcut_preferences_survive_close_and_can_reset_to_defaults) {
    auto model = WithPhrase(2);
    completionist::PopupSettings settings;
    settings.partial_accept = completionist::PartialAccept::AltRight;
    settings.dismiss = completionist::DismissShortcut::CtrlBackspace;
    model.SetSettings(settings);
    model.Close();
    model.Open(2);
    model.SetPhrase(true, 0);
    CHECK_EQ(model.Peek(Key::Right, kAlt).action, Action::AcceptPhraseWord);
    CHECK_EQ(model.Peek(Key::Backspace, kCtrl).action, Action::Dismiss);
    model.SetSettings({});
    CHECK(!model.Peek(Key::Right, kAlt).consume);
    CHECK(!model.Peek(Key::Backspace, kCtrl).consume);
    CHECK_EQ(model.Peek(Key::Right, kCtrl).action, Action::AcceptPhraseWord);
    CHECK_EQ(model.Peek(Key::Escape, kNone).action, Action::Dismiss);
}
