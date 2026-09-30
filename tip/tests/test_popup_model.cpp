#include "../src/popup_model.h"
#include "test_harness.h"

using typer::Action;
using typer::Key;
using typer::Modifiers;
using typer::PopupModel;

namespace {
constexpr Modifiers kNone{};
constexpr Modifiers kCtrl{true, false, false};
constexpr Modifiers kAlt{false, true, false};
constexpr Modifiers kShift{false, false, true};
constexpr Key kAllKeys[] = {Key::Tab, Key::Up, Key::Down, Key::Escape, Key::Enter, Key::Other};

PopupModel OpenWith(std::size_t count) {
    PopupModel model;
    model.Open(count);
    return model;
}
}  // namespace

TEST(closed_popup_never_consumes_any_key) {
    PopupModel model;
    for (Key key : kAllKeys) {
        auto decision = model.OnKey(key, kNone);
        CHECK(!decision.consume);
        CHECK(decision.action == Action::None);
    }
}

TEST(opening_with_no_words_leaves_it_closed) {
    PopupModel model = OpenWith(0);
    CHECK(!model.visible());
    CHECK(!model.OnKey(Key::Tab, kNone).consume);
}

TEST(enter_is_never_consumed_in_any_state) {
    PopupModel model = OpenWith(3);
    CHECK(!model.OnKey(Key::Enter, kNone).consume);
    CHECK(model.visible());  // and it leaves the popup as it was

    model.MarkStale();
    CHECK(!model.OnKey(Key::Enter, kNone).consume);
    model.Close();
    CHECK(!model.OnKey(Key::Enter, kNone).consume);
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
    CHECK_EQ(model.highlight(), 1u);
    model.OnKey(Key::Down, kNone);
    model.OnKey(Key::Down, kNone);
    CHECK_EQ(model.highlight(), 0u);  // wrapped past the end

    model.OnKey(Key::Up, kNone);
    CHECK_EQ(model.highlight(), 2u);  // wrapped past the start
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
    CHECK_EQ(model.highlight(), 0u);
    model.OnKey(Key::Up, kNone);
    CHECK_EQ(model.highlight(), 0u);
}

TEST(escape_dismisses_and_closes) {
    PopupModel model = OpenWith(3);
    auto decision = model.OnKey(Key::Escape, kNone);
    CHECK(decision.consume);
    CHECK(decision.action == Action::Dismiss);
    CHECK(!model.visible());
}

TEST(other_keys_pass_through_an_open_popup) {
    PopupModel model = OpenWith(3);
    auto decision = model.OnKey(Key::Other, kNone);
    CHECK(!decision.consume);
    CHECK(model.visible());
}

TEST(no_key_is_consumed_when_a_modifier_is_held) {
    for (Modifiers mods : {kCtrl, kAlt, kShift, Modifiers{true, true, true}}) {
        for (Key key : kAllKeys) {
            PopupModel model = OpenWith(3);
            CHECK(!model.OnKey(key, mods).consume);
            CHECK(model.visible());
            CHECK_EQ(model.highlight(), 0u);
        }
    }
}

TEST(a_stale_popup_consumes_nothing_until_reopened) {
    PopupModel model = OpenWith(3);
    model.MarkStale();
    CHECK(model.stale());
    for (Key key : kAllKeys) CHECK(!model.OnKey(key, kNone).consume);

    model.Open(2);  // fresh words for the new text
    CHECK(!model.stale());
    CHECK(model.OnKey(Key::Tab, kNone).consume);
}

TEST(opening_again_resets_the_highlight) {
    PopupModel model = OpenWith(3);
    model.OnKey(Key::Down, kNone);
    model.Open(3);
    CHECK_EQ(model.highlight(), 0u);
}

TEST(peek_reports_the_decision_without_changing_state) {
    PopupModel model = OpenWith(3);
    auto peek = model.Peek(Key::Down, kNone);
    CHECK(peek.consume && peek.action == Action::MoveHighlight);
    CHECK_EQ(model.highlight(), 0u);

    CHECK(model.Peek(Key::Tab, kNone).consume);
    CHECK(model.visible());
    CHECK(!model.Peek(Key::Enter, kNone).consume);
}

TEST(peek_and_onkey_agree_for_every_key_and_state) {
    for (Key key : kAllKeys) {
        for (Modifiers mods : {kNone, kCtrl, kShift}) {
            for (int state = 0; state < 3; ++state) {  // closed, open, stale
                PopupModel model;
                if (state >= 1) model.Open(3);
                if (state == 2) model.MarkStale();
                auto peek = model.Peek(key, mods);
                auto real = model.OnKey(key, mods);
                CHECK_EQ(peek.consume, real.consume);
                CHECK(peek.action == real.action);
            }
        }
    }
}
