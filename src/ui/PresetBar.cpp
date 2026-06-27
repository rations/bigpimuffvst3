// BigBubbleMuff — preset bar implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PresetBar.h"

namespace bbm {

PresetBar::PresetBar(juce::AudioProcessorValueTreeState &apvts) : manager_(apvts) {
  presetBox_.setEditableText(true); // typing a new name + Save creates a preset
  presetBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff14160f));
  presetBox_.setColour(juce::ComboBox::textColourId, juce::Colour(0xffd8d8c4));
  presetBox_.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a3f30));
  presetBox_.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffd8d8c4));
  presetBox_.onChange = [this] {
    // Only a real dropdown pick has a non-zero id; freshly typed text does not,
    // so typing a name never triggers an accidental load.
    if (presetBox_.getSelectedId() > 0)
      manager_.load(presetBox_.getText());
  };
  addAndMakeVisible(presetBox_);

  saveButton_.onClick = [this] {
    const auto name = presetBox_.getText().trim();
    if (name.isNotEmpty() && manager_.save(name))
      refreshList(name);
  };
  addAndMakeVisible(saveButton_);

  deleteButton_.onClick = [this] { confirmDelete(); };
  addAndMakeVisible(deleteButton_);

  refreshList();
}

void PresetBar::refreshList(const juce::String &selectName) {
  presetBox_.clear(juce::dontSendNotification);

  const auto names = manager_.getPresetNames();
  int idToSelect = 0;
  for (int i = 0; i < names.size(); ++i) {
    presetBox_.addItem(names[i], i + 1);
    if (names[i] == selectName)
      idToSelect = i + 1;
  }

  presetBox_.setTextWhenNothingSelected(names.isEmpty() ? "No presets" : "Select preset");
  if (idToSelect > 0)
    presetBox_.setSelectedId(idToSelect, juce::dontSendNotification);
  else if (selectName.isNotEmpty())
    presetBox_.setText(selectName, juce::dontSendNotification);
}

void PresetBar::confirmDelete() {
  const auto name = presetBox_.getText().trim();
  if (name.isEmpty())
    return;

  // Two-button box: button[0] ("Delete") returns 1, button[1] ("Cancel") 0. The
  // associated component means this is auto-dismissed (result 0) if the bar dies.
  const auto options = juce::MessageBoxOptions()
                           .withIconType(juce::MessageBoxIconType::WarningIcon)
                           .withTitle("Delete preset")
                           .withMessage("Delete \"" + name + "\"?")
                           .withButton("Delete")
                           .withButton("Cancel")
                           .withAssociatedComponent(this);
  juce::AlertWindow::showAsync(options, [this, name](int result) {
    if (result == 1 && manager_.remove(name))
      refreshList();
  });
}

void PresetBar::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff20231b));
  g.setColour(juce::Colour(0xff0d0f08));
  g.fillRect(0, getHeight() - 1, getWidth(), 1); // hairline under the strip
}

void PresetBar::resized() {
  const int pad = juce::roundToInt(static_cast<float>(getHeight()) * 0.14f);
  const int gap = juce::roundToInt(static_cast<float>(getHeight()) * 0.12f);
  const int btnW = juce::roundToInt(static_cast<float>(getHeight()) * 1.4f);

  auto r = getLocalBounds().reduced(pad);
  deleteButton_.setBounds(r.removeFromRight(btnW));
  r.removeFromRight(gap);
  saveButton_.setBounds(r.removeFromRight(btnW));
  r.removeFromRight(gap);
  presetBox_.setBounds(r);
}

} // namespace bbm
