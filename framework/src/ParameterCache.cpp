#include "ParameterCache.h"

namespace AgentVST {

void ParameterCache::registerParameter(const std::string& id, std::atomic<float>* ptr) {
    if (finalized_)
        throw std::logic_error("ParameterCache::registerParameter called after finalize()");
    if (ptr == nullptr)
        throw std::invalid_argument("ParameterCache: null atomic pointer for id '" + id + "'");
    if (idToIndex_.count(id))
        throw std::invalid_argument("ParameterCache: duplicate registration for id '" + id + "'");

    std::size_t idx = entries_.size();
    idToIndex_[id] = idx;
    entries_.emplace_back();
    entries_.back().ptr = ptr;
}

void ParameterCache::finalize() {
    finalized_ = true;
}

float ParameterCache::getValue(const std::string& id) const noexcept {
    auto it = idToIndex_.find(id);
    if (it == idToIndex_.end())
        return 0.0f;
    const auto* ptr = entries_[it->second].ptr;
    if (ptr == nullptr)
        return 0.0f;
    return ptr->load(std::memory_order_relaxed);
}

std::atomic<float>* ParameterCache::getPointer(const std::string& id) const noexcept {
    auto it = idToIndex_.find(id);
    if (it == idToIndex_.end())
        return nullptr;
    return entries_[it->second].ptr;
}

bool ParameterCache::hasParameter(const std::string& id) const noexcept {
    return idToIndex_.count(id) > 0;
}

std::vector<std::string> ParameterCache::getParameterIds() const {
    std::vector<std::string> ids;
    ids.reserve(idToIndex_.size());
    for (const auto& [id, _] : idToIndex_)
        ids.push_back(id);
    return ids;
}

} // namespace AgentVST
