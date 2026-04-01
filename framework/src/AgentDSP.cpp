#include "AgentDSP.h"
#include "ParameterCache.h"
#include <stdexcept>

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// Global DSP factory storage
// ─────────────────────────────────────────────────────────────────────────────

static DSPFactory gRegisteredFactory;

void registerDSP(DSPFactory factory) {
    if (gRegisteredFactory) {
        // A second call means two AGENTVST_REGISTER_DSP macros fired —
        // likely a linker issue where two DSP .cpp files were compiled in.
        // This is a hard error at startup, not a runtime fault.
        throw std::logic_error(
            "AgentVST: registerDSP() called more than once. "
            "Only one AGENTVST_REGISTER_DSP() macro is allowed per plugin.");
    }
    gRegisteredFactory = std::move(factory);
}

DSPFactory& getRegisteredDSPFactory() {
    return gRegisteredFactory;
}

// ─────────────────────────────────────────────────────────────────────────────
// DSPContext::getParameter
// ─────────────────────────────────────────────────────────────────────────────

float DSPContext::getParameter(const std::string& paramId) const noexcept {
    if (_paramCache == nullptr)
        return 0.0f;
    const auto* cache = static_cast<const ParameterCache*>(_paramCache);
    return cache->getValue(paramId);
}

} // namespace AgentVST
