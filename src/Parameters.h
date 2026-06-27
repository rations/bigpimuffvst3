// BigBubbleMuff — parameter identifiers and ranges.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bbm {

// Stable parameter IDs (persisted in host sessions — never renumber/rename).
namespace pid {
inline constexpr auto sustain = "sustain";   // R24 100k SUSTAIN/DIST pot, 0..1
inline constexpr auto tone = "tone";         // R23 100k TONE pot, 0..1
inline constexpr auto volume = "volume";     // R26 100k VOLUME pot, 0..1
inline constexpr auto output = "outputTrim"; // post-model trim in dB
inline constexpr auto gate = "gate";         // pre-gain noise gate, 0..1 (0 = off)
inline constexpr auto bypass = "bypass";     // host bypass
} // namespace pid

// State schema version, written into the saved ValueTree so future versions can
// migrate or reject incompatible blobs defensively (see setStateInformation).
inline constexpr int kStateVersion = 1;

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
  using juce::AudioParameterBool;
  using juce::AudioParameterFloat;
  using juce::NormalisableRange;
  using juce::ParameterID;

  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{pid::sustain, kStateVersion}, "Sustain",
      NormalisableRange<float>{0.0f, 1.0f}, 0.75f));
  layout.add(
      std::make_unique<AudioParameterFloat>(ParameterID{pid::tone, kStateVersion}, "Tone",
                                            NormalisableRange<float>{0.0f, 1.0f}, 0.5f));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{pid::volume, kStateVersion}, "Volume",
      NormalisableRange<float>{0.0f, 1.0f}, 0.5f));
  layout.add(std::make_unique<AudioParameterFloat>(
      ParameterID{pid::output, kStateVersion}, "Output",
      NormalisableRange<float>{-24.0f, 24.0f, 0.1f}, 0.0f));
  layout.add(
      std::make_unique<AudioParameterFloat>(ParameterID{pid::gate, kStateVersion}, "Gate",
                                            NormalisableRange<float>{0.0f, 1.0f}, 0.4f));
  layout.add(std::make_unique<AudioParameterBool>(ParameterID{pid::bypass, kStateVersion},
                                                  "Bypass", false));

  return layout;
}

} // namespace bbm
