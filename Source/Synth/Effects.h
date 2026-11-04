#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace ric
{

struct FxParams
{
    float  drive        = 0.1f;
    float  delayMix     = 0.12f;
    float  feedback     = 0.35f;
    double delaySeconds = 0.25;
    float  reverbMix    = 0.25f;
    float  roomSize     = 0.65f;
    float  master       = 0.75f;
};

/** Master bus: drive -> ping-pong delay -> reverb -> gain -> limiter. */
class Effects
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (float* left, float* right, int numSamples, const FxParams& p) noexcept;

private:
    double sr = 48000.0;
    std::vector<float> delayL, delayR;
    int delayWrite = 0;
    float smoothedDelay = 0.0f;
    float fbLowL = 0.0f, fbLowR = 0.0f;
    float limiterGain = 1.0f;
    float dcL = 0.0f, dcR = 0.0f, dcInL = 0.0f, dcInR = 0.0f;
    juce::Reverb reverb;
    juce::Reverb::Parameters lastReverb;
};

} // namespace ric
