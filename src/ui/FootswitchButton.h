// BigBubbleMuff — chrome stompbox footswitch button.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bbm {

// Round footswitch that paints the depressed image when engaged and the raised
// image when bypassed. Visual state is driven externally (setEngaged) from the
// bypass parameter; the owning editor wires onClick to flip that parameter.
class FootswitchButton final : public juce::Button {
public:
  FootswitchButton();
  ~FootswitchButton() override = default;

  // Framework object: non-copyable (JUCE macro below) and non-movable.
  FootswitchButton(FootswitchButton &&) = delete;
  FootswitchButton &operator=(FootswitchButton &&) = delete;

  void setEngaged(bool engaged);

  // Restrict clicks to the circular button so the transparent corners are inert.
  bool hitTest(int x, int y) override;
  void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted,
                   bool shouldDrawButtonAsDown) override;

private:
  juce::Image up_;
  juce::Image down_;
  bool engaged_ = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FootswitchButton)
};

} // namespace bbm
