// BigBubbleMuff — DSP engine unit tests.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "dsp/BigMuffPi.h"
#include "dsp/ToneStack.h"

#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace {

juce::dsp::ProcessSpec makeSpec(double fs, int block, int ch) {
  return {fs, static_cast<juce::uint32>(block), static_cast<juce::uint32>(ch)};
}

bool allFinite(const juce::AudioBuffer<float> &b) {
  for (int ch = 0; ch < b.getNumChannels(); ++ch)
    for (int n = 0; n < b.getNumSamples(); ++n)
      if (!std::isfinite(b.getSample(ch, n)))
        return false;
  return true;
}

// Fills a buffer with a sine, advancing `phase` so successive blocks stay
// phase-continuous (no per-block discontinuity that would ring the oversampler).
void fillSine(juce::AudioBuffer<float> &b, double fs, double freq, float amp,
              double &phase) {
  const double inc = juce::MathConstants<double>::twoPi * freq / fs;
  for (int n = 0; n < b.getNumSamples(); ++n) {
    const float s = amp * static_cast<float>(std::sin(phase));
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
      b.setSample(ch, n, s);
    phase += inc;
    if (phase > juce::MathConstants<double>::twoPi)
      phase -= juce::MathConstants<double>::twoPi;
  }
}

// Linear RMS gain of the tone stack at a single frequency.
float measureToneGain(float tone, double fs, double freq) {
  bbm::ToneStack ts;
  ts.prepare(fs);
  ts.setTone(tone);
  const double inc = juce::MathConstants<double>::twoPi * freq / fs;
  double phase = 0.0;
  double sumIn = 0.0, sumOut = 0.0;
  const int warmup = static_cast<int>(fs * 0.05);
  const int measure = static_cast<int>(fs * 0.2);
  for (int n = 0; n < warmup + measure; ++n) {
    const float x = static_cast<float>(std::sin(phase));
    const float y = ts.process(x);
    phase += inc;
    if (n >= warmup) {
      sumIn += static_cast<double>(x) * x;
      sumOut += static_cast<double>(y) * y;
    }
  }
  return static_cast<float>(std::sqrt(sumOut / juce::jmax(1.0e-12, sumIn)));
}

// Ratio of harmonic energy to fundamental energy for a pure sine into the engine.
// f0 is aligned to an FFT bin (no leakage). Higher = more distortion.
float measureHarmonicRatio(float sustain, double fs) {
  constexpr int order = 12;
  constexpr int N = 1 << order; // 4096
  constexpr int k = 20;         // fundamental bin
  const double f0 = fs * k / N;

  bbm::BigMuffPi engine;
  engine.prepare(makeSpec(fs, N, 1));
  bbm::Controls c;
  c.sustain = sustain;
  c.tone = 0.5f;
  c.volume = 1.0f;
  engine.setControls(c);

  juce::AudioBuffer<float> buf(1, N);
  juce::dsp::AudioBlock<float> block(buf);
  double phase = 0.0;
  for (int i = 0; i < 6; ++i) { // warm up oversampler + smoothers
    fillSine(buf, fs, f0, 0.3f, phase);
    engine.process(block);
  }
  fillSine(buf, fs, f0, 0.3f, phase);
  engine.process(block);

  juce::dsp::FFT fft(order);
  std::vector<float> fftData(2 * N, 0.0f);
  for (int n = 0; n < N; ++n)
    fftData[static_cast<size_t>(n)] = buf.getSample(0, n);
  fft.performFrequencyOnlyForwardTransform(fftData.data());

  const double fund = fftData[k];
  double harm = 0.0;
  for (int h = 2; h * k < N / 2; ++h)
    harm += static_cast<double>(fftData[h * k]) * fftData[h * k];
  return static_cast<float>(std::sqrt(harm) / juce::jmax(1.0e-9, fund));
}

class BigMuffEngineTests final : public juce::UnitTest {
public:
  BigMuffEngineTests() : juce::UnitTest("BigMuffPi engine") {}

  void runTest() override {
    const std::vector<double> rates{44100.0, 48000.0, 96000.0};

    beginTest("prepare + process stays finite across sample rates");
    for (double fs : rates) {
      bbm::BigMuffPi engine;
      engine.prepare(makeSpec(fs, 512, 2));
      bbm::Controls c;
      c.sustain = 1.0f;
      c.tone = 0.5f;
      c.volume = 1.0f;
      engine.setControls(c);

      juce::AudioBuffer<float> buf(2, 512);
      juce::dsp::AudioBlock<float> block(buf);
      double phase = 0.0;
      for (int i = 0; i < 8; ++i) {
        fillSine(buf, fs, 220.0, 0.9f, phase);
        engine.process(block);
        if (!allFinite(buf))
          break;
      }
      expect(allFinite(buf), "non-finite output at fs=" + juce::String(fs));
    }

    beginTest("rejects non-finite input without propagating NaN/Inf");
    {
      bbm::BigMuffPi engine;
      engine.prepare(makeSpec(48000.0, 256, 1));
      juce::AudioBuffer<float> buf(1, 256);
      buf.clear();
      buf.setSample(0, 10, std::numeric_limits<float>::infinity());
      buf.setSample(0, 20, std::numeric_limits<float>::quiet_NaN());
      juce::dsp::AudioBlock<float> block(buf);
      engine.process(block);
      expect(allFinite(buf), "NaN/Inf leaked through the engine");
    }

    beginTest("output stays bounded at full drive");
    {
      bbm::BigMuffPi engine;
      engine.prepare(makeSpec(48000.0, 512, 1));
      bbm::Controls c;
      c.sustain = 1.0f;
      c.volume = 1.0f;
      c.outputTrimDb = 24.0f; // ~15.85x — bounded output must track this, not blow up.
      engine.setControls(c);
      juce::AudioBuffer<float> buf(1, 512);
      juce::dsp::AudioBlock<float> block(buf);
      double phase = 0.0;
      float peak = 0.0f;
      for (int i = 0; i < 16; ++i) {
        fillSine(buf, 48000.0, 110.0, 1.0f, phase);
        engine.process(block);
        peak = juce::jmax(peak, buf.getMagnitude(0, buf.getNumSamples()));
      }
      // 24 dB gain on a unit sine ~= 16; allow oversampler ripple headroom.
      expect(peak < 32.0f, "output magnitude implausibly large (runaway?)");
    }

    beginTest("silence in -> silence out");
    {
      bbm::BigMuffPi engine;
      engine.prepare(makeSpec(44100.0, 128, 2));
      juce::AudioBuffer<float> buf(2, 128);
      buf.clear();
      juce::dsp::AudioBlock<float> block(buf);
      for (int i = 0; i < 4; ++i)
        engine.process(block);
      expect(buf.getMagnitude(0, buf.getNumSamples()) < 1.0e-6f,
             "engine produced output from silence");
    }

    beginTest("distortion increases with the Sustain knob");
    {
      const float lo = measureHarmonicRatio(0.05f, 48000.0);
      const float hi = measureHarmonicRatio(1.0f, 48000.0);
      logMessage("harmonic ratio: low sustain=" + juce::String(lo) +
                 " high sustain=" + juce::String(hi));
      expect(std::isfinite(lo) && std::isfinite(hi), "non-finite THD measurement");
      expect(hi > lo, "more Sustain should produce more harmonic distortion");
      expect(hi > 0.1f, "max Sustain should be strongly distorted (fuzz)");
    }
  }
};

class ToneStackTests final : public juce::UnitTest {
public:
  ToneStackTests() : juce::UnitTest("Big Muff tone stack") {}

  void runTest() override {
    const double fs = 48000.0;

    beginTest("tone fully CCW favours bass over treble");
    {
      const float bass = measureToneGain(0.0f, fs, 100.0);
      const float treble = measureToneGain(0.0f, fs, 5000.0);
      expect(bass > treble, "CCW tone should attenuate treble vs bass");
    }

    beginTest("tone fully CW favours treble over bass");
    {
      const float bass = measureToneGain(1.0f, fs, 100.0);
      const float treble = measureToneGain(1.0f, fs, 5000.0);
      expect(treble > bass, "CW tone should attenuate bass vs treble");
    }

    beginTest("centre tone has a mid-scoop notch");
    {
      const float low = measureToneGain(0.5f, fs, 120.0);
      const float mid = measureToneGain(0.5f, fs, 900.0);
      const float high = measureToneGain(0.5f, fs, 4000.0);
      logMessage("tone@0.5 gains  low=" + juce::String(low) +
                 " mid=" + juce::String(mid) + " high=" + juce::String(high));
      expect(mid < low && mid < high, "mids should be scooped vs low and high");
    }
  }
};

BigMuffEngineTests g_bigMuffEngineTests;
ToneStackTests g_toneStackTests;

} // namespace
