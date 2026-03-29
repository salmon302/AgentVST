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
#include <memory>
#include <vector>

namespace AgentVST {

class AgentVSTEditor : public juce::AudioProcessorEditor {
public:
    explicit AgentVSTEditor(AgentVSTProcessor& processor);
    ~AgentVSTEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AgentVSTProcessor& proc_;

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
