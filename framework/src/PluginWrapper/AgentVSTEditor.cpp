#include "AgentVSTEditor.h"
#include "UIGenerator.h"

namespace AgentVST {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AgentVSTEditor::AgentVSTEditor(AgentVSTProcessor& processor)
    : AudioProcessorEditor(processor), proc_(processor)
{
    buildUI();

    const int h = computeEditorHeight();
    setSize(kMinWidth, std::max(h, 120));
    setResizable(true, true);
    setResizeLimits(kMinWidth, 100, 1200, 900);
}

AgentVSTEditor::~AgentVSTEditor() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Build controls
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTEditor::buildUI() {
    auto& apvts  = proc_.getAPVTS();
    const auto& schema = proc_.getSchema();

    for (const auto& def : schema.parameters) {
        if (def.type == "boolean") {
            ToggleRow row;
            row.button = std::make_unique<juce::ToggleButton>(def.name);
            row.button->setToggleState(def.defaultValue >= 0.5f,
                                       juce::dontSendNotification);
            row.attachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, def.id, *row.button);
            addAndMakeVisible(*row.button);
            toggleRows_.push_back(std::move(row));

        } else if (def.type == "enum") {
            ComboRow row;
            row.label = std::make_unique<juce::Label>("", def.name);
            row.label->setFont(juce::Font(12.0f));
            row.label->setJustificationType(juce::Justification::centredRight);

            row.combo = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < static_cast<int>(def.enumOptions.size()); ++i)
                row.combo->addItem(def.enumOptions[static_cast<std::size_t>(i)], i + 1);

            row.attachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                apvts, def.id, *row.combo);

            addAndMakeVisible(*row.label);
            addAndMakeVisible(*row.combo);
            comboRows_.push_back(std::move(row));

        } else {
            // float or int — use a horizontal slider
            SliderRow row;
            row.label = std::make_unique<juce::Label>("", def.name);
            row.label->setFont(juce::Font(12.0f));
            row.label->setJustificationType(juce::Justification::centredRight);

            row.slider = std::make_unique<juce::Slider>(
                juce::Slider::LinearHorizontal,
                juce::Slider::TextBoxRight);
            row.slider->setRange(def.minValue, def.maxValue,
                                  def.step > 0.0f ? def.step : 0.0);
            row.slider->setValue(def.defaultValue, juce::dontSendNotification);
            row.slider->setTextValueSuffix(
                def.unit.empty() ? "" : " " + def.unit);
            row.slider->setNumDecimalPlacesToDisplay(
                (def.type == "int" || def.step >= 1.0f) ? 0 : 2);

            row.attachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, def.id, *row.slider);

            addAndMakeVisible(*row.label);
            addAndMakeVisible(*row.slider);
            sliderRows_.push_back(std::move(row));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

int AgentVSTEditor::computeEditorHeight() const {
    const int totalRows = static_cast<int>(
        sliderRows_.size() + toggleRows_.size() + comboRows_.size());
    return kHeaderHeight + kPadding * 2 + totalRows * kRowHeight;
}

void AgentVSTEditor::resized() {
    auto area = getLocalBounds().reduced(kPadding);

    // Header area
    area.removeFromTop(kHeaderHeight);

    // Lay out all controls in a single vertical column
    const auto& schema = proc_.getSchema();
    int sliderIdx  = 0;
    int toggleIdx  = 0;
    int comboIdx   = 0;

    for (const auto& def : schema.parameters) {
        auto row = area.removeFromTop(kRowHeight).reduced(0, 4);

        if (def.type == "boolean" && toggleIdx < static_cast<int>(toggleRows_.size())) {
            toggleRows_[static_cast<std::size_t>(toggleIdx)].button->setBounds(
                row.withTrimmedLeft(kLabelWidth));
            ++toggleIdx;
        } else if (def.type == "enum" && comboIdx < static_cast<int>(comboRows_.size())) {
            auto& cr = comboRows_[static_cast<std::size_t>(comboIdx)];
            cr.label->setBounds(row.removeFromLeft(kLabelWidth));
            cr.combo->setBounds(row);
            ++comboIdx;
        } else if (sliderIdx < static_cast<int>(sliderRows_.size())) {
            auto& sr = sliderRows_[static_cast<std::size_t>(sliderIdx)];
            sr.label->setBounds(row.removeFromLeft(kLabelWidth));
            sr.slider->setBounds(row);
            ++sliderIdx;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────

void AgentVSTEditor::paint(juce::Graphics& g) {
    // Background
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Header bar
    auto headerArea = getLocalBounds()
                          .reduced(kPadding)
                          .removeFromTop(kHeaderHeight);

    g.setColour(juce::Colour(0xff0f3460));
    g.fillRoundedRectangle(headerArea.toFloat(), 6.0f);

    // Plugin name
    g.setColour(juce::Colour(0xffe94560));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText(proc_.getSchema().metadata.name,
               headerArea.reduced(8, 0),
               juce::Justification::centredLeft);

    // Version tag
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(11.0f));
    g.drawText("v" + proc_.getSchema().metadata.version,
               headerArea.reduced(8, 0),
               juce::Justification::centredRight);

    // Separator line
    g.setColour(juce::Colour(0xff2d2d4e));
    g.drawHorizontalLine(kPadding + kHeaderHeight + 4,
                         static_cast<float>(kPadding),
                         static_cast<float>(getWidth() - kPadding));
}

} // namespace AgentVST
