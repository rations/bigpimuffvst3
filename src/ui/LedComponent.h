// BigBubbleMuff — bypass indicator LED (swaps the on/off lamp images).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "BinaryData.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace bbm {

// Draws the lit lamp (onlightmuff) when the pedal is engaged and the dark lamp
// (offlightmuff) when bypassed. Purely indicative — it never intercepts mouse
// events (the footswitch owns the toggle).
class LedComponent final : public juce::Component {
public:
  LedComponent()
      : on_(juce::ImageCache::getFromMemory(BinaryData::onlightmuff_png,
                                            BinaryData::onlightmuff_pngSize)),
        off_(juce::ImageCache::getFromMemory(BinaryData::offlightmuff_png,
                                             BinaryData::offlightmuff_pngSize)) {
    setInterceptsMouseClicks(false, false);
  }

  ~LedComponent() override = default;

  // Framework object: non-copyable (JUCE macro below) and non-movable.
  LedComponent(LedComponent &&) = delete;
  LedComponent &operator=(LedComponent &&) = delete;

  void setOn(bool shouldBeOn) {
    if (shouldBeOn != isOn_) {
      isOn_ = shouldBeOn;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    const auto &img = isOn_ ? on_ : off_;
    if (img.isValid()) {
      g.drawImage(img, getLocalBounds().toFloat(),
                  juce::RectanglePlacement::stretchToFit);
    }
  }

private:
  juce::Image on_;
  juce::Image off_;
  bool isOn_ = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LedComponent)
};

} // namespace bbm
