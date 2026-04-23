#include "ParameterCache.h"

#include <algorithm>

namespace AgentVST {

void ParameterCache::registerParameter(const std::string& id, std::atomic<float>* ptr) {
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
    entries_.back().ptr = ptr;
}

void ParameterCache::finalize() {
    finalized_ = true;
}

float ParameterCache::getValue(const std::string& id) const noexcept {
    for (const auto& pair : idToIndex_) {
        if (pair.first == id) {
            const auto* ptr = entries_[pair.second].ptr;
            if (ptr == nullptr)
                return 0.0f;
            return ptr->load(std::memory_order_relaxed);
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
        const auto* ptr = entries_[i].ptr;
        destination[i] = (ptr != nullptr)
            ? ptr->load(std::memory_order_relaxed)
            : 0.0f;
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
