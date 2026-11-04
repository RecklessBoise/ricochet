#include "Effects.h"

namespace ric
{

void Effects::prepare (double sampleRate)
{
    sr = sampleRate;
    const auto size = (size_t) (sr * 4.0) + 8;
    delayL.assign (size, 0.0f);
    delayR.assign (size, 0.0f);
    reverb.setSampleRate (sr);
    reset();
}

void Effects::reset() noexcept
{
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    delayWrite = 0;
    smoothedDelay = 0.0f;
    fbLowL = fbLowR = 0.0f;
    limiterGain = 1.0f;
    dcL = dcR = dcInL = dcInR = 0.0f;
    reverb.reset();
}

void Effects::process (float* left, float* right, int numSamples, const FxParams& p) noexcept
{
    if (delayL.empty()) return;

    const int size = (int) delayL.size();
    const float targetDelay = (float) juce::jlimit (0.01, 3.9, p.delaySeconds) * (float) sr;
    if (smoothedDelay <= 0.0f) smoothedDelay = targetDelay;

    const float driveAmt = 1.0f + p.drive * 8.0f;
    const float driveNorm = 1.0f / std::tanh (driveAmt);
    const float fb = juce::jlimit (0.0f, 0.95f, p.feedback);
    const float limCeiling = 0.92f;
    const float release = (float) std::exp (-1.0 / (0.15 * sr));

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i], r = right[i];

        // drive (blend so that drive = 0 is clean)
        if (p.drive > 0.001f)
        {
            const float dl = std::tanh (l * driveAmt) * driveNorm;
            const float dr = std::tanh (r * driveAmt) * driveNorm;
            l += (dl - l) * juce::jmin (1.0f, p.drive * 2.0f);
            r += (dr - r) * juce::jmin (1.0f, p.drive * 2.0f);
        }

        // ping-pong delay
        smoothedDelay += (targetDelay - smoothedDelay) * 0.0005f;
        float readPos = (float) delayWrite - smoothedDelay;
        while (readPos < 0.0f) readPos += (float) size;
        const int i0 = (int) readPos;
        const float frac = readPos - (float) i0;
        const int i1 = (i0 + 1) % size;
        const float dlOut = delayL[(size_t) i0] + (delayL[(size_t) i1] - delayL[(size_t) i0]) * frac;
        const float drOut = delayR[(size_t) i0] + (delayR[(size_t) i1] - delayR[(size_t) i0]) * frac;

        fbLowL += (drOut - fbLowL) * 0.35f;
        fbLowR += (dlOut - fbLowR) * 0.35f;
        delayL[(size_t) delayWrite] = (l + r) * 0.5f + fbLowL * fb;
        delayR[(size_t) delayWrite] = fbLowR * fb;
        delayWrite = (delayWrite + 1) % size;

        left[i]  = l + dlOut * p.delayMix;
        right[i] = r + drOut * p.delayMix;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize   = juce::jlimit (0.0f, 1.0f, p.roomSize);
    rp.damping    = 0.45f;
    rp.wetLevel   = juce::jlimit (0.0f, 1.0f, p.reverbMix) * 0.55f;
    rp.dryLevel   = 1.0f - juce::jlimit (0.0f, 1.0f, p.reverbMix) * 0.35f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    if (rp.roomSize != lastReverb.roomSize || rp.wetLevel != lastReverb.wetLevel || rp.dryLevel != lastReverb.dryLevel)
    {
        reverb.setParameters (rp);
        lastReverb = rp;
    }
    if (p.reverbMix > 0.0005f)
        reverb.processStereo (left, right, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        // DC blocker
        float l = left[i], r = right[i];
        dcL = l - dcInL + 0.9995f * dcL; dcInL = l; l = dcL;
        dcR = r - dcInR + 0.9995f * dcR; dcInR = r; r = dcR;

        l *= p.master;
        r *= p.master;

        const float peak = juce::jmax (std::abs (l), std::abs (r));
        const float needed = peak > limCeiling ? limCeiling / peak : 1.0f;
        limiterGain = needed < limiterGain ? needed : needed + (limiterGain - needed) * release;
        l *= limiterGain;
        r *= limiterGain;

        left[i]  = juce::jlimit (-1.0f, 1.0f, std::isfinite (l) ? l : 0.0f);
        right[i] = juce::jlimit (-1.0f, 1.0f, std::isfinite (r) ? r : 0.0f);
    }
}

} // namespace ric
