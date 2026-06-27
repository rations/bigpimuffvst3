// BigBubbleMuff — user preset store implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PresetManager.h"

#include "Parameters.h"
#include "StateIO.h"

namespace bbm {
namespace {

constexpr auto kPresetExtension = ".xml";
constexpr juce::int64 kMaxPresetBytes = 1 << 20; // 1 MiB, matches StateIO's cap.

} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState &apvts) : apvts_(apvts) {}

juce::File PresetManager::presetDirectory() {
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("BigBubbleMuff")
      .getChildFile("Presets");
}

juce::StringArray PresetManager::getPresetNames() const {
  juce::StringArray names;
  const auto dir = presetDirectory();
  if (!dir.isDirectory())
    return names;

  for (const auto &file : dir.findChildFiles(juce::File::findFiles, false,
                                             juce::String("*") + kPresetExtension))
    names.add(file.getFileNameWithoutExtension());

  names.sortNatural(); // case-insensitive, human-friendly ordering
  return names;
}

bool PresetManager::save(const juce::String &name) {
  const auto legal = juce::File::createLegalFileName(name).trim();
  if (legal.isEmpty())
    return false;

  const auto dir = presetDirectory();
  if (!dir.createDirectory())
    return false;

  auto state = apvts_.copyState();
  state.setProperty("stateVersion", kStateVersion, nullptr);
  auto xml = state.createXml();
  if (xml == nullptr)
    return false;

  const auto file = dir.getChildFile(legal + kPresetExtension);
  return xml->writeTo(file);
}

bool PresetManager::load(const juce::String &name) {
  const auto file = presetDirectory().getChildFile(name + kPresetExtension);
  // Untrusted on disk (users can edit/swap these): bound the size, then run the
  // same structural/version validation as the host state path.
  if (!file.existsAsFile() || file.getSize() <= 0 || file.getSize() > kMaxPresetBytes)
    return false;

  auto xml = juce::parseXML(file);
  if (xml == nullptr)
    return false;

  auto tree = validatePluginState(juce::ValueTree::fromXml(*xml), apvts_.state.getType());
  if (!tree)
    return false;

  apvts_.replaceState(*tree);
  return true;
}

bool PresetManager::remove(const juce::String &name) {
  const auto file = presetDirectory().getChildFile(name + kPresetExtension);
  return file.existsAsFile() && file.deleteFile();
}

} // namespace bbm
