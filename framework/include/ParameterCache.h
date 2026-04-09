/**
 * ParameterCache.h — Thread-safe parameter value cache
 *
 * Stores raw std::atomic<float>* pointers obtained from APVTS::getRawParameterValue().
 * These pointers remain valid for the entire lifetime of the APVTS.
 *
 * Audio thread: reads via getValue() with memory_order_relaxed — zero overhead.
 * Message thread: writes happen through JUCE's APVTS machinery (not through this class).
 *
 * Cache-line padding is applied to each entry to prevent false sharing between
 * adjacent parameters when accessed by different CPU cores simultaneously.
 */
#pragma once

#include <string>
#include <atomic>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace AgentVST {

class ParameterCache {
public:
    ParameterCache()  = default;
    ~ParameterCache() = default;

    // Non-copyable (contains raw pointers that must not be duplicated)
    ParameterCache(const ParameterCache&)            = delete;
    ParameterCache& operator=(const ParameterCache&) = delete;

    // Movable
    ParameterCache(ParameterCache&&)            = default;
    ParameterCache& operator=(ParameterCache&&) = default;

    // ── Initialization (message thread) ──────────────────────────────────────

    /**
     * Register a parameter's atomic pointer.
     * Call this once per parameter during plugin initialization (in the
     * AudioProcessor constructor), after the APVTS has been constructed.
     *
     * @param id   The parameter ID string (must match APVTS parameter ID)
     * @param ptr  Pointer returned by apvts.getRawParameterValue(id)
     */
    void registerParameter(const std::string& id, std::atomic<float>* ptr);

    /**
     * Finalize the cache. Call after all parameters have been registered.
     * Builds an indexed array for O(1) lookup by name.
     */
    void finalize();

    // ── Audio thread access ───────────────────────────────────────────────────

    /**
     * Get the current value of a parameter.
     * Lock-free; uses std::memory_order_relaxed.
     * Returns 0.0f if id is not found (no allocation, no throw).
     */
    [[nodiscard]] float getValue(const std::string& id) const noexcept;

    /**
     * Get the raw atomic pointer for a parameter.
     * Returns nullptr if not found.
     */
    [[nodiscard]] std::atomic<float>* getPointer(const std::string& id) const noexcept;

    /**
     * Resolve a parameter ID to its internal index.
     * Returns true and writes index on success.
     */
    [[nodiscard]] bool tryGetIndex(const std::string& id, std::size_t& index) const noexcept;

    /**
     * Copy all current parameter values into destination.
     * Reads each atomic with memory_order_relaxed.
     */
    void copyValuesTo(float* destination, std::size_t count) const noexcept;

    // ── Query ─────────────────────────────────────────────────────────────────

    bool hasParameter(const std::string& id) const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }

    /// Returns all registered parameter IDs (for UI generation)
    std::vector<std::string> getParameterIds() const;

private:
    // Cache-line aligned entry to prevent false sharing (64-byte cache lines)
    struct alignas(64) Entry {
        std::atomic<float>* ptr = nullptr;
        char _pad[64 - sizeof(std::atomic<float>*)];
    };

    std::unordered_map<std::string, std::size_t> idToIndex_;
    std::vector<Entry> entries_;
    bool finalized_ = false;
};

} // namespace AgentVST
