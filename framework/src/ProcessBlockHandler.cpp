#include "ProcessBlockHandler.h"
#include "DSPRouter.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace AgentVST {

namespace {

constexpr double kFallbackSampleRate = 44100.0;

double sanitizeSampleRate(double sampleRate) noexcept {
    if (std::isfinite(sampleRate) && sampleRate > 1.0)
        return sampleRate;
    return kFallbackSampleRate;
}

} // namespace

void ProcessBlockHandler::prepare(double sampleRate, int maxBlockSize) {
    sampleRate_   = sanitizeSampleRate(sampleRate);
    maxBlockSize_ = std::max(1, maxBlockSize);

    if (paramCache_)
        blockParamSnapshot_.assign(paramCache_->size(), 0.0f);

    if (agentDSP_)
        agentDSP_->prepare(sampleRate_, maxBlockSize_);

    watchdogTriggered_.store(false, std::memory_order_relaxed);
    noOpTriggered_.store(false, std::memory_order_relaxed);
    unchangedBlockStreak_ = 0;
    absoluteSampleCounter_ = 0;
}

void ProcessBlockHandler::setAgentDSP(IAgentDSP* agentDSP,
                                       const ParameterCache& paramCache) {
    agentDSP_   = agentDSP;
    paramCache_ = &paramCache;
    blockParamSnapshot_.assign(paramCache.size(), 0.0f);
}

void ProcessBlockHandler::setDSPRouter(DSPRouter* router) {
    router_ = router;
}

void ProcessBlockHandler::setWatchdogBudget(double budgetFraction) noexcept {
    budgetFraction_ = std::clamp(budgetFraction, 0.01, 1.0);
}

void ProcessBlockHandler::setWatchdogCheckInterval(int checkInterval) noexcept {
    checkInterval_ = std::max(1, checkInterval);
}

void ProcessBlockHandler::setNoOpDetectionEnabled(bool enabled) noexcept {
    noOpDetectionEnabled_ = enabled;
    if (!enabled) {
        unchangedBlockStreak_ = 0;
        noOpTriggered_.store(false, std::memory_order_relaxed);
    }
}

void ProcessBlockHandler::setNoOpDetectionThreshold(double threshold) noexcept {
    noOpThreshold_ = std::clamp(threshold, 1.0e-12, 1.0e-2);
}

void ProcessBlockHandler::setNoOpDetectionConsecutiveBlocks(int blocks) noexcept {
    noOpConsecutiveBlocks_ = std::max(1, blocks);
}

void ProcessBlockHandler::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&         /*midi*/,
    const juce::AudioPlayHead::CurrentPositionInfo& posInfo)
{
    if (agentDSP_ == nullptr || paramCache_ == nullptr) {
        buffer.clear();
        return;
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    sampleRate_ = sanitizeSampleRate(sampleRate_);

    const double bufferDurationMs = (static_cast<double>(numSamples) / sampleRate_) * 1000.0;
    const double budgetMs         = bufferDurationMs * budgetFraction_;

    watchdogTriggered_.store(false, std::memory_order_relaxed);

    if (!blockParamSnapshot_.empty())
        paramCache_->copyValuesTo(blockParamSnapshot_.data(), blockParamSnapshot_.size());

    if (router_ && router_->isConfigured())
        router_->updateParameters(*paramCache_);

    auto startTime = std::chrono::high_resolution_clock::now();
    bool timedOut  = false;
    double inputEnergy = 0.0;
    double diffEnergy  = 0.0;

    const std::int64_t blockStartSample = absoluteSampleCounter_;

    // Pre-fetch channel pointers to avoid calling getWritePointer in the inner loop
    const int processingChannels = std::min(numChannels, 32); // Cap at 32 for safety
    float* channelDataPtrs[32];
    for (int ch = 0; ch < processingChannels; ++ch)
        channelDataPtrs[ch] = buffer.getWritePointer(ch);

    for (int sample = 0; sample < numSamples; ++sample) {
        DSPContext ctx = buildContext(blockStartSample + static_cast<std::int64_t>(sample),
                                      numChannels,
                                      numSamples,
                                      posInfo);
        // Watchdog check (every checkInterval_ samples)
        if (!timedOut && sample > 0 && (sample % checkInterval_) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(now - startTime).count();
            if (elapsedMs > budgetMs) {
                timedOut = true;
                watchdogTriggered_.store(true, std::memory_order_relaxed);
                violationCount_.fetch_add(1, std::memory_order_relaxed);

                // Fire violation callback asynchronously (never on audio thread)
                if (onWatchdogViolation) {
                    auto cb = onWatchdogViolation;
                    std::thread([cb, elapsedMs, budgetMs]() {
                        cb(elapsedMs, budgetMs);
                    }).detach();
                }
            }
        }

        for (int ch = 0; ch < processingChannels; ++ch) {
            const float inputSample = channelDataPtrs[ch][sample];
            float outputSample = inputSample;

            if (!timedOut) {
                if (router_ && router_->isConfigured()) {
                    outputSample = router_->processSample(ch, inputSample);
                } else {
                    outputSample = agentDSP_->processSample(ch, inputSample, ctx);
                }
            }

            if (!timedOut && noOpDetectionEnabled_) {
                const double in = static_cast<double>(inputSample);
                const double diff = static_cast<double>(outputSample - inputSample);
                inputEnergy += in * in;
                diffEnergy += diff * diff;
            }

            channelDataPtrs[ch][sample] = outputSample;
        }
    }

    absoluteSampleCounter_ = blockStartSample + static_cast<std::int64_t>(numSamples);

    bool noOpThisBlock = false;
    if (!timedOut && noOpDetectionEnabled_) {
        constexpr double kMinInputEnergy = 1.0e-12;

        if (inputEnergy > kMinInputEnergy) {
            const double relativeDiffEnergy = diffEnergy / inputEnergy;

            if (relativeDiffEnergy <= noOpThreshold_) {
                ++unchangedBlockStreak_;

                if (unchangedBlockStreak_ >= noOpConsecutiveBlocks_) {
                    noOpThisBlock = true;

                    if (unchangedBlockStreak_ == noOpConsecutiveBlocks_) {
                        noOpCount_.fetch_add(1, std::memory_order_relaxed);
                        if (onPotentialNoOp) {
                            auto cb = onPotentialNoOp;
                            const int streak = unchangedBlockStreak_;
                            std::thread([cb, relativeDiffEnergy, streak]() {
                                cb(relativeDiffEnergy, streak);
                            }).detach();
                        }
                    }
                }
            } else {
                unchangedBlockStreak_ = 0;
            }
        } else {
            unchangedBlockStreak_ = 0;
        }
    }

    noOpTriggered_.store(noOpThisBlock, std::memory_order_relaxed);
}

bool ProcessBlockHandler::hadWatchdogViolation() const noexcept {
    return watchdogTriggered_.load(std::memory_order_relaxed);
}

std::uint64_t ProcessBlockHandler::watchdogViolationCount() const noexcept {
    return violationCount_.load(std::memory_order_relaxed);
}

void ProcessBlockHandler::resetWatchdogStats() noexcept {
    watchdogTriggered_.store(false, std::memory_order_relaxed);
    violationCount_.store(0, std::memory_order_relaxed);
}

bool ProcessBlockHandler::hadPotentialNoOp() const noexcept {
    return noOpTriggered_.load(std::memory_order_relaxed);
}

std::uint64_t ProcessBlockHandler::potentialNoOpCount() const noexcept {
    return noOpCount_.load(std::memory_order_relaxed);
}

void ProcessBlockHandler::resetNoOpStats() noexcept {
    noOpTriggered_.store(false, std::memory_order_relaxed);
    noOpCount_.store(0, std::memory_order_relaxed);
    unchangedBlockStreak_ = 0;
}

DSPContext ProcessBlockHandler::buildContext(
    std::int64_t sampleIndex, int numChannels, int numSamples,
    const juce::AudioPlayHead::CurrentPositionInfo& pos) const noexcept
{
    DSPContext ctx;
    ctx.sampleRate        = sampleRate_;
    ctx.numChannels       = numChannels;
    ctx.numSamplesInBlock = numSamples;
    ctx.currentSample     = sampleIndex;
    ctx.isPlaying         = pos.isPlaying;
    ctx.bpm               = pos.bpm;
    ctx.ppqPosition       = pos.ppqPosition;
    ctx._paramCache       = paramCache_;
    if (!blockParamSnapshot_.empty()) {
        ctx._paramSnapshotValues = blockParamSnapshot_.data();
        ctx._paramSnapshotCount  = blockParamSnapshot_.size();
    }
    return ctx;
}

} // namespace AgentVST
