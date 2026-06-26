// BigBubbleMuff — top-level DSP engine implementation (Phase A circuit model).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
//
// Signal chain (per the schematic, see docs/netlist.md):
//   IN -> [C1 high-pass] -> Q4 booster gain
//      -> SUSTAIN drive -> [C6 high-pass] -> clip stage 1 (WDF diode pair, C12)
//      -> stage-2 drive  -> [C7 high-pass] -> clip stage 2 (WDF diode pair, C11)
//      -> TONE stack (R23/C8/R5) -> Q1 recovery gain
//      -> [C2 high-pass / DC block] -> VOLUME -> OUT
// The nonlinear clippers run inside a 4x oversampled region to limit aliasing.
// Everything reaching the output is finiteness-guarded.
#include "dsp/BigMuffPi.h"

#include "dsp/DiodeClipper.h"
#include "dsp/Filters.h"
#include "dsp/Netlist.h"
#include "dsp/ToneStack.h"

#include <cmath>
#include <vector>

namespace bbm {

namespace {
constexpr int kOversampleFactor = 2; // 4x total (2^2) around the nonlinear core.

// Fixed gain staging (tuned from the schematic's high open-loop stage gains).
constexpr float kInputGain = 2.0f;    // Q4 input booster
constexpr float kStage2Drive = 40.0f; // fixed drive into the second clipper
constexpr float kRecoveryGain = 1.4f; // Q1 output recovery

// SUSTAIN maps to drive into clipper 1: dMin..dMax, geometric (knob feel).
constexpr float kDrive1Min = 2.0f;
constexpr float kDrive1Max = 300.0f;

inline float sustainToDrive(float sustain01) {
  const float s = juce::jlimit(0.0f, 1.0f, sustain01);
  return kDrive1Min * std::pow(kDrive1Max / kDrive1Min, s);
}

// Replace non-finite samples with silence to stop NaN/Inf escaping the engine.
inline float sanitise(float x) {
  return std::isfinite(x) ? x : 0.0f;
}

// Per-audio-channel circuit state (no heap once constructed).
struct Channel {
  OnePole inputHP;  // C1 1uF input coupling / DC block
  OnePole stage1HP; // C6 0.047uF coupling into clip 1
  DiodeClipper clip1;
  OnePole stage2HP; // C7 0.047uF coupling into clip 2
  DiodeClipper clip2;
  ToneStack tone;
  OnePole outputHP; // C2 1uF output coupling / DC block

  void prepare(double fs) {
    inputHP.prepare(fs, 15.0f);
    stage1HP.prepare(fs, 34.0f); // ~1/(2pi*100k*0.047uF)
    stage2HP.prepare(fs, 34.0f);
    outputHP.prepare(fs, 15.0f);
    clip1.prepare(fs);
    clip2.prepare(fs);
    tone.prepare(fs);
  }

  void reset() {
    inputHP.reset();
    stage1HP.reset();
    stage2HP.reset();
    outputHP.reset();
    clip1.reset();
    clip2.reset();
    tone.reset();
  }

  inline float process(float x, float drive1, float tone01) noexcept {
    x = inputHP.processHighpass(x) * kInputGain;

    // Stage 1: couple, drive, clip.
    x = stage1HP.processHighpass(x) * drive1;
    x = clip1.process(x);

    // Stage 2: couple, drive, clip.
    x = stage2HP.processHighpass(x) * kStage2Drive;
    x = clip2.process(x);

    // Tone + recovery + output coupling.
    tone.setTone(tone01);
    x = tone.process(x) * kRecoveryGain;
    return outputHP.processHighpass(x);
  }
};
} // namespace

struct BigMuffPi::Impl {
  double sampleRate = 44100.0;

  std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
  // Channels hold WDF objects with internal references -> non-movable; own each
  // via unique_ptr and construct in place.
  std::vector<std::unique_ptr<Channel>> channels;

  juce::SmoothedValue<float> drive1{sustainToDrive(0.75f)};
  juce::SmoothedValue<float> tone{0.5f};
  juce::SmoothedValue<float> volume{0.5f};
  juce::SmoothedValue<float> outputGain{1.0f};

  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;
    const double osRate = spec.sampleRate * (1 << kOversampleFactor);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels, kOversampleFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing(static_cast<size_t>(spec.maximumBlockSize));

    channels.clear();
    channels.reserve(spec.numChannels);
    for (juce::uint32 i = 0; i < spec.numChannels; ++i)
      channels.push_back(std::make_unique<Channel>());
    for (auto &ch : channels)
      ch->prepare(osRate); // circuit runs at the oversampled rate

    const double smooth = 0.02; // 20 ms control smoothing
    // drive/tone/volume are consumed inside the oversampled loop; outputGain at
    // base rate. Reset each at the rate it is advanced so 20 ms is accurate.
    drive1.reset(osRate, smooth);
    tone.reset(osRate, smooth);
    volume.reset(osRate, smooth);
    outputGain.reset(spec.sampleRate, smooth);

    reset();
  }

  void reset() {
    if (oversampler != nullptr)
      oversampler->reset();
    for (auto &ch : channels)
      ch->reset();
  }
};

BigMuffPi::BigMuffPi() : impl_(std::make_unique<Impl>()) {}
BigMuffPi::~BigMuffPi() = default;

void BigMuffPi::prepare(const juce::dsp::ProcessSpec &spec) {
  impl_->prepare(spec);
}

void BigMuffPi::reset() {
  impl_->reset();
}

void BigMuffPi::setControls(const Controls &c) {
  impl_->drive1.setTargetValue(sustainToDrive(c.sustain));
  impl_->tone.setTargetValue(juce::jlimit(0.0f, 1.0f, c.tone));
  impl_->volume.setTargetValue(juce::jlimit(0.0f, 1.0f, c.volume));
  impl_->outputGain.setTargetValue(
      juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 24.0f, c.outputTrimDb)));
}

int BigMuffPi::getOversamplingLatencySamples() const {
  return impl_->oversampler != nullptr
             ? static_cast<int>(impl_->oversampler->getLatencyInSamples())
             : 0;
}

void BigMuffPi::process(juce::dsp::AudioBlock<float> block) {
  auto &os = *impl_->oversampler;
  auto upBlock = os.processSamplesUp(block);

  const auto numCh = juce::jmin(upBlock.getNumChannels(), impl_->channels.size());
  const auto numSamp = upBlock.getNumSamples();

  for (size_t n = 0; n < numSamp; ++n) {
    const float drive1 = impl_->drive1.getNextValue();
    const float tone = impl_->tone.getNextValue();
    const float vol = impl_->volume.getNextValue();
    for (size_t ch = 0; ch < numCh; ++ch) {
      const auto ci = static_cast<int>(ch);
      float x = upBlock.getSample(ci, static_cast<int>(n));
      x = impl_->channels[ch]->process(sanitise(x), drive1, tone);
      upBlock.setSample(ci, static_cast<int>(n), sanitise(x * vol));
    }
  }

  os.processSamplesDown(block);

  // Output trim at base rate; advance the smoother once per sample (all channels).
  const auto outCh = block.getNumChannels();
  for (size_t n = 0; n < block.getNumSamples(); ++n) {
    const float g = impl_->outputGain.getNextValue();
    for (size_t ch = 0; ch < outCh; ++ch) {
      auto *d = block.getChannelPointer(ch);
      d[n] = sanitise(d[n] * g);
    }
  }
}

} // namespace bbm
