// BigBubbleMuff — minimal generic editor implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "PluginEditor.h"

namespace bbm {

BigBubbleMuffEditor::BigBubbleMuffEditor(BigBubbleMuffProcessor &p)
    : juce::AudioProcessorEditor(p), generic_(p) {
  addAndMakeVisible(generic_);
  setResizable(true, true);
  setSize(420, 240);
}

void BigBubbleMuffEditor::resized() {
  generic_.setBounds(getLocalBounds());
}

} // namespace bbm
