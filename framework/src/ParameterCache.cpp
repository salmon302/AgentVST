#include "ParameterCache.h"

#include <algorithm>
#include <cmath>

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// Denormalization
//
// JUCE's APVTS stores all parameters as normalized floats in [0.0, 1.0].
// For a parameter with range [min, max] and optional skew factor:
//   JUCE normalizes:  norm = pow((value - min) / (max - min), 1/skew)
//   We denormalize:   value = min + (max - min) * pow(norm, skew)
//
// When skew == 0 or skew == 1, the relationship is linear.
// ─────────────────────────────────────────────────────────────────────────────

float ParameterCache::denormalize(float normalized, float min, float max,
                                   float skew, bool isBool) noexcept {
    if (isBool)
        return normalized >= 0.5f ? 1.0f : 0.0f;

    // Clamp to valid range
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    if (max <= min)
        return min;

    // No skew or unit skew → linear mapping
    if (skew <= 0.0f || std::abs(skew - 1.0f) < 1e-6f)
        return min + normalized * (max - min);

    // Skewed mapping: inverse of JUCE's pow(x, 1/skew)
    return min + (max - min) * std::pow(normalized, skew);
}

void ParameterCache::registerParameter(const std::string& id, std::atomic<float>* ptr,
                                        float minValue, float maxValue,
                                        float skew, bool isBoolean) {
    if (finalized_)
        throw std::logic_error("ParameterCache::registerParameter called after finalize()");
    if (ptr == nullptr)
        throw std::invalid_argument("ParameterCache: null atomic pointer for id '" + id + "'");
    
    for (const auto& pair : idToIndex_) {
        if (pair.first == id)
            throw std::invalid_argument("ParameterCache: duplicate registration for id '" + id + "'");
    }

    std::size_t idx = entries_.size();
    idToIndex_.push_back({id, idx});
    entries_.emplace_back();
    entries_.back().ptr       = ptr;
    entries_.back().minValue  = minValue;
    entries_.back().maxValue  = maxValue;
    entries_.back().skew      = skew;
    entries_.back().isBoolean = isBoolean;
}

void ParameterCache::finalize() {
    finalized_ = true;
}

float ParameterCache::getValue(const std::string& id) const noexcept {
    for (const auto& pair : idToIndex_) {
        if (pair.first == id) {
            const auto& entry = entries_[pair.second];
            if (entry.ptr == nullptr)
                return 0.0f;
            const float normalized = entry.ptr->load(std::memory_order_relaxed);
            return denormalize(normalized, entry.minValue, entry.maxValue,
                               entry.skew, entry.isBoolean);
        }
    }
    return 0.0f;
}

std::atomic<float>* ParameterCache::getPointer(const std::string& id) const noexcept {
    for (const auto& pair : idToIndex_) {
        if (pair.first == id)
            return entries_[pair.second].ptr;
    }
    return nullptr;
}

bool ParameterCache::tryGetIndex(const std::string& id, std::size_t& index) const noexcept {
    // Linear search is usually faster than unordered_map hash for very small N (N<20 typically).
    // And this completely avoids unordered_map inside the real-time thread!
    for (const auto& pair : idToIndex_) {
        if (pair.first == id) {
            index = pair.second;
            return true;
        }
    }
    return false;
}

void ParameterCache::copyValuesTo(float* destination, std::size_t count) const noexcept {
    if (destination == nullptr || count == 0)
        return;

    const std::size_t n = std::min(count, entries_.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto& entry = entries_[i];
        if (entry.ptr != nullptr) {
            const float normalized = entry.ptr->load(std::memory_order_relaxed);
            destination[i] = denormalize(normalized, entry.minValue, entry.maxValue,
                                         entry.skew, entry.isBoolean);
        } else {
            destination[i] = 0.0f;
        }
    }

    for (std::size_t i = n; i < count; ++i)
        destination[i] = 0.0f;
}

bool ParameterCache::hasParameter(const std::string& id) const noexcept {
    for (const auto& pair : idToIndex_) {
        if (pair.first == id)
            return true;
    }
    return false;
}

std::vector<std::string> ParameterCache::getParameterIds() const {
    std::vector<std::string> ids;
    ids.reserve(idToIndex_.size());
    for (const auto& [id, _] : idToIndex_)
        ids.push_back(id);
    return ids;
}

} // namespace AgentVST
