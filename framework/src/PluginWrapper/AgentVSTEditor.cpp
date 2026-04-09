#include "AgentVSTEditor.h"
#include "UIGenerator.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#ifndef AGENTVST_DEV_SERVER_URL
    #define AGENTVST_DEV_SERVER_URL "http://127.0.0.1:5173"
#endif

#ifndef AGENTVST_UI_ROOT
    #define AGENTVST_UI_ROOT ""
#endif

namespace AgentVST {

namespace {

#if JUCE_WEB_BROWSER
double varToDouble(const juce::var& value) {
    if (value.isBool())
        return static_cast<bool>(value) ? 1.0 : 0.0;
    if (value.isInt() || value.isInt64() || value.isDouble())
        return static_cast<double>(value);
    if (value.isString())
        return value.toString().getDoubleValue();
    return 0.0;
}

std::vector<std::byte> stringToBytes(const std::string& text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (unsigned char c : text)
        bytes.push_back(static_cast<std::byte>(c));
    return bytes;
}

std::vector<std::byte> fileToBytes(const juce::File& file) {
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
        return {};

    std::vector<std::byte> bytes(data.getSize());
    if (!bytes.empty())
        std::memcpy(bytes.data(), data.getData(), data.getSize());

    return bytes;
}

bool parseBooleanEnvValue(const juce::String& value) {
    const auto lower = value.trim().toLowerCase();
    return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

std::string mimeTypeForPath(const juce::String& path) {
    const auto extension = path.fromLastOccurrenceOf(".", false, false).toLowerCase();

    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js" || extension == ".mjs") return "application/javascript; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".webp") return "image/webp";
    if (extension == ".woff") return "font/woff";
    if (extension == ".woff2") return "font/woff2";

    return "application/octet-stream";
}
#endif

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AgentVSTEditor::AgentVSTEditor(AgentVSTProcessor& processor)
    : AudioProcessorEditor(processor), proc_(processor)
{
#if JUCE_WEB_BROWSER
    usingWebView_ = initialiseWebView();
#endif

    if (usingWebView_) {
        setSize(840, 560);
        setResizable(true, true);
        setResizeLimits(kMinWidth, 280, 1800, 1400);
        return;
    }

    buildUI();

    const int h = computeEditorHeight();
    setSize(kMinWidth, std::max(h, 120));
    setResizable(true, true);
    setResizeLimits(kMinWidth, 100, 1200, 900);
}

AgentVSTEditor::~AgentVSTEditor() {
#if JUCE_WEB_BROWSER
    stopTimer();
    unregisterParameterListeners();
#endif
}

#if JUCE_WEB_BROWSER
bool AgentVSTEditor::initialiseWebView() {
#if !JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    return false;
#else
    UIGenerator uiGenerator;
    webUiHtml_ = uiGenerator.generateHTML(proc_.getSchema());

    devModeEnabled_ = isDevModeEnabled();

    const auto uiRoot = juce::SystemStats::getEnvironmentVariable(
        "AGENTVST_UI_ROOT",
        juce::String(AGENTVST_UI_ROOT));

    if (uiRoot.isNotEmpty())
        uiAssetRoot_ = juce::File(uiRoot);
    else
        uiAssetRoot_ = juce::File();

    juce::WebBrowserComponent::Options options;
#if JUCE_WINDOWS
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
#endif

    options = options.withKeepPageLoadedWhenBrowserIsHidden()
                     .withNativeIntegrationEnabled()
                     .withNativeFunction(
                         juce::Identifier("agentGetParameter"),
                         [this](const juce::Array<juce::var>& args,
                                juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                             if (args.isEmpty()) {
                                 complete(juce::var());
                                 return;
                             }
                             complete(getParameterForJs(args[0].toString()));
                         })
                     .withNativeFunction(
                         juce::Identifier("agentSetParameter"),
                         [this](const juce::Array<juce::var>& args,
                                juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                             if (args.size() < 2) {
                                 complete(juce::var(false));
                                 return;
                             }

                             const auto paramId = args[0].toString();
                             setParameterFromJs(paramId, args[1]);
                             complete(getParameterForJs(paramId));
                         })
                     .withNativeFunction(
                         juce::Identifier("agentGetAllParameters"),
                         [this](const juce::Array<juce::var>&,
                                juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                             complete(getAllParametersForJs());
                         })
                     .withNativeFunction(
                         juce::Identifier("agentGetMeters"),
                         [this](const juce::Array<juce::var>&,
                                juce::WebBrowserComponent::NativeFunctionCompletion complete) {
                             complete(getMeterSnapshotForJs());
                         })
                     .withResourceProvider(
                         [this](const juce::String& path) {
                             return provideWebResource(path);
                         });

    if (!juce::WebBrowserComponent::areOptionsSupported(options))
        return false;

    webView_ = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView_);

    if (devModeEnabled_)
        webView_->goToURL(resolveDevServerUrl());
    else
        webView_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    registerParameterListeners();
    pendingParameterPush_.store(true, std::memory_order_relaxed);
    startTimerHz(30);

    return true;
#endif
}

bool AgentVSTEditor::isDevModeEnabled() const {
    const auto envValue = juce::SystemStats::getEnvironmentVariable("AGENTVST_DEV_MODE", {});
    if (envValue.isEmpty())
        return false;

    return parseBooleanEnvValue(envValue);
}

juce::String AgentVSTEditor::resolveDevServerUrl() const {
    auto url = juce::SystemStats::getEnvironmentVariable(
        "AGENTVST_DEV_SERVER_URL",
        juce::String(AGENTVST_DEV_SERVER_URL));

    if (url.isEmpty())
        url = AGENTVST_DEV_SERVER_URL;

    return url;
}

void AgentVSTEditor::registerParameterListeners() {
    auto& apvts = proc_.getAPVTS();
    for (const auto& def : proc_.getSchema().parameters)
        apvts.addParameterListener(def.id, this);
}

void AgentVSTEditor::unregisterParameterListeners() {
    auto& apvts = proc_.getAPVTS();
    for (const auto& def : proc_.getSchema().parameters)
        apvts.removeParameterListener(def.id, this);
}

void AgentVSTEditor::pushParameterSnapshotToWebView() {
    if (!usingWebView_ || webView_ == nullptr)
        return;

    webView_->emitEventIfBrowserIsVisible(
        juce::Identifier("agentParameterSnapshot"),
        getAllParametersForJs());
}

void AgentVSTEditor::pushMeterSnapshotToWebView() {
    if (!usingWebView_ || webView_ == nullptr)
        return;

    webView_->emitEventIfBrowserIsVisible(
        juce::Identifier("agentMeterSnapshot"),
        getMeterSnapshotForJs());
}

#if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
std::optional<juce::WebBrowserComponent::Resource>
AgentVSTEditor::provideWebResource(const juce::String& rawPath) const {
    auto path = rawPath.upToFirstOccurrenceOf("?", false, false);

    if (path.startsWithIgnoreCase("https://") || path.startsWithIgnoreCase("juce://")) {
        const auto afterHost = path.fromFirstOccurrenceOf("/", false, false);
        path = afterHost.isEmpty() ? "/" : afterHost;
    }

    if (path.isEmpty())
        path = "/";

    auto relativePath = path;
    if (relativePath.startsWithChar('/'))
        relativePath = relativePath.substring(1);

    if (relativePath.isEmpty())
        relativePath = "index.html";

    if (relativePath.contains(".."))
        return std::nullopt;

    if (uiAssetRoot_.isDirectory()) {
        const auto file = uiAssetRoot_.getChildFile(relativePath);
        if (file.existsAsFile()) {
            juce::WebBrowserComponent::Resource resource;
            resource.data = fileToBytes(file);
            if (!resource.data.empty()) {
                resource.mimeType = mimeTypeForPath(file.getFileName());
                return resource;
            }
        }
    }

    if (path == "/" || path == "/index.html" || path == "index.html") {
        juce::WebBrowserComponent::Resource resource;
        resource.data = stringToBytes(webUiHtml_);
        resource.mimeType = "text/html; charset=utf-8";
        return resource;
    }

    return std::nullopt;
}
#endif

juce::var AgentVSTEditor::getParameterForJs(const juce::String& paramId) const {
    const auto* def = proc_.getSchema().findParameter(paramId.toStdString());
    if (def == nullptr)
        return juce::var();

    if (auto* parameter = proc_.getAPVTS().getParameter(def->id)) {
        const auto rawValue = parameter->convertFrom0to1(parameter->getValue());

        if (def->type == "boolean")
            return juce::var(rawValue >= 0.5f);

        if (def->type == "int")
            return juce::var(static_cast<int>(std::lround(rawValue)));

        if (def->type == "enum") {
            auto index = static_cast<int>(std::lround(rawValue));
            if (!def->enumOptions.empty()) {
                index = juce::jlimit(0,
                                     static_cast<int>(def->enumOptions.size()) - 1,
                                     index);
            }
            return juce::var(index);
        }

        return juce::var(static_cast<double>(rawValue));
    }

    return juce::var();
}

juce::var AgentVSTEditor::getAllParametersForJs() const {
    juce::DynamicObject::Ptr stateObject(new juce::DynamicObject());

    for (const auto& def : proc_.getSchema().parameters)
        stateObject->setProperty(juce::Identifier(def.id),
                                 getParameterForJs(juce::String(def.id)));

    return juce::var(stateObject.get());
}

juce::var AgentVSTEditor::getMeterSnapshotForJs() const {
    const auto meterSnapshot = proc_.getMeterSnapshot();
    juce::DynamicObject::Ptr meterObject(new juce::DynamicObject());

    meterObject->setProperty("inputPeakL", meterSnapshot.inputPeakL);
    meterObject->setProperty("inputPeakR", meterSnapshot.inputPeakR);
    meterObject->setProperty("inputRmsL", meterSnapshot.inputRmsL);
    meterObject->setProperty("inputRmsR", meterSnapshot.inputRmsR);
    meterObject->setProperty("outputPeakL", meterSnapshot.outputPeakL);
    meterObject->setProperty("outputPeakR", meterSnapshot.outputPeakR);
    meterObject->setProperty("outputRmsL", meterSnapshot.outputRmsL);
    meterObject->setProperty("outputRmsR", meterSnapshot.outputRmsR);

    return juce::var(meterObject.get());
}

void AgentVSTEditor::setParameterFromJs(const juce::String& paramId,
                                        const juce::var& value) {
    const auto* def = proc_.getSchema().findParameter(paramId.toStdString());
    if (def == nullptr)
        return;

    auto* parameter = proc_.getAPVTS().getParameter(def->id);
    if (parameter == nullptr)
        return;

    auto rawValue = static_cast<float>(varToDouble(value));

    if (def->type == "boolean") {
        const auto boolValue = value.isBool() ? static_cast<bool>(value)
                                              : (rawValue >= 0.5f);
        rawValue = boolValue ? 1.0f : 0.0f;
    } else if (def->type == "int" || def->type == "enum") {
        rawValue = std::round(rawValue);
    }

    auto minValue = def->minValue;
    auto maxValue = def->maxValue;

    if (def->type == "enum") {
        minValue = 0.0f;
        if (!def->enumOptions.empty())
            maxValue = static_cast<float>(def->enumOptions.size() - 1);
    }

    rawValue = juce::jlimit(minValue, maxValue, rawValue);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(rawValue));
}
#endif

void AgentVSTEditor::parameterChanged(const juce::String& /*parameterID*/,
                                      float /*newValue*/) {
#if JUCE_WEB_BROWSER
    pendingParameterPush_.store(true, std::memory_order_relaxed);
#endif
}

void AgentVSTEditor::timerCallback() {
#if JUCE_WEB_BROWSER
    if (pendingParameterPush_.exchange(false, std::memory_order_relaxed))
        pushParameterSnapshotToWebView();

    pushMeterSnapshotToWebView();
#endif
}

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
            row.label->setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
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
            row.label->setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
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
    if (usingWebView_) {
#if JUCE_WEB_BROWSER
        if (webView_)
            webView_->setBounds(getLocalBounds());
#endif
        return;
    }

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
    if (usingWebView_) {
        g.fillAll(juce::Colours::black);
        return;
    }

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
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.0f).withStyle("Bold")));
    g.drawText(proc_.getSchema().metadata.name,
               headerArea.reduced(8, 0),
               juce::Justification::centredLeft);

    // Version tag
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
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
