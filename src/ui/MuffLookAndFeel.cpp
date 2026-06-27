// BigBubbleMuff — LookAndFeel implementation (image-rotation rotary knobs).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#include "MuffLookAndFeel.h"

#include "BinaryData.h"

namespace bbm {

MuffLookAndFeel::MuffLookAndFeel()
    : knob_(juce::ImageCache::getFromMemory(BinaryData::dialknob_png,
                                            BinaryData::dialknob_pngSize)) {}

void MuffLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width,
                                       int height, float sliderPosProportional,
                                       float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider &) {
  if (!knob_.isValid()) {
    return;
  }

  // Largest centred square that fits the slider bounds (the knob art is square).
  const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
  const float side = juce::jmin(area.getWidth(), area.getHeight());
  const auto box = juce::Rectangle<float>(side, side).withCentre(area.getCentre());

  const float angle =
      rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
  const float scale = side / static_cast<float>(knob_.getWidth());

  // Scale the source art to the box, place it, then rotate about the box centre.
  const auto transform = juce::AffineTransform::scale(scale)
                             .translated(box.getX(), box.getY())
                             .rotated(angle, box.getCentreX(), box.getCentreY());

  g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
  g.drawImageTransformed(knob_, transform);
}

} // namespace bbm
