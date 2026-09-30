// The popup's state and key rules, kept free of Windows and TSF so they can be tested natively.
//
// Rules (PRD stories 4-9):
//   * A closed popup never consumes a key, so Typer is invisible when it has nothing to offer.
//   * Tab accepts the highlighted word; Up/Down move the highlight (wrapping); Esc dismisses.
//   * Enter is never consumed, so it still sends messages and inserts newlines.
//   * Keys with Ctrl/Alt/Shift held are never consumed (Shift+Tab, Ctrl+Up and so on belong to the app).
//   * A popup whose words were computed for text that has since changed is "stale" and consumes nothing.
#pragma once

#include <cstddef>

namespace typer {

enum class Key { Tab, Up, Down, Escape, Enter, Other };

struct Modifiers {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool any() const { return ctrl || alt || shift; }
};

enum class Action { None, Accept, Dismiss, MoveHighlight };

struct KeyDecision {
    bool consume = false;
    Action action = Action::None;
    std::size_t index = 0;  // for Accept: which word to insert
};

class PopupModel {
public:
    // Show `count` words with the first highlighted. An empty list closes the popup.
    void Open(std::size_t count) {
        count_ = count;
        highlight_ = 0;
        stale_ = false;
    }

    void Close() {
        count_ = 0;
        highlight_ = 0;
        stale_ = false;
    }

    // The text changed since the words were computed. Keys pass through until the next Open().
    void MarkStale() { stale_ = true; }

    bool visible() const { return count_ > 0; }
    bool stale() const { return stale_; }
    std::size_t count() const { return count_; }
    std::size_t highlight() const { return highlight_; }

    // What OnKey() would decide, without changing anything (for TSF's OnTestKeyDown).
    KeyDecision Peek(Key key, Modifiers mods) const {
        PopupModel copy = *this;
        return copy.OnKey(key, mods);
    }

    KeyDecision OnKey(Key key, Modifiers mods) {
        if (!visible() || stale_ || mods.any()) return {};
        switch (key) {
            case Key::Tab: {
                KeyDecision decision{true, Action::Accept, highlight_};
                Close();
                return decision;
            }
            case Key::Down:
                highlight_ = (highlight_ + 1) % count_;
                return {true, Action::MoveHighlight, highlight_};
            case Key::Up:
                highlight_ = (highlight_ + count_ - 1) % count_;
                return {true, Action::MoveHighlight, highlight_};
            case Key::Escape:
                Close();
                return {true, Action::Dismiss, 0};
            case Key::Enter:
            case Key::Other:
                return {};
        }
        return {};
    }

private:
    std::size_t count_ = 0;
    std::size_t highlight_ = 0;
    bool stale_ = false;
};

}  // namespace typer
