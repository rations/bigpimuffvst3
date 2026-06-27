// BigBubbleMuff — skinned editor implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PluginEditor.h"

#include "BinaryData.h"

namespace bbm {
namespace {

// Absolute layout (pixels) tuned against gui/mufffbase.png (500x750). Knobs are
// laid out three-over-two; the lamp sits in the gap, the wordmark below, and the
// footswitch near the bottom — mirroring gui/examplegui.png.
namespace Layout {
constexpr int kWidth = 500;
constexpr int kHeight = 750;

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
                                                  BinaryData::mufffbase_pngSize)) {
  auto &apvts = processor_.getApvts();

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

  setSize(Layout::kWidth, Layout::kHeight);
  setResizable(false, false);
}

BigBubbleMuffEditor::~BigBubbleMuffEditor() {
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

  if (background_.isValid()) {
    g.drawImage(background_, getLocalBounds().toFloat(),
                juce::RectanglePlacement::stretchToFit);
  }

  // "BIG BUBBLE MUFF" stencil wordmark (the base art carries no text). A faint
  // light offset under the dark ink reads as a worn screen print.
  const juce::Rectangle<int> word(Layout::kWordCx - Layout::kWordWidth / 2,
                                  Layout::kWordTop, Layout::kWordWidth,
                                  Layout::kWordHeight);
  const auto font =
      juce::Font(juce::FontOptions{}
                     .withHeight(static_cast<float>(Layout::kWordHeight) * 0.78f)
                     .withStyle("Bold Italic"));
  g.setFont(font);
  // drawFittedText scales down only if the word would overrun the band, so the
  // larger size is honoured while never clipping.
  g.setColour(juce::Colour(0x55000000)); // soft drop shadow
  g.drawFittedText("BIG BUBBLE MUFF", word.translated(2, 2), juce::Justification::centred,
                   1);
  g.setColour(juce::Colour(0xd11a2110)); // worn dark-green ink
  g.drawFittedText("BIG BUBBLE MUFF", word, juce::Justification::centred, 1);
}

void BigBubbleMuffEditor::resized() {
  knobs_[0].setBounds(centredBox(Layout::kRow1Cx[0], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[1].setBounds(centredBox(Layout::kRow1Cx[1], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[2].setBounds(centredBox(Layout::kRow1Cx[2], Layout::kRow1Cy, Layout::kKnobBox));
  knobs_[3].setBounds(centredBox(Layout::kRow2Cx[0], Layout::kRow2Cy, Layout::kKnobBox));
  knobs_[4].setBounds(centredBox(Layout::kRow2Cx[1], Layout::kRow2Cy, Layout::kKnobBox));

  led_.setBounds(centredBox(Layout::kLedCx, Layout::kLedCy, Layout::kLedBox));
  footswitch_.setBounds(centredBox(Layout::kFootCx, Layout::kFootCy, Layout::kFootBox));
}

} // namespace bbm
