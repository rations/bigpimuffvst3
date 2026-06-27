// BigBubbleMuff — top-level DSP engine implementation (Phase B circuit model).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
//
// Signal chain (per the schematic, see docs/netlist.md):
//   IN -> [C1 high-pass] -> Q4 booster gain
//      -> SUSTAIN drive -> clip stage 1 (nodal BJT + diode pair, C6 coupling)
//      -> interstage    -> clip stage 2 (nodal BJT + diode pair, C7 coupling)
//      -> TONE stack (R23/C8/R5) -> Q1 recovery gain
//      -> [C2 high-pass / DC block] -> VOLUME -> OUT
// The two clipping stages are full nodal circuit models (Ebers-Moll BJT with the
// antiparallel diode pair jointly inside the collector->base feedback loop, see
// TransistorStage.h). Each stage carries its own input coupling capacitor, so no
// separate coupling high-pass is needed in front of it. The nonlinear core runs
// inside a 4x oversampled region to limit aliasing; everything reaching the
// output is finiteness-guarded.
#include "dsp/BigMuffPi.h"

#include "dsp/DiodeClipper.h"
#include "dsp/Filters.h"
#include "dsp/Netlist.h"
#include "dsp/NoiseGate.h"
#include "dsp/ToneStack.h"
#include "dsp/TransistorStage.h"

#include <cmath>

namespace bbm {

namespace {
constexpr int kOversampleFactor = 2; // 4x total (2^2) around the nonlinear core.

// All four Big Muff stages are real common-emitter transistor stages (see
// TransistorStage), each with ~19x gain. The two clip stages add an antiparallel
// diode-pair limiter (DiodeClipper) on their output, clamping the swing to
// ~+/-0.5 V — the Big Muff fuzz. The high stage gain is what gives the pedal its
// sustain: even a tiny input is amplified into the clippers. The only scalars are
// the SUSTAIN pot divider feeding clip 1 and a final scale to plug-in full scale.
constexpr float kOutputScale = 0.18f; // circuit volts -> ~unity at the output

// SUSTAIN maps to the divider feeding clip 1 (R24 pot): dMin..dMax, geometric
// (knob feel). With ~19x booster gain ahead of it, even a small fraction pushes
// the clippers into clipping at high settings (max sustain/compression); low
// settings keep quiet playing below the diode knee for a cleaner edge.
constexpr float kDrive1Min = 0.005f;
constexpr float kDrive1Max = 0.6f;

inline float sustainToDrive(float sustain01) {
  const float s = juce::jlimit(0.0f, 1.0f, sustain01);
  return kDrive1Min * std::pow(kDrive1Max / kDrive1Min, s);
}

// Replace non-finite samples with silence to stop NaN/Inf escaping the engine.
inline float sanitise(float x) {
  return std::isfinite(x) ? x : 0.0f;
}

// Per-audio-channel circuit state (no heap once constructed). The four NPN
// stages of the real pedal, in order: input booster, two diode clippers, output
// recovery — each a nodal TransistorStage.
struct Channel {
  OnePole inputHP;         // C1 1uF input coupling / DC block
  TransistorStage booster; // Q4 input booster gain stage
  TransistorStage clip1;   // Q3 gain stage
  DiodeClipper clip1Diode; // D1 antiparallel pair across Q3 feedback
  TransistorStage clip2;   // Q2 gain stage
  DiodeClipper clip2Diode; // D2 antiparallel pair across Q2 feedback
  ToneStack tone;
  TransistorStage recovery; // Q1 output recovery gain stage
  OnePole outputHP;         // C2 1uF output coupling / DC block

  void prepare(double fs) {
    inputHP.prepare(fs, 15.0f);
    outputHP.prepare(fs, 15.0f);
    booster.prepare(fs);
    clip1.prepare(fs);
    clip1Diode.prepare();
    clip2.prepare(fs);
    clip2Diode.prepare();
    recovery.prepare(fs);
    tone.prepare(fs);
  }

  void reset() {
    inputHP.reset();
    outputHP.reset();
    booster.reset();
    clip1.reset();
    clip1Diode.reset();
    clip2.reset();
    clip2Diode.reset();
    recovery.reset();
    tone.reset();
  }

  inline float process(float x, float drive1, float tone01) noexcept {
    x = inputHP.processHighpass(x);

    // Q4 booster brings the guitar level up into the clippers.
    x = booster.process(x);

    // SUSTAIN pot divides the booster output into the first high-gain clip stage;
    // each clip stage amplifies then the antiparallel diode pair clamps the swing
    // (~+/-0.5 V) — the soft, sustaining Big Muff fuzz.
    x = clip1Diode.process(clip1.process(x * drive1));
    x = clip2Diode.process(clip2.process(x));

    // Passive tone stack, then the Q1 recovery stage makes up its loss.
    tone.setTone(tone01);
    x = tone.process(x);
    x = recovery.process(x);

    return outputHP.processHighpass(x) * kOutputScale;
  }
};
} // namespace

struct BigMuffPi::Impl {
  double sampleRate = 44100.0;

  // The Big Muff is a mono pedal: one input jack, one circuit, one output jack.
  // We solve a single mono circuit (the expensive Newton nodal solve) and fan the
  // result out to however many output channels the host wants. Processing per
  // channel would just solve the identical guitar signal twice. Hence one mono
  // oversampler, one Channel, and a mono scratch buffer for the downmixed input.
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
  // The Channel holds WDF objects with internal references -> non-movable; own it
  // via unique_ptr and construct in place.
  std::unique_ptr<Channel> channel;
  juce::AudioBuffer<float> monoBuf; // downmixed mono input/output, base rate
  NoiseGate gate;                   // pre-gain input gate (base rate, pre-oversample)

  juce::SmoothedValue<float> drive1{sustainToDrive(0.75f)};
  juce::SmoothedValue<float> tone{0.5f};
  juce::SmoothedValue<float> volume{0.5f};
  juce::SmoothedValue<float> outputGain{1.0f};
  float gateAmount = 0.4f; // set from setControls (audio thread), applied per block

  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;
    const double osRate = spec.sampleRate * (1 << kOversampleFactor);

    // One mono oversampler / circuit (see above).
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        1, kOversampleFactor, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing(static_cast<size_t>(spec.maximumBlockSize));

    monoBuf.setSize(1, static_cast<int>(spec.maximumBlockSize), false, false, true);

    // The gate runs on the clean mono input at the base rate, before oversampling.
    gate.prepare(spec.sampleRate);

    channel = std::make_unique<Channel>();
    channel->prepare(osRate); // circuit runs at the oversampled rate

    const double smooth = 0.02; // 20 ms control smoothing
    // drive/tone/volume are consumed inside the oversampled loop; outputGain at
    // base rate. Reset each at the rate it is advanced so 20 ms is accurate.
    drive1.reset(osRate, smooth);
    tone.reset(osRate, smooth);
    volume.reset(osRate, smooth);
    outputGain.reset(spec.sampleRate, smooth);

    reset();
    settle();
  }

  // Cheap, real-time-safe: zero filter/oversampler state. Safe to call from a
  // host's audio-thread reset().
  void reset() {
    if (oversampler != nullptr)
      oversampler->reset();
    if (channel != nullptr)
      channel->reset();
    gate.reset();
  }

  // Flush the turn-on transient. The stages' DC operating points relax through
  // the AC-coupled cascade (and are amplified by the recovery stage) over
  // ~100 ms; run silence through so the first audio block starts settled and the
  // plug-in emits no turn-on pop. Expensive — call only from prepare() on the
  // message thread, never from the audio thread.
  void settle() {
    const float d = drive1.getTargetValue();
    const float t = tone.getTargetValue();
    const int n =
        static_cast<int>(sampleRate * static_cast<double>(1 << kOversampleFactor) * 0.2);
    for (int i = 0; i < n; ++i)
      channel->process(0.0f, d, t);
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
  impl_->gateAmount = juce::jlimit(0.0f, 1.0f, c.gate);
}

int BigMuffPi::getOversamplingLatencySamples() const {
  return impl_->oversampler != nullptr
             ? static_cast<int>(impl_->oversampler->getLatencyInSamples())
             : 0;
}

void BigMuffPi::process(juce::dsp::AudioBlock<float> block) {
  auto &os = *impl_->oversampler;

  const auto numInCh = block.getNumChannels();
  const auto numSamp = block.getNumSamples();
  if (numInCh == 0 || numSamp == 0)
    return;

  // Downmix to a single mono input (a Big Muff has one input jack). Averaging the
  // channels keeps a mono-duplicated stereo feed identical to a true mono feed,
  // and avoids the +6 dB a naive sum would add into the clippers. The pre-gain
  // noise gate then acts on this clean input, before the high-gain stages amplify
  // its noise floor into hiss (see NoiseGate.h).
  impl_->gate.setAmount(impl_->gateAmount);
  auto *mono = impl_->monoBuf.getWritePointer(0);
  const float invCh = 1.0f / static_cast<float>(numInCh);
  for (size_t n = 0; n < numSamp; ++n) {
    float acc = 0.0f;
    for (size_t ch = 0; ch < numInCh; ++ch)
      acc += block.getSample(static_cast<int>(ch), static_cast<int>(n));
    mono[n] = impl_->gate.process(sanitise(acc * invCh));
  }

  // Solve the one mono circuit at the oversampled rate.
  juce::dsp::AudioBlock<float> monoBlock(impl_->monoBuf);
  auto baseMono = monoBlock.getSubBlock(0, numSamp);
  auto upMono = os.processSamplesUp(baseMono);
  const auto upSamp = upMono.getNumSamples();
  for (size_t n = 0; n < upSamp; ++n) {
    const float drive1 = impl_->drive1.getNextValue();
    const float tone = impl_->tone.getNextValue();
    const float vol = impl_->volume.getNextValue();
    float x = upMono.getSample(0, static_cast<int>(n));
    x = impl_->channel->process(sanitise(x), drive1, tone);
    upMono.setSample(0, static_cast<int>(n), sanitise(x * vol));
  }
  os.processSamplesDown(baseMono);

  // Output trim at base rate, then fan the mono result out to every output
  // channel (dual-mono on a stereo bus).
  for (size_t n = 0; n < numSamp; ++n) {
    const float g = impl_->outputGain.getNextValue();
    const float out = sanitise(mono[n] * g);
    for (size_t ch = 0; ch < numInCh; ++ch)
      block.setSample(static_cast<int>(ch), static_cast<int>(n), out);
  }
}

} // namespace bbm
