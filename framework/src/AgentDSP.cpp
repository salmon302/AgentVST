#include "AgentDSP.h"
#include "ParameterCache.h"

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// DSPContext::getParameter
// ─────────────────────────────────────────────────────────────────────────────

float DSPContext::getParameter(const std::string& paramId) const noexcept {
    if (_paramCache == nullptr)
        return 0.0f;

    const auto* cache = static_cast<const ParameterCache*>(_paramCache);

    if (_paramSnapshotValues != nullptr) {
        std::size_t index = 0;
        if (cache->tryGetIndex(paramId, index) && index < _paramSnapshotCount)
            return _paramSnapshotValues[index];
    }

    return cache->getValue(paramId);
}

} // namespace AgentVST
