// BigBubbleMuff — footswitch button implementation.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "FootswitchButton.h"

#include "BinaryData.h"

namespace bbm {

FootswitchButton::FootswitchButton()
    : juce::Button("Footswitch"),
      up_(juce::ImageCache::getFromMemory(BinaryData::footswitch_up_png,
                                          BinaryData::footswitch_up_pngSize)),
      down_(juce::ImageCache::getFromMemory(BinaryData::footswitch_down_png,
                                            BinaryData::footswitch_down_pngSize)) {}

void FootswitchButton::setEngaged(bool engaged) {
  if (engaged != engaged_) {
    engaged_ = engaged;
    repaint();
  }
}

bool FootswitchButton::hitTest(int x, int y) {
  const auto centre = getLocalBounds().getCentre().toFloat();
  const float radius = static_cast<float>(juce::jmin(getWidth(), getHeight())) * 0.5f;
  const juce::Point<float> p{static_cast<float>(x), static_cast<float>(y)};
  return centre.getDistanceFrom(p) <= radius;
}

void FootswitchButton::paintButton(juce::Graphics &g, bool, bool) {
  const auto &img = engaged_ ? down_ : up_;
  if (img.isValid()) {
    g.drawImage(img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
  }
}

} // namespace bbm
