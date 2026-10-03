#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace renderer::dock {

struct PresentationFrame {
    double connectionOpacity = 1.0;
    unsigned nextFrameMs = 0;
};

// Progress is the amount of expanded body currently visible: 0 collapsed, 1 expanded.
// Manual preference is session-only and always wins over geometry-driven auto collapse.
class State {
public:
    void SetReducedMotion(bool reduced, uint64_t nowMs) {
        reduced_ = reduced;
        if (reduced_) { progress_ = targetExpanded_ ? 1.0 : 0.0; opacity_ = progress_; moving_ = false; }
        else Sample(nowMs);
    }

    void Toggle(uint64_t nowMs) {
        Sample(nowMs);
        if (targetExpanded_) {
            manualMinimized_ = true;
            targetExpanded_ = false;
        } else {
            manualMinimized_ = false;
            targetExpanded_ = canFitExpanded_ && !overlapsMenu_;
        }
        Retarget(nowMs);
    }

    void SetExpanded(bool expanded,uint64_t nowMs) {
        Sample(nowMs);
        manualMinimized_=!expanded;
        targetExpanded_=expanded ? canFitExpanded_&&!overlapsMenu_ : false;
        Retarget(nowMs);
    }

    void SetGeometry(bool canFitExpandedBody, bool overlapsMenu, uint64_t nowMs) {
        Sample(nowMs);
        canFitExpanded_ = canFitExpandedBody;
        overlapsMenu_ = overlapsMenu;
        if (!manualMinimized_) {
            targetExpanded_ = canFitExpandedBody && !overlapsMenu;
            Retarget(nowMs);
        }
    }

    // Geometry and accessibility changes are immediate; appearance never animates.
    void Immediate(bool canFitExpandedBody, bool overlapsMenu, uint64_t nowMs) {
        canFitExpanded_ = canFitExpandedBody;
        overlapsMenu_ = overlapsMenu;
        if (!manualMinimized_) targetExpanded_ = canFitExpandedBody && !overlapsMenu;
        progress_ = targetExpanded_ ? 1.0 : 0.0;
        opacity_ = progress_;
        moving_ = false;
        startedAtMs_ = nowMs;
    }

    void CompleteImmediately(uint64_t nowMs) {
        progress_ = targetExpanded_ ? 1.0 : 0.0;
        opacity_ = progress_;
        moving_ = false;
        startedAtMs_ = nowMs;
    }

    double Sample(uint64_t nowMs) {
        if (!moving_) return progress_;
        const uint64_t elapsed = nowMs >= startedAtMs_ ? nowMs - startedAtMs_ : 0;
        const uint64_t duration = targetExpanded_ ? 220 : 220;
        const double linear = std::min(1.0, static_cast<double>(elapsed) / duration);
        const double eased = EaseOut(linear);
        progress_ = startProgress_ + (targetExpanded_ ? 1.0 - startProgress_ : -startProgress_) * eased;
        const double opacityAmount = EaseOut(std::min(1.0, static_cast<double>(elapsed) / 180.0));
        opacity_ = startOpacity_ + ((targetExpanded_ ? 1.0 : 0.0) - startOpacity_) * opacityAmount;
        if (linear >= 1.0) { progress_ = targetExpanded_ ? 1.0 : 0.0; moving_ = false; }
        if (elapsed >= 180) opacity_ = targetExpanded_ ? 1.0 : 0.0;
        return progress_;
    }

    bool TargetExpanded() const { return targetExpanded_; }
    bool ManuallyControlled() const { return manualMinimized_; }
    bool BodyHitTestable(uint64_t nowMs) { return Sample(nowMs) >= 1.0; }
    bool NeedsFrame() const { return moving_; }
    bool ReducedMotion() const { return reduced_; }
    PresentationFrame Frame(uint64_t nowMs, bool visible, bool connected) const {
        const bool pulseActive = visible && connected && !reduced_;
        return {ConnectionPulse(nowMs, visible, connected, reduced_),
                moving_ ? 16U : (pulseActive ? 33U : 0U)};
    }
    double Opacity(uint64_t nowMs) {
        Sample(nowMs);
        return opacity_;
    }

    static double ConnectionPulse(uint64_t nowMs, bool visible, bool connected, bool reducedMotion) {
        if (!visible || !connected || reducedMotion) return 1.0;
        constexpr double kPeriodMs = 2400.0;
        const double phase = (static_cast<double>(nowMs % 2400) / kPeriodMs) * 6.283185307179586;
        return 0.78 + 0.22 * (0.5 + 0.5 * std::sin(phase));
    }

private:
    static double EaseOut(double x) {
        // Cubic-bezier(0.22, 1, 0.36, 1), solved by bounded binary search for x(t).
        constexpr double x1 = .22, y1 = 1.0, x2 = .36, y2 = 1.0;
        double low = 0, high = 1, t = x;
        for (int i = 0; i < 24; ++i) {
            t = (low + high) * .5;
            const double u = 1 - t;
            const double curveX = 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t;
            if (curveX < x) low = t; else high = t;
        }
        const double u = 1 - t;
        return 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t;
    }

    void Retarget(uint64_t nowMs) {
        if (reduced_) { progress_ = targetExpanded_ ? 1.0 : 0.0; opacity_ = progress_; moving_ = false; return; }
        startProgress_ = progress_;
        startOpacity_ = opacity_;
        startedAtMs_ = nowMs;
        moving_ = (targetExpanded_ && progress_ < 1.0) || (!targetExpanded_ && progress_ > 0.0);
        if (!moving_) { progress_ = targetExpanded_ ? 1.0 : 0.0; return; }
    }

    double progress_ = 1.0;
    double startProgress_ = 1.0;
    double startOpacity_ = 1.0;
    double opacity_ = 1.0;
    uint64_t startedAtMs_ = 0;
    bool targetExpanded_ = true;
    bool manualMinimized_ = false;
    bool canFitExpanded_ = true;
    bool overlapsMenu_ = false;
    bool reduced_ = false;
    bool moving_ = false;
};

}  // namespace renderer::dock
