// BigBubbleMuff — shared validation for untrusted plugin-state blobs.
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
#pragma once

#include "Parameters.h"

#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace bbm {

// Validate an untrusted state tree against this plugin's schema. Returns the tree
// when it is structurally valid and not from a newer schema version, otherwise
// nullopt. APVTS itself clamps every restored parameter to its NormalisableRange,
// so callers only need this structural/version gate before replaceState().
inline std::optional<juce::ValueTree>
validatePluginState(juce::ValueTree tree, const juce::Identifier &expectedType) {
  if (!tree.isValid() || tree.getType() != expectedType)
    return std::nullopt;

  // Reject blobs from an incompatible future schema.
  const int version = static_cast<int>(tree.getProperty("stateVersion", kStateVersion));
  if (version > kStateVersion)
    return std::nullopt;

  return tree;
}

// Parse + validate an untrusted binary blob in the host getStateInformation
// format (XML wrapped by copyXmlToBinary). Used for both host state restore and
// on-disk user presets, so the trust model is identical in both paths.
inline std::optional<juce::ValueTree>
parsePluginStateBinary(const void *data, int sizeInBytes,
                       const juce::Identifier &expectedType) {
  // 1 MiB sanity cap before we hand anything to the XML parser.
  if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > (1 << 20))
    return std::nullopt;

  auto xml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);
  if (xml == nullptr)
    return std::nullopt;

  return validatePluginState(juce::ValueTree::fromXml(*xml), expectedType);
}

} // namespace bbm
