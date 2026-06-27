// BigBubbleMuff — DSP engine unit tests.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "dsp/BigMuffPi.h"
#include "dsp/DiodeClipper.h"
#include "dsp/NoiseGate.h"
#include "dsp/ToneStack.h"
#include "dsp/TransistorStage.h"

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
      // prepare() settles the cascade, so even the first block is silent (no
      // turn-on pop). Residue is float rounding of the stages' multi-volt DC
      // bias removal (~-100 dBFS); a real DC leak/oscillation would dwarf this.
      // Re-clear every block: a real host delivers fresh input each call, so the
      // engine processes in place. Reusing the previous output as input would form
      // an artificial feedback loop and (given the cascade's true high gain) ring.
      float silMax = 0.0f;
      for (int i = 0; i < 8; ++i) {
        buf.clear();
        engine.process(block);
        silMax = juce::jmax(silMax, buf.getMagnitude(0, buf.getNumSamples()));
      }
      logMessage("silence residue max = " + juce::String(silMax));
      expect(silMax < 1.0e-4f, "engine produced output from silence");
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

class TransistorStageTests final : public juce::UnitTest {
public:
  TransistorStageTests() : juce::UnitTest("Phase B transistor stage") {}

  void runTest() override {
    const double fs = 192000.0; // representative oversampled rate

    beginTest("DC operating point is physically sane");
    {
      bbm::TransistorStage stage;
      stage.prepare(fs);
      const auto op = stage.operatingPoint();
      logMessage("DC bias  vb=" + juce::String(op.vb) + " vc=" + juce::String(op.vc) +
                 " ve=" + juce::String(op.ve));
      // Every node sits between the rails.
      expect(op.vb > 0.0f && op.vb < bbm::netlist::kSupplyV, "base between rails");
      expect(op.vc > 0.0f && op.vc < bbm::netlist::kSupplyV, "collector between rails");
      expect(op.ve > 0.0f && op.ve < bbm::netlist::kSupplyV, "emitter between rails");
      // Vbe is a forward silicon junction drop (transistor is biased on).
      expect((op.vb - op.ve) > 0.4f && (op.vb - op.ve) < 0.8f,
             "Vbe should be a forward junction drop");
      // With the diode clipping moved out of the DC feedback path, the stage is a
      // textbook collector-feedback common-emitter amp: the collector biases up
      // near mid-rail (active region, high gain), well above the base. This is the
      // gain that gives the Big Muff its sustain.
      expect(op.vc > 2.0f && op.vc < 7.0f, "collector biases near mid-rail (high gain)");
      expect((op.vc - op.vb) > 1.0f, "BC junction reverse-biased (not saturated)");
    }

    beginTest("zero input -> settled (near-zero AC) output");
    {
      bbm::TransistorStage stage;
      stage.prepare(fs);
      float maxAbs = 0.0f;
      for (int n = 0; n < 4096; ++n)
        maxAbs = juce::jmax(maxAbs, std::abs(stage.process(0.0f)));
      expect(maxAbs < 1.0e-3f, "no self-oscillation / drift at rest");
    }

    beginTest("large drive saturates against the rails, finite/bounded/asymmetric");
    {
      bbm::TransistorStage stage;
      stage.prepare(fs);
      double sumPos = 0.0, sumNeg = 0.0;
      float peak = 0.0f;
      double phase = 0.0;
      const double inc = juce::MathConstants<double>::twoPi * 300.0 / fs;
      bool finite = true;
      for (int n = 0; n < 8192; ++n) {
        const float x = 0.8f * static_cast<float>(std::sin(phase));
        const float y = stage.process(x);
        phase += inc;
        finite = finite && std::isfinite(y);
        peak = juce::jmax(peak, std::abs(y));
        if (n > 2048) {
          if (y > 0.0f)
            sumPos += y;
          else
            sumNeg += -y;
        }
      }
      expect(finite, "stage produced non-finite output");
      expect(peak < 20.0f, "collector swing bounded by the rails");
      // The collector bias is not exactly mid-rail, so railing the high-gain stage
      // clips the two half-cycles by different amounts (even-harmonic content).
      const double asym = std::abs(sumPos - sumNeg) / juce::jmax(1.0e-9, sumPos + sumNeg);
      logMessage("rail-clip asymmetry = " + juce::String(asym));
      expect(asym > 0.01, "rail clipping should be asymmetric (even harmonics)");
    }
  }
};

class DiodeClipperTests final : public juce::UnitTest {
public:
  DiodeClipperTests() : juce::UnitTest("Antiparallel diode clipper") {}

  void runTest() override {
    beginTest("clamps large drive to ~a diode drop, stays finite");
    {
      bbm::DiodeClipper clip;
      clip.prepare();
      float peak = 0.0f;
      bool finite = true;
      for (int n = 0; n < 4096; ++n) {
        // Sweep well past the clip threshold in both directions.
        const float x = (n % 2 == 0 ? 1.0f : -1.0f) * 12.0f;
        const float y = clip.process(x);
        finite = finite && std::isfinite(y);
        peak = juce::jmax(peak, std::abs(y));
      }
      logMessage("diode clamp peak = " + juce::String(peak));
      expect(finite, "clipper produced non-finite output");
      // 1N914 antiparallel pair clamps to roughly +/-0.4..0.7 V even when driven
      // many volts past the knee.
      expect(peak > 0.3f && peak < 0.8f, "clamp lands near a diode forward drop");
    }

    beginTest("near-transparent below the knee");
    {
      bbm::DiodeClipper clip;
      clip.prepare();
      // A small signal should pass almost unchanged (diodes barely conduct).
      double sumErr = 0.0;
      const double inc = juce::MathConstants<double>::twoPi * 1000.0 / 192000.0;
      double phase = 0.0;
      for (int n = 0; n < 2048; ++n) {
        const float x = 0.05f * static_cast<float>(std::sin(phase));
        const float y = clip.process(x);
        phase += inc;
        sumErr += std::abs(static_cast<double>(y - x));
      }
      expect(sumErr / 2048.0 < 0.01, "small signals pass nearly unclipped");
    }
  }
};

class NoiseGateTests final : public juce::UnitTest {
public:
  NoiseGateTests() : juce::UnitTest("Pre-gain noise gate") {}

  void runTest() override {
    constexpr double fs = 48000.0;

    beginTest("passes a loud signal nearly unchanged when open");
    {
      bbm::NoiseGate gate;
      gate.prepare(fs);
      gate.setAmount(0.4f); // open threshold ~ -50 dBFS
      double maxErr = 0.0;
      const double inc = juce::MathConstants<double>::twoPi * 220.0 / fs;
      double phase = 0.0;
      for (int n = 0; n < 8192; ++n) {
        const float x = 0.2f * static_cast<float>(std::sin(phase)); // ~ -14 dBFS
        const float y = gate.process(x);
        phase += inc;
        if (n > 4096) // after attack: gate fully open
          maxErr = juce::jmax(maxErr, std::abs(static_cast<double>(y - x)));
      }
      expect(maxErr < 1.0e-3, "open gate is transparent to a loud signal");
    }

    beginTest("closes to true silence on a quiet idle floor");
    {
      bbm::NoiseGate gate;
      gate.prepare(fs);
      gate.setAmount(0.4f);
      juce::Random rng(1234);
      const auto noise = [&rng] { return 1.0e-3f * (2.0f * rng.nextFloat() - 1.0f); };
      // ~1.5 s of quiet noise (-60 dBFS-ish): well past hold + release.
      double tailRms = 0.0;
      int tailN = 0;
      for (int n = 0; n < 72000; ++n) {
        const float y = gate.process(noise());
        if (n > 67000) { // steady state: gate fully closed
          tailRms += static_cast<double>(y) * y;
          ++tailN;
        }
      }
      tailRms = std::sqrt(tailRms / juce::jmax(1, tailN));
      logMessage("gated idle tail RMS = " + juce::String(tailRms));
      expect(tailRms < 1.0e-6, "gate mutes a quiet idle floor");
    }

    beginTest("amount = 0 disables the gate (bit-transparent)");
    {
      bbm::NoiseGate gate;
      gate.prepare(fs);
      gate.setAmount(0.0f);
      juce::Random rng(99);
      float maxDiff = 0.0f;
      for (int n = 0; n < 4096; ++n) {
        const float x = 1.0e-3f * (2.0f * rng.nextFloat() - 1.0f);
        maxDiff = juce::jmax(maxDiff, std::abs(gate.process(x) - x));
      }
      // <= avoids -Wfloat-equal; true only when every sample passed bit-exact.
      expect(maxDiff <= 0.0f, "disabled gate passes the input unchanged");
    }
  }
};

BigMuffEngineTests g_bigMuffEngineTests;
ToneStackTests g_toneStackTests;
TransistorStageTests g_transistorStageTests;
DiodeClipperTests g_diodeClipperTests;
NoiseGateTests g_noiseGateTests;

} // namespace
