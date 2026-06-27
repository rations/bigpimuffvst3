// BigBubbleMuff — skinned editor implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PluginEditor.h"

#include "BinaryData.h"

// Standalone wrapper support: gives access to StandalonePluginHolder (audio
// settings dialog) and the host DocumentWindow. Inert in the VST3 build.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace bbm {
namespace {

// Native titlebar menu shown only in the Standalone app (gateway-linux style).
class MuffMenuModel final : public juce::MenuBarModel {
public:
  juce::StringArray getMenuBarNames() override { return {"File", "Help"}; }

  juce::PopupMenu getMenuForIndex(int index, const juce::String &) override {
    juce::PopupMenu menu;
    if (index == 0) {
      menu.addItem(1, "Preferences...");
      menu.addSeparator();
      menu.addItem(2, "Quit");
    } else {
      menu.addItem(3, "About BigBubbleMuff");
    }
    return menu;
  }

  void menuItemSelected(int menuItemID, int) override {
    if (menuItemID == 1) {
      if (auto *holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
    } else if (menuItemID == 2) {
      if (auto *app = juce::JUCEApplicationBase::getInstance())
        app->systemRequestedQuit();
    } else if (menuItemID == 3) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon, "About BigBubbleMuff",
          "BigBubbleMuff \xe2\x80\x94 native Linux emulation of the Russian "
          "\"Bubble Font\" Big Muff Pi.\n\nGPL-3.0-or-later.");
    }
  }
};

// Absolute layout (pixels) tuned against gui/mufffbase.png (500x750). Knobs are
// laid out three-over-two; the lamp sits in the gap, the wordmark below, and the
// footswitch near the bottom — mirroring gui/examplegui.png.
//
// These are *base* coordinates. The window is resizable with a locked aspect
// ratio: resized()/paint() multiply everything by s = width / kBaseWidth, and the
// faceplate is pushed down by the preset bar (kBarHeight) before scaling.
namespace Layout {
constexpr int kWidth = 500;  // faceplate art width
constexpr int kHeight = 750; // faceplate art height

constexpr int kBarHeight = 40;                    // slim preset strip on top
constexpr int kBaseWidth = kWidth;                // base window width
constexpr int kBaseHeight = kHeight + kBarHeight; // base window height

// The faceplate has a faux-3D right edge, so the true face is defined by the
// four corner screws (~x={42,415}, y={40,712}) and its centre is ~(228, 376) —
// left of the image centre. Everything below is centred on that, not the image.
constexpr int kFaceCx = 228;

constexpr int kKnobBox = 118;
constexpr int kRow1Cy = 152; // top row centre Y (Sustain, Tone, Volume)
constexpr int kRow2Cy = 344; // bottom row centre Y (Output, Gate)
constexpr std::array<int, 3> kRow1Cx{102, 228, 354}; // outer pair nudged wider
constexpr std::array<int, 2> kRow2Cx{160, 296};

constexpr int kLedBox = 40;
constexpr int kLedCx = kFaceCx;
constexpr int kLedCy = 248;

constexpr int kFootBox = 132;
constexpr int kFootCx = kFaceCx;
constexpr int kFootCy = 600;

constexpr int kWordCx = kFaceCx;
constexpr int kWordWidth = 446;
constexpr int kWordTop = 446;
constexpr int kWordHeight = 64;
} // namespace Layout

juce::Rectangle<int> centredBox(int cx, int cy, int size) {
  return juce::Rectangle<int>(cx - size / 2, cy - size / 2, size, size);
}

} // namespace

BigBubbleMuffEditor::BigBubbleMuffEditor(BigBubbleMuffProcessor &p)
    : juce::AudioProcessorEditor(p), processor_(p),
      background_(juce::ImageCache::getFromMemory(BinaryData::mufffbase_png,
                                                  BinaryData::mufffbase_pngSize)),
      presetBar_(p.getApvts()) {
  auto &apvts = processor_.getApvts();

  addAndMakeVisible(presetBar_);

  // Constant indices keep this clean under clang-tidy's bounds checks; the order
  // here matches the three-over-two placement in resized().
  configureKnob(knobs_[0], knobAttachments_[0], pid::sustain, "Sustain");
  configureKnob(knobs_[1], knobAttachments_[1], pid::tone, "Tone");
  configureKnob(knobs_[2], knobAttachments_[2], pid::volume, "Volume");
  configureKnob(knobs_[3], knobAttachments_[3], pid::output, "Output");
  configureKnob(knobs_[4], knobAttachments_[4], pid::gate, "Gate");

  addAndMakeVisible(led_);

  footswitch_.setTooltip("Bypass (footswitch)");
  footswitch_.onClick = [this] {
    if (auto *bypass = processor_.getApvts().getParameter(pid::bypass)) {
      const bool bypassedNow = bypass->getValue() >= 0.5f;
      bypassAttachment_->setValueAsCompleteGesture(bypassedNow ? 0.0f : 1.0f);
    }
  };
  addAndMakeVisible(footswitch_);

  if (auto *bypass = apvts.getParameter(pid::bypass)) {
    bypassAttachment_ = std::make_unique<juce::ParameterAttachment>(
        *bypass,
        [this](float value) {
          const bool engaged = value < 0.5f; // engaged == NOT bypassed
          led_.setOn(engaged);
          footswitch_.setEngaged(engaged);
        },
        nullptr);
    bypassAttachment_->sendInitialUpdate();
  }

  setSize(Layout::kBaseWidth, Layout::kBaseHeight);
  setResizable(true, true);
  if (auto *bounds = getConstrainer()) {
    bounds->setFixedAspectRatio(static_cast<double>(Layout::kBaseWidth) /
                                static_cast<double>(Layout::kBaseHeight));
  }
  setResizeLimits(Layout::kBaseWidth / 2, Layout::kBaseHeight / 2, Layout::kBaseWidth * 2,
                  Layout::kBaseHeight * 2);

  // Standalone only: once parented, switch to a native titlebar and install the
  // File/Help menu. No-op under a plugin host (no parent DocumentWindow). Guarded
  // by a SafePointer in case the editor is torn down before this runs.
  juce::Component::SafePointer<BigBubbleMuffEditor> safe(this);
  juce::MessageManager::callAsync([safe]() mutable {
    if (safe == nullptr)
      return;
    if (auto *window = safe->findParentComponentOfClass<juce::DocumentWindow>()) {
      window->setUsingNativeTitleBar(true);
      safe->menuModel_ = std::make_unique<MuffMenuModel>();
      window->setMenuBar(safe->menuModel_.get());
    }
  });
}

BigBubbleMuffEditor::~BigBubbleMuffEditor() {
  if (auto *window = findParentComponentOfClass<juce::DocumentWindow>())
    window->setMenuBar(nullptr); // detach before menuModel_ is destroyed

  for (auto &knob : knobs_) {
    knob.setLookAndFeel(nullptr);
  }
}

void BigBubbleMuffEditor::configureKnob(juce::Slider &knob,
                                        std::unique_ptr<SliderAttachment> &attachment,
                                        const char *paramId, const char *name) {
  knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);
  knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  knob.setLookAndFeel(&lookAndFeel_);
  knob.setName(name);
  knob.setTooltip(name);
  addAndMakeVisible(knob);
  attachment = std::make_unique<SliderAttachment>(processor_.getApvts(), paramId, knob);
}

void BigBubbleMuffEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2f3326)); // behind any transparent faceplate corners

  const float s = static_cast<float>(getWidth()) / static_cast<float>(Layout::kBaseWidth);
  const float barH = static_cast<float>(Layout::kBarHeight) * s;

  if (background_.isValid()) {
    const juce::Rectangle<float> face(0.0f, barH, static_cast<float>(getWidth()),
                                      static_cast<float>(getHeight()) - barH);
    g.drawImage(background_, face, juce::RectanglePlacement::stretchToFit);
  }

  // "BIG BUBBLE MUFF" stencil wordmark (the base art carries no text). A faint
  // light offset under the dark ink reads as a worn screen print. Coordinates are
  // in faceplate space, pushed below the preset bar and scaled with the window.
  const juce::Rectangle<int> wordBase(Layout::kWordCx - Layout::kWordWidth / 2,
                                      Layout::kWordTop + Layout::kBarHeight,
                                      Layout::kWordWidth, Layout::kWordHeight);
  const auto word = (wordBase.toFloat() * s).toNearestInt();
  const auto font =
      juce::Font(juce::FontOptions{}
                     .withHeight(static_cast<float>(Layout::kWordHeight) * 0.78f * s)
                     .withStyle("Bold Italic"));
  g.setFont(font);
  // drawFittedText scales down only if the word would overrun the band, so the
  // larger size is honoured while never clipping.
  const int shadow = juce::roundToInt(2.0f * s);
  g.setColour(juce::Colour(0x55000000)); // soft drop shadow
  g.drawFittedText("BIG BUBBLE MUFF", word.translated(shadow, shadow),
                   juce::Justification::centred, 1);
  g.setColour(juce::Colour(0xd11a2110)); // worn dark-green ink
  g.drawFittedText("BIG BUBBLE MUFF", word, juce::Justification::centred, 1);
}

void BigBubbleMuffEditor::resized() {
  const float s = static_cast<float>(getWidth()) / static_cast<float>(Layout::kBaseWidth);

  // Place a faceplate-space centred box: offset below the preset bar, then scale.
  const auto place = [s](int cx, int cy, int size) {
    return (centredBox(cx, cy + Layout::kBarHeight, size).toFloat() * s).toNearestInt();
  };

  presetBar_.setBounds(0, 0, getWidth(),
                       juce::roundToInt(static_cast<float>(Layout::kBarHeight) * s));

  knobs_[0].setBounds(place(Layout::kRow1Cx[0], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[1].setBounds(place(Layout::kRow1Cx[1], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[2].setBounds(place(Layout::kRow1Cx[2], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[3].setBounds(place(Layout::kRow2Cx[0], Layout::kRow2Cy, Layout::kKnobBox));
  knobs_[4].setBounds(place(Layout::kRow2Cx[1], Layout::kRow2Cy, Layout::kKnobBox));

  led_.setBounds(place(Layout::kLedCx, Layout::kLedCy, Layout::kLedBox));
  footswitch_.setBounds(place(Layout::kFootCx, Layout::kFootCy, Layout::kFootBox));
}

} // namespace bbm
