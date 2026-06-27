// BigBubbleMuff — LookAndFeel that draws rotary knobs from the dial image.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bbm {

// Renders rotary sliders by rotating the supplied knob bitmap (which already
// carries a pointer) about its centre — unlike a procedurally drawn arc. The
// image is loaded once from embedded binary data and shared by every knob.
class MuffLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  MuffLookAndFeel();
  ~MuffLookAndFeel() override = default;

  // Framework object: non-copyable (JUCE macro below) and non-movable.
  MuffLookAndFeel(MuffLookAndFeel &&) = delete;
  MuffLookAndFeel &operator=(MuffLookAndFeel &&) = delete;

  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override;

private:
  juce::Image knob_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MuffLookAndFeel)
};

} // namespace bbm
