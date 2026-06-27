// BigBubbleMuff — user preset store (save/load knob positions to disk).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bbm {

// Reads and writes user presets as plain, human-readable .xml files under a fixed
// app-data folder (~/.config/BigBubbleMuff/Presets on Linux). All I/O happens on
// the message thread (never from processBlock), and loading reuses the same
// untrusted-blob validation as the host state restore path (see StateIO.h).
class PresetManager final {
public:
  explicit PresetManager(juce::AudioProcessorValueTreeState &apvts);
  ~PresetManager() = default;

  PresetManager(const PresetManager &) = delete;
  PresetManager &operator=(const PresetManager &) = delete;
  PresetManager(PresetManager &&) = delete;
  PresetManager &operator=(PresetManager &&) = delete;

  // ~/.config/BigBubbleMuff/Presets (created on demand by save()).
  static juce::File presetDirectory();

  // Names (without the .xml extension) of the presets currently on disk, sorted
  // case-insensitively.
  juce::StringArray getPresetNames() const;

  // Capture the current parameter state under the given name. Returns false if
  // the name is empty or the file could not be written.
  bool save(const juce::String &name);

  // Restore a named preset into the APVTS. Returns false if the file is missing,
  // too large, malformed, or from an incompatible schema.
  bool load(const juce::String &name);

  // Delete a named preset. Returns false if it does not exist.
  bool remove(const juce::String &name);

private:
  juce::AudioProcessorValueTreeState &apvts_;

  JUCE_LEAK_DETECTOR(PresetManager)
};

} // namespace bbm
