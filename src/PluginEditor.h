// BigBubbleMuff — minimal generic editor (auto-generated parameter sliders).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace bbm {

// Thin wrapper over JUCE's generic editor: three pots (Sustain, Tone, Volume)
// plus output trim and bypass, drawn by the host-standard generic UI. A custom
// skin can replace this later without touching the processor or DSP.
class BigBubbleMuffEditor final : public juce::AudioProcessorEditor {
public:
  explicit BigBubbleMuffEditor(BigBubbleMuffProcessor &p);
  ~BigBubbleMuffEditor() override = default;

  void resized() override;

private:
  juce::GenericAudioProcessorEditor generic_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigBubbleMuffEditor)
};

} // namespace bbm
