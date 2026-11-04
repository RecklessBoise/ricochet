#pragma once

#include "../Core/Scene.h"
#include <vector>

namespace ric
{

struct SynthParams
{
    float brightness = 0.6f;  // 0..1
    float decay      = 0.5f;  // 0..1 (0.5 = engine default length)
    float attack     = 0.0f;  // 0..1
    float noteLength = 0.25f; // seconds, sustaining engines only
};

/** Polyphonic multi-engine synthesiser. All memory is allocated in prepare(). */
class Synth
{
public:
    static constexpr int kNumVoices = 32;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const SynthParams& p) noexcept { params = p; }

    void noteOn (int engine, int note, float velocity, float pan, float gain) noexcept;

    /** Adds the voices' output to L/R between [start, start + num). */
    void render (float* left, float* right, int start, int num) noexcept;

    int numActiveVoices() const noexcept;

private:
    struct Voice
    {
        bool  active = false;
        int   engine = 0;
        int   note   = 60;
        int   kind   = 0;       // drum sub-type
        float freq   = 440.0f;
        float vel    = 1.0f;
        float gainL  = 0.7f, gainR = 0.7f;
        juce::int64 age = 0;
        juce::int64 releaseAt = 0;
        bool  sustaining = false;
        bool  released = false;

        float level = 0.0f;
        float attackInc = 1.0f;
        float attackPos = 0.0f;
        float sustain = 0.0f;
        float decayCoef = 0.999f;
        float releaseCoef = 0.999f;

        std::array<double, 6> phase {};
        std::array<float, 6>  partialEnv {};
        std::array<float, 6>  partialCoef {};
        float modEnv = 1.0f, modCoef = 0.999f;
        float pitchEnv = 0.0f, pitchCoef = 0.99f;
        float noiseEnv = 0.0f, noiseCoef = 0.99f;

        // state-variable filter
        float ic1 = 0.0f, ic2 = 0.0f;
        float lp = 0.0f, hp = 0.0f;

        // Karplus-Strong
        std::vector<float> line;
        int   writePos = 0;
        float ksDelay = 100.0f;
        float ksFeedback = 0.99f;
        float ksLowpass = 0.5f;
        float ksState = 0.0f;

        juce::uint32 rng = 22222u;
    };

    float nextNoise (Voice& v) noexcept;
    float renderVoice (Voice& v) noexcept;
    void  startVoice (Voice& v, int engine, int note, float velocity, float pan, float gain) noexcept;

    std::array<Voice, kNumVoices> voices;
    SynthParams params;
    double sr = 48000.0;
    juce::uint32 noteCounter = 1;
};

} // namespace ric
