// BigBubbleMuff — AudioProcessor implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "StateIO.h"

namespace bbm {

BigBubbleMuffProcessor::BigBubbleMuffProcessor()
    : juce::AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "BigBubbleMuff", createParameterLayout()),
      // apvts_ is fully constructed by this point (it precedes these in
      // declaration order), so the raw-value pointers can be cached here.
      sustainParam_(apvts_.getRawParameterValue(pid::sustain)),
      toneParam_(apvts_.getRawParameterValue(pid::tone)),
      volumeParam_(apvts_.getRawParameterValue(pid::volume)),
      outputParam_(apvts_.getRawParameterValue(pid::output)),
      gateParam_(apvts_.getRawParameterValue(pid::gate)),
      bypassParam_(apvts_.getRawParameterValue(pid::bypass)) {}

bool BigBubbleMuffProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
  const auto &out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
    return false;
  // Input and output channel layouts must match.
  return out == layouts.getMainInputChannelSet();
}

void BigBubbleMuffProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  juce::dsp::ProcessSpec spec{};
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock));
  spec.numChannels =
      static_cast<juce::uint32>(juce::jmax(1, getTotalNumOutputChannels()));

  engine_.prepare(spec);
  setLatencySamples(engine_.getOversamplingLatencySamples());
}

void BigBubbleMuffProcessor::pullControls() {
  Controls c;
  c.sustain = sustainParam_->load();
  c.tone = toneParam_->load();
  c.volume = volumeParam_->load();
  c.outputTrimDb = outputParam_->load();
  c.gate = gateParam_->load();
  engine_.setControls(c);
}

void BigBubbleMuffProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                          juce::MidiBuffer &) {
  juce::ScopedNoDenormals noDenormals;

  const int totalIn = getTotalNumInputChannels();
  const int totalOut = getTotalNumOutputChannels();
  for (int ch = totalIn; ch < totalOut; ++ch)
    buffer.clear(ch, 0, buffer.getNumSamples());

  if (bypassParam_->load() >= 0.5f)
    return; // host-visible bypass: leave the buffer untouched.

  pullControls();

  juce::dsp::AudioBlock<float> block(buffer);
  engine_.process(block);
}

juce::AudioProcessorEditor *BigBubbleMuffProcessor::createEditor() {
  // JUCE contract: the host takes ownership of the returned editor (framework
  // entry points are exempt from the project's no-raw-`new` rule).
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return new BigBubbleMuffEditor(*this);
}

void BigBubbleMuffProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = apvts_.copyState();
  state.setProperty("stateVersion", kStateVersion, nullptr);
  if (auto xml = state.createXml())
    copyXmlToBinary(*xml, destData);
}

void BigBubbleMuffProcessor::setStateInformation(const void *data, int sizeInBytes) {
  // Host-supplied (untrusted) blob: parse + validate defensively before applying
  // (shared with the on-disk preset loader so both paths enforce the same rules).
  if (auto tree = parsePluginStateBinary(data, sizeInBytes, apvts_.state.getType()))
    apvts_.replaceState(*tree);
}

} // namespace bbm

// Plugin entry point. JUCE/the host takes ownership of the returned processor
// (this framework entry point is exempt from the project's no-raw-`new` rule).
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  return new bbm::BigBubbleMuffProcessor();
}
