// The popup's state and key rules, kept free of Windows and TSF so they can be tested natively.
//
// The popup shows an optional phrase row on top of the word rows. Rules (PRD stories 4-9, 18-21, 25):
//   * A closed popup never consumes a key (except the phrase hotkey, when phrases are available).
//   * Tab accepts the highlighted row: a word, or the whole phrase. The phrase row is highlighted by
//     default when present, but only 150 ms after it appears, so a phrase that arrives just as Tab is
//     pressed can't steal it: that Tab still takes the word that was highlighted before.
//   * Next-word rows (offered after a space) open with nothing highlighted, so Tab passes through to the
//     app until Up or Down highlights a row. Enter is never consumed either way.
//   * Up/Down move the highlight through the rows, wrapping. The configured dismiss shortcut closes
//     the popup (Esc by default). Partial accept takes the next phrase word (Ctrl+Right by default).
//     Ctrl+Space asks for a phrase.
//   * Enter is never consumed, so it still sends messages and inserts newlines.
//   * Every other key with Ctrl/Alt/Shift held belongs to the app.
//   * A popup whose words were computed for text that has since changed is "stale" and consumes nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include "popup_settings.h"

namespace completionist {

enum class Key { Tab, Up, Down, Left, Right, Space, Escape, Enter, Backspace, Other };

struct Modifiers {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool any() const { return ctrl || alt || shift; }
    bool only_ctrl() const { return ctrl && !alt && !shift; }
    bool only_alt() const { return alt && !ctrl && !shift; }
};

enum class Action { None, Accept, AcceptPhrase, AcceptPhraseWord, RequestPhrase, Dismiss, MoveHighlight };

struct KeyDecision {
    bool consume = false;
    Action action = Action::None;
    std::size_t index = 0;  // for Accept: which word to insert
};

class PopupModel {
public:
    static constexpr std::uint64_t kNoStealMs = 150;
    static constexpr int kPhraseRow = -1;  // "selection" value meaning the phrase row
    static constexpr int kNoRow = -2;      // "selection" value meaning nothing is highlighted

    // Show `count` words (0 words is fine when there's a phrase). The first is highlighted, unless
    // `highlightFirst` is false: next-word rows offered after a space start with nothing highlighted,
    // so Tab is still the app's until the writer presses Down or Up.
    void Open(std::size_t count, bool highlightFirst = true) {
        count_ = count;
        selection_ = highlightFirst ? 0 : kNoRow;
        moved_ = false;
        stale_ = false;
    }

    // The phrase row appears, changes or goes away. It's armed (highlighted by default) after kNoStealMs.
    void SetPhrase(bool present, std::uint64_t nowMs) {
        if (present && !phrase_) armedAt_ = nowMs + kNoStealMs;
        if (!present && phrase_ && selection_ == kPhraseRow) selection_ = 0;
        phrase_ = present;
    }

    void Close() {
        count_ = 0;
        selection_ = 0;
        moved_ = false;
        stale_ = false;
        phrase_ = false;
    }

    // Whether Ctrl+Space should be handled: phrases are switched on in this field.
    void SetPhraseAvailable(bool available) { available_ = available; }
    void SetSettings(const PopupSettings& settings) { settings_ = settings; }

    // The text changed since the words were computed. Keys pass through until the next Open().
    // Keep the previous presentation only until fresh words arrive, with a deadline
    // that repeated edits cannot extend. Stale rows never consume an accept key.
    void MarkStale(std::uint64_t nowMs = 0) {
        if (!stale_) staleDeadline_ = nowMs + 250;
        stale_ = true;
    }
    std::uint64_t stale_deadline() const { return stale_ ? staleDeadline_ : 0; }

    bool visible() const { return count_ > 0 || phrase_; }
    bool stale() const { return stale_; }
    bool has_phrase() const { return phrase_; }
    std::size_t count() const { return count_; }

    // The highlighted row at `nowMs`: kPhraseRow, or a word index.
    int selection(std::uint64_t nowMs) const {
        if (!visible()) return kNoRow;
        if (moved_ || !phrase_) return count_ == 0 && phrase_ ? kPhraseRow : selection_;
        if (count_ == 0) return kPhraseRow;
        return nowMs >= armedAt_ ? kPhraseRow : selection_;
    }

    // When the phrase row becomes highlighted by default (0 if it's not waiting on that).
    std::uint64_t armed_at(std::uint64_t nowMs) const {
        return phrase_ && !moved_ && count_ > 0 && nowMs < armedAt_ ? armedAt_ : 0;
    }

    // What OnKey() would decide, without changing anything (for TSF's OnTestKeyDown).
    KeyDecision Peek(Key key, Modifiers mods, std::uint64_t nowMs = 0) const {
        PopupModel copy = *this;
        return copy.OnKey(key, mods, nowMs);
    }

    KeyDecision OnKey(Key key, Modifiers mods, std::uint64_t nowMs = 0) {
        if (key == Key::Space && mods.only_ctrl()) {
            return available_ ? KeyDecision{true, Action::RequestPhrase, 0} : KeyDecision{};
        }
        if (!visible() || stale_) return {};
        bool partial = settings_.partial_accept == PartialAccept::CtrlRight ? key == Key::Right && mods.only_ctrl()
                     : settings_.partial_accept == PartialAccept::AltRight ? key == Key::Right && mods.only_alt()
                     : key == Key::Tab && mods.only_ctrl();
        if (partial) {
            return phrase_ ? KeyDecision{true, Action::AcceptPhraseWord, 0} : KeyDecision{};
        }
        bool dismiss = settings_.dismiss == DismissShortcut::Escape ? key == Key::Escape && !mods.any()
                     : settings_.dismiss == DismissShortcut::CtrlBackspace ? key == Key::Backspace && mods.only_ctrl()
                     : key == Key::Backspace && mods.only_alt();
        if (dismiss) {
            Close();
            return {true, Action::Dismiss, 0};
        }
        if (mods.any()) return {};
        switch (key) {
            case Key::Tab: {
                int row = selection(nowMs);
                if (row == kNoRow) return {};  // nothing highlighted: Tab belongs to the app
                KeyDecision decision = row == kPhraseRow ? KeyDecision{true, Action::AcceptPhrase, 0}
                                                         : KeyDecision{true, Action::Accept, static_cast<std::size_t>(row)};
                Close();
                return decision;
            }
            case Key::Down:
                Move(+1, nowMs);
                return {true, Action::MoveHighlight, 0};
            case Key::Up:
                Move(-1, nowMs);
                return {true, Action::MoveHighlight, 0};
            default:
                return {};
        }
    }

private:
    // Rows top to bottom: the phrase (if any), then the words. Moving wraps around.
    void Move(int delta, std::uint64_t nowMs) {
        int rows = static_cast<int>(count_) + (phrase_ ? 1 : 0);
        int current = selection(nowMs);
        int position = phrase_ ? (current == kPhraseRow ? 0 : current + 1) : current;
        if (current == kNoRow) position = delta > 0 ? -1 : 0;  // from nothing: Down is the top row, Up the bottom
        position = (position + delta + rows) % rows;
        selection_ = phrase_ ? position - 1 : position;  // position 0 is the phrase row when there is one
        moved_ = true;
    }

    std::size_t count_ = 0;
    PopupSettings settings_;
    int selection_ = 0;
    bool moved_ = false;
    bool stale_ = false;
    std::uint64_t staleDeadline_ = 0;
    bool phrase_ = false;
    bool available_ = false;
    std::uint64_t armedAt_ = 0;
};

}  // namespace completionist
