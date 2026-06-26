// BigBubbleMuff — AudioProcessor.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "Parameters.h"
#include "dsp/BigMuffPi.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace bbm {

class BigBubbleMuffProcessor final : public juce::AudioProcessor {
public:
  BigBubbleMuffProcessor();
  ~BigBubbleMuffProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "BigBubbleMuff"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState &getApvts() { return apvts_; }

private:
  void pullControls();

  juce::AudioProcessorValueTreeState apvts_;

  // Cached atomic parameter pointers (set once; read on the audio thread).
  std::atomic<float> *sustainParam_ = nullptr;
  std::atomic<float> *toneParam_ = nullptr;
  std::atomic<float> *volumeParam_ = nullptr;
  std::atomic<float> *outputParam_ = nullptr;
  std::atomic<float> *bypassParam_ = nullptr;

  BigMuffPi engine_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BigBubbleMuffProcessor)
};

} // namespace bbm
