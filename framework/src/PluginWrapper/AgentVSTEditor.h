/**
 * AgentVSTEditor.h
 *
 * The concrete juce::AudioProcessorEditor for AgentVST plugins.
 *
 * Phase 0 / 1: Generates a functional C++ UI with juce::Slider / juce::ToggleButton
 *              / juce::ComboBox controls for all APVTS parameters, bound via
 *              SliderAttachment / ButtonAttachment / ComboBoxAttachment.
 *
 * Phase 3:     Upgraded to use juce::WebBrowserComponent (JUCE 8 WebView UI).
 */
#pragma once

#include "AgentVSTProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AgentVST {

class AgentVSTEditor : public juce::AudioProcessorEditor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::Timer {
public:
    explicit AgentVSTEditor(AgentVSTProcessor& processor);
    ~AgentVSTEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AgentVSTProcessor& proc_;
    bool usingWebView_ = false;

#if JUCE_WEB_BROWSER
    std::unique_ptr<juce::WebBrowserComponent> webView_;
    std::string webUiHtml_;
    juce::File uiAssetRoot_;
    bool devModeEnabled_ = false;
    std::atomic<bool> pendingParameterPush_ { false };

    bool initialiseWebView();
    bool isDevModeEnabled() const;
    juce::String resolveDevServerUrl() const;
    void registerParameterListeners();
    void unregisterParameterListeners();
    void pushParameterSnapshotToWebView();
    void pushMeterSnapshotToWebView();

   #if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    std::optional<juce::WebBrowserComponent::Resource>
    provideWebResource(const juce::String& path) const;
   #endif

    juce::var getParameterForJs(const juce::String& paramId) const;
    juce::var getAllParametersForJs() const;
    juce::var getMeterSnapshotForJs() const;
    void setParameterFromJs(const juce::String& paramId, const juce::var& value);
#endif

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    // ── Per-parameter control storage ─────────────────────────────────────────
    struct SliderRow {
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  valueLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct ToggleRow {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct ComboRow {
        std::unique_ptr<juce::Label>     label;
        std::unique_ptr<juce::ComboBox>  combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    std::vector<SliderRow>  sliderRows_;
    std::vector<ToggleRow>  toggleRows_;
    std::vector<ComboRow>   comboRows_;

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kRowHeight    = 44;
    static constexpr int kPadding      = 12;
    static constexpr int kHeaderHeight = 36;
    static constexpr int kLabelWidth   = 110;
    static constexpr int kValueWidth   = 60;
    static constexpr int kMinWidth     = 480;

    void buildUI();
    int  computeEditorHeight() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AgentVSTEditor)
};

} // namespace AgentVST
