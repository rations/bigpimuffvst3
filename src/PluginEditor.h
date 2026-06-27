// BigBubbleMuff — skinned, layered editor (faceplate + knobs + LED + footswitch).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "PluginProcessor.h"
#include "ui/FootswitchButton.h"
#include "ui/LedComponent.h"
#include "ui/MuffLookAndFeel.h"
#include "ui/PresetBar.h"

#include <array>
#include <cstddef>
#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

namespace bbm {

// Image-layered editor: a slim preset bar tops a worn faceplate backdrop, five
// rotary knobs (Sustain, Tone, Volume, Output, Gate) sit on the face, a lamp
// shows engage state, and the chrome footswitch toggles bypass. The window is
// resizable with the art's aspect ratio locked. In the Standalone build it also
// installs a native titlebar and a File/Help menu.
class BigBubbleMuffEditor final : public juce::AudioProcessorEditor {
public:
  explicit BigBubbleMuffEditor(BigBubbleMuffProcessor &p);
  ~BigBubbleMuffEditor() override;

  // Framework object: non-copyable (JUCE macro below) and non-movable.
  BigBubbleMuffEditor(BigBubbleMuffEditor &&) = delete;
  BigBubbleMuffEditor &operator=(BigBubbleMuffEditor &&) = delete;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  static constexpr std::size_t kNumKnobs = 5;
  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

  void configureKnob(juce::Slider &knob, std::unique_ptr<SliderAttachment> &attachment,
                     const char *paramId, const char *name);

  BigBubbleMuffProcessor &processor_;
  MuffLookAndFeel lookAndFeel_;
  juce::Image background_;
  juce::TooltipWindow tooltips_;

  PresetBar presetBar_;

  std::array<juce::Slider, kNumKnobs> knobs_;
  std::array<std::unique_ptr<SliderAttachment>, kNumKnobs> knobAttachments_;

  LedComponent led_;
  FootswitchButton footswitch_;
  std::unique_ptr<juce::ParameterAttachment> bypassAttachment_;

  // Standalone-only: owns the window menu bar (null and unused under VST3).
  std::unique_ptr<juce::MenuBarModel> menuModel_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigBubbleMuffEditor)
};

} // namespace bbm
