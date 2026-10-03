#pragma once

namespace renderer {

enum class MaterialMode { Hidden, Opaque, Glass };
struct MaterialConditions {
    bool visible = false;
    bool locked = false;
    bool highContrast = false;
    bool transparencyEnabled = true;
    bool supportedHdr = false;
    bool supportedSession = true;
    bool captureAvailable = true;
    bool windowsExcluded = true;
};

constexpr MaterialMode ChooseMaterialMode(const MaterialConditions& c) {
    if (!c.visible || c.locked) return MaterialMode::Hidden;
    if (c.highContrast || !c.transparencyEnabled || c.supportedHdr || !c.supportedSession ||
        !c.captureAvailable || !c.windowsExcluded) return MaterialMode::Opaque;
    return MaterialMode::Glass;
}

// Shared transition used by rendering and policy tests. Capture resources are retired
// before an opaque/hidden state can be exposed to the user.
template<class Retire>
MaterialMode ApplyMaterialMode(MaterialMode current, const MaterialConditions& conditions, Retire retire) {
    const MaterialMode next = ChooseMaterialMode(conditions);
    if (current == MaterialMode::Glass && next != MaterialMode::Glass) retire();
    return next;
}

constexpr bool SupportsLiveWindowsBuild(unsigned major, unsigned minor, unsigned build) {
    return major > 10 || (major == 10 && (minor > 0 || (minor == 0 && build >= 19041)));
}

}  // namespace renderer
