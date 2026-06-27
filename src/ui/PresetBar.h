// BigBubbleMuff — slim preset bar (dropdown + save/delete) above the pedal.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "../PresetManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace bbm {

// A thin header strip holding an editable preset dropdown plus Save and Delete
// buttons. Selecting a name loads it; typing a new name and pressing Save stores
// the current knob positions. Owns its PresetManager and does all I/O on the
// message thread.
class PresetBar final : public juce::Component {
public:
  explicit PresetBar(juce::AudioProcessorValueTreeState &apvts);
  ~PresetBar() override = default;

  // Framework object: non-copyable (JUCE macro below) and non-movable.
  PresetBar(PresetBar &&) = delete;
  PresetBar &operator=(PresetBar &&) = delete;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  // Repopulate the dropdown from disk, optionally re-selecting a given name.
  void refreshList(const juce::String &selectName = {});
  void confirmDelete();

  PresetManager manager_;
  juce::ComboBox presetBox_;
  juce::TextButton saveButton_{"Save"};
  juce::TextButton deleteButton_{"Del"};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};

} // namespace bbm
