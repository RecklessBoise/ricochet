#pragma once

#include "Scene.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace ric
{

struct BounceSettings
{
    double sampleRate  = 48000.0;
    double bpm         = 120.0;
    double bars        = 8.0;   // 4/4 bars
    double tailSeconds = 3.0;
};

/** Opens a 24-bit stereo WAV writer, replacing any existing file. */
std::unique_ptr<juce::AudioFormatWriter> createWavWriter (const juce::File& file, double sampleRate,
                                                          int numChannels = 2, int bitsPerSample = 24);

/** Renders a scene deterministically from its initial state. */
juce::AudioBuffer<float> renderScene (const Scene& scene, const SoundSettings& sound,
                                      const BounceSettings& settings,
                                      std::atomic<float>* progress = nullptr,
                                      int* numHits = nullptr);

/** Renders a scene and writes it as a WAV file. */
bool bounceSceneToWav (const Scene& scene, const SoundSettings& sound, const BounceSettings& settings,
                       const juce::File& file, std::atomic<float>* progress = nullptr,
                       juce::String* error = nullptr);

/** Default location for recordings and bounces (~/Music/Ricochet/Recordings). */
juce::File recordingsFolder();
/** A new timestamped file inside recordingsFolder(). */
juce::File newRecordingFile (const juce::String& prefix);

} // namespace ric
