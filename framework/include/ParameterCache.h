/**
 * ParameterCache.h — Thread-safe parameter value cache
 *
 * Stores raw std::atomic<float>* pointers obtained from APVTS::getRawParameterValue().
 * These pointers remain valid for the entire lifetime of the APVTS.
 *
 * IMPORTANT: APVTS stores parameters as normalized values (0.0–1.0).
 * This cache denormalizes them back to their real-world ranges on read,
 * so that DSP code receives meaningful values (e.g., -15.0 to 15.0 dB,
 * 20.0 to 20000.0 Hz) rather than raw normalized floats.
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
#include <stdexcept>
#include <vector>
#include <cmath>

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
     * Register a parameter's atomic pointer along with its real-world range.
     * Call this once per parameter during plugin initialization (in the
     * AudioProcessor constructor), after the APVTS has been constructed.
     *
     * @param id   The parameter ID string (must match APVTS parameter ID)
     * @param ptr  Pointer returned by apvts.getRawParameterValue(id)
     * @param minValue  Real-world minimum value (from schema)
     * @param maxValue  Real-world maximum value (from schema)
     * @param skew      Skew factor (from schema, 0.0 means no skew)
     * @param isBoolean True if this is a boolean parameter
     */
    void registerParameter(const std::string& id, std::atomic<float>* ptr,
                           float minValue = 0.0f, float maxValue = 1.0f,
                           float skew = 0.0f, bool isBoolean = false);

    /**
     * Finalize the cache. Call after all parameters have been registered.
     * Builds an indexed array for O(1) lookup by name.
     */
    void finalize();

    // ── Audio thread access ───────────────────────────────────────────────────

    /**
     * Get the current value of a parameter.
     * Lock-free; uses std::memory_order_relaxed.
     * Returns the DENORMALIZED (real-world) value.
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
     * Reads each atomic with memory_order_relaxed and DENORMALIZES the value.
     */
    void copyValuesTo(float* destination, std::size_t count) const noexcept;

    // ── Query ─────────────────────────────────────────────────────────────────

    bool hasParameter(const std::string& id) const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }

    /// Returns all registered parameter IDs (for UI generation)
    std::vector<std::string> getParameterIds() const;

private:
    // Denormalize a normalized 0.0–1.0 value back to its real-world range.
    static float denormalize(float normalized, float min, float max, float skew, bool isBool) noexcept;

    // Cache-line aligned entry to prevent false sharing (64-byte cache lines)
    struct alignas(64) Entry {
        std::atomic<float>* ptr = nullptr;
        float minValue  = 0.0f;
        float maxValue  = 1.0f;
        float skew      = 0.0f;
        bool  isBoolean = false;
        char  _pad[64 - sizeof(std::atomic<float>*) - 3*sizeof(float) - sizeof(bool)];
    };

    std::vector<std::pair<std::string, std::size_t>> idToIndex_;
    std::vector<Entry> entries_;
    bool finalized_ = false;
};

} // namespace AgentVST
