#pragma once

#include "Scene.h"
#include <vector>

namespace ric
{

/** All factory presets, generated deterministically (same list on every machine). */
const std::vector<NamedScene>& factoryPresets();

/** Distinct factory categories in display order. */
juce::StringArray factoryCategories();

/** The scene a fresh plug-in instance starts with. */
NamedScene defaultScene();

/** Folder for user presets (~/Music/Ricochet/Presets). */
juce::File userPresetFolder();
std::vector<NamedScene> loadUserPresets();
bool saveUserPreset (const NamedScene&, juce::File* writtenFile = nullptr);

} // namespace ric
