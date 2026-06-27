// BigBubbleMuff — top-level DSP engine.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
//
// Models the four-stage Russian "Bubble Font" Big Muff Pi as a chain of WDF
// sub-circuits, oversampled around the nonlinear clipping core. The public
// interface is host-facing and real-time-safe: prepare() does all allocation,
// process() and setParameters() never allocate, lock, or perform I/O.
#pragma once

#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>

namespace bbm {

// Smoothed, normalised control values handed from the message thread to audio.
struct Controls {
  float sustain = 0.75f; // 0..1, R24 drive into the clippers
  float tone = 0.5f;     // 0..1, R23 bass<->treble blend
  float volume = 0.5f;   // 0..1, R26 output divider
  float outputTrimDb = 0.0f;
  float gate = 0.4f; // 0..1 pre-gain noise gate threshold (0 = off)
};

class BigMuffPi {
public:
  BigMuffPi();
  ~BigMuffPi();

  BigMuffPi(const BigMuffPi &) = delete;
  BigMuffPi &operator=(const BigMuffPi &) = delete;
  BigMuffPi(BigMuffPi &&) = delete;
  BigMuffPi &operator=(BigMuffPi &&) = delete;

  // Allocates and sizes all state. Call from prepareToPlay (message thread).
  void prepare(const juce::dsp::ProcessSpec &spec);

  // Resets filter/WDF state without reallocating.
  void reset();

  // Processes one block in place. The pedal is mono: the input is downmixed to a
  // single channel, the circuit is solved once, and the result is fanned out to
  // every output channel (dual-mono on a stereo bus). Real-time-safe.
  void process(juce::dsp::AudioBlock<float> block);

  // Pushes new control values (real-time-safe; smoothed internally).
  void setControls(const Controls &c);

  int getOversamplingLatencySamples() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace bbm
