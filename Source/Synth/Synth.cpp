#include "Synth.h"

namespace ric
{

namespace
{
    constexpr double kTwoPiD = 6.283185307179586;

    float mtof (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

    /** Per-sample multiplier reaching -60 dB after `seconds`. */
    float t60Coef (float seconds, double sr) noexcept
    {
        seconds = juce::jmax (0.002f, seconds);
        return (float) std::pow (0.001, 1.0 / (seconds * sr));
    }

    float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    float saw (double ph, double dt) noexcept   { return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt); }

    float pulse (double ph, double dt, double duty) noexcept
    {
        float v = ph < duty ? 1.0f : -1.0f;
        v += polyBlep (ph, dt);
        double t2 = ph - duty;
        if (t2 < 0.0) t2 += 1.0;
        v -= polyBlep (t2, dt);
        return v;
    }

    void wrap (double& ph) noexcept { if (ph >= 1.0) ph -= std::floor (ph); }

    /** Topology-preserving state variable filter; returns low-pass, writes band-pass. */
    float svf (float in, float cutoff, float q, double sr, float& ic1, float& ic2, float& band) noexcept
    {
        cutoff = juce::jlimit (20.0f, (float) (sr * 0.45), cutoff);
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
        const float k = 1.0f / juce::jmax (0.5f, q);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = in - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        band = v1;
        return v2;
    }

    struct EngineShape { float attack, decay, sustain, release, level; bool sustains; };

    EngineShape shapeFor (int engine) noexcept
    {
        switch (engine)
        {
            case 0:  return { 0.001f, 2.2f, 0.0f, 0.3f, 0.55f, false }; // Glass FM
            case 1:  return { 0.001f, 3.0f, 0.0f, 0.3f, 0.70f, false }; // Pluck
            case 2:  return { 0.001f, 1.4f, 0.0f, 0.3f, 0.60f, false }; // Mallet
            case 3:  return { 0.003f, 0.7f, 0.35f, 0.25f, 0.32f, true }; // Analog
            case 4:  return { 0.090f, 1.8f, 0.75f, 1.2f, 0.30f, true }; // Halo pad
            case 5:  return { 0.001f, 0.5f, 0.0f, 0.1f, 0.85f, false }; // Drums
            case 6:  return { 0.030f, 1.0f, 0.55f, 0.5f, 0.60f, true }; // Wind
            case 7:  return { 0.001f, 0.6f, 0.55f, 0.06f, 0.20f, true }; // Chip
            default: return { 0.001f, 1.0f, 0.0f, 0.2f, 0.50f, false };
        }
    }
}

//==============================================================================
void Synth::prepare (double sampleRate)
{
    sr = sampleRate;
    const auto lineSize = (size_t) juce::nextPowerOfTwo ((int) (sr / 25.0) + 8);
    for (auto& v : voices)
    {
        v.line.assign (lineSize, 0.0f);
        v.active = false;
    }
}

void Synth::reset() noexcept
{
    for (auto& v : voices)
        v.active = false;
}

int Synth::numActiveVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

float Synth::nextNoise (Voice& v) noexcept
{
    v.rng ^= v.rng << 13;
    v.rng ^= v.rng >> 17;
    v.rng ^= v.rng << 5;
    return (float) (v.rng & 0xffffff) / (float) 0x800000 - 1.0f;
}

void Synth::noteOn (int engine, int note, float velocity, float pan, float gain) noexcept
{
    engine = juce::jlimit (0, kNumEngines - 1, engine);

    // Retrigger the same engine/note, otherwise take a free voice, otherwise steal the quietest.
    Voice* target = nullptr;
    for (auto& v : voices)
        if (v.active && v.engine == engine && v.note == note) { target = &v; break; }

    if (target == nullptr)
        for (auto& v : voices)
            if (! v.active) { target = &v; break; }

    if (target == nullptr)
    {
        float quietest = 1.0e9f;
        for (auto& v : voices)
        {
            const float score = v.level - (v.released ? 1.0f : 0.0f);
            if (score < quietest) { quietest = score; target = &v; }
        }
    }

    if (target != nullptr)
        startVoice (*target, engine, note, velocity, pan, gain);
}

void Synth::startVoice (Voice& v, int engine, int note, float velocity, float pan, float gain) noexcept
{
    const auto shape = shapeFor (engine);
    const float decayMul = std::pow (4.0f, params.decay * 2.0f - 1.0f);
    const float bright = params.brightness;

    v.active     = true;
    v.engine     = engine;
    v.note       = note;
    v.freq       = mtof ((float) note);
    v.vel        = velocity;
    v.age        = 0;
    v.released   = false;
    v.sustaining = shape.sustains;
    v.releaseAt  = (juce::int64) (juce::jmax (0.02f, params.noteLength) * (float) sr);
    v.rng        = 0x9E3779B9u * (++noteCounter) + (juce::uint32) note;

    const float p = juce::jlimit (-1.0f, 1.0f, pan);
    const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const float g = shape.level * gain;
    v.gainL = std::cos (ang) * g;
    v.gainR = std::sin (ang) * g;

    const float attackSec = shape.attack + params.attack * 0.6f;
    v.attackInc   = 1.0f / juce::jmax (1.0f, attackSec * (float) sr);
    v.attackPos   = 0.0f;
    v.level       = 1.0f;
    v.sustain     = shape.sustain;
    v.decayCoef   = t60Coef (shape.decay * decayMul, sr);
    v.releaseCoef = t60Coef (shape.release * juce::jmax (0.5f, decayMul), sr);

    v.phase.fill (0.0);
    v.ic1 = v.ic2 = 0.0f;
    v.lp = v.hp = 0.0f;
    v.modEnv = 1.0f;
    v.pitchEnv = 0.0f;
    v.noiseEnv = 0.0f;
    v.kind = 0;

    switch (engine)
    {
        case 0: // Glass FM
            v.modCoef = t60Coef (0.35f + 0.8f * decayMul * (1.0f - bright * 0.5f), sr);
            break;

        case 1: // Pluck (Karplus-Strong)
        {
            const float f = juce::jlimit (30.0f, (float) (sr * 0.25), v.freq);
            v.ksDelay = juce::jlimit (2.0f, (float) v.line.size() - 4.0f, (float) sr / f);
            v.ksFeedback = (float) std::pow (0.001, 1.0 / ((double) f * (double) (2.5f * decayMul)));
            v.ksLowpass = 0.25f + 0.7f * bright;
            v.ksState = 0.0f;
            float s = 0.0f;
            const float excite = 0.2f + 0.8f * bright;
            for (auto& x : v.line)
            {
                s += (nextNoise (v) - s) * excite;
                x = s * velocity;
            }
            v.writePos = 0;
            v.decayCoef = t60Coef (6.0f * decayMul, sr);
            break;
        }

        case 2: // Mallet: modal partials
        {
            static const float ratios[] { 1.0f, 3.99f, 10.65f, 2.01f };
            static const float amps[]   { 1.0f, 0.45f, 0.18f, 0.12f };
            static const float decays[] { 1.0f, 0.45f, 0.18f, 0.6f };
            for (size_t i = 0; i < 4; ++i)
            {
                const bool audible = v.freq * ratios[i] < (float) sr * 0.45f;
                v.partialEnv[i] = audible ? amps[i] * (i == 0 ? 1.0f : 0.3f + bright) : 0.0f;
                v.partialCoef[i] = t60Coef (shapeFor (2).decay * decayMul * decays[i], sr);
            }
            v.noiseEnv = 0.25f * bright;
            v.noiseCoef = t60Coef (0.01f, sr);
            break;
        }

        case 3: // Analog
            v.modCoef = t60Coef (0.25f + 0.4f * decayMul, sr);
            break;

        case 5: // Drums
        {
            const int pc = note % 12;
            v.kind = pc <= 2 ? 0 : pc <= 5 ? 1 : pc <= 8 ? 2 : pc <= 10 ? 3 : 4;
            switch (v.kind)
            {
                case 0: // kick
                    v.freq = 42.0f + (float) pc * 4.0f;
                    v.pitchEnv = 3.0f;  v.pitchCoef = t60Coef (0.06f, sr);
                    v.decayCoef = t60Coef (0.45f * decayMul, sr);
                    v.noiseEnv = 0.3f;  v.noiseCoef = t60Coef (0.004f, sr);
                    break;
                case 1: // snare
                    v.freq = 175.0f + (float) (pc - 3) * 15.0f;
                    v.pitchEnv = 0.6f;  v.pitchCoef = t60Coef (0.02f, sr);
                    v.decayCoef = t60Coef (0.12f * decayMul, sr);
                    v.noiseEnv = 1.0f;  v.noiseCoef = t60Coef (0.22f * decayMul, sr);
                    break;
                case 2: // tom, pitched by the octave
                    v.freq = juce::jlimit (70.0f, 320.0f, mtof ((float) note) * 0.5f);
                    v.pitchEnv = 0.8f;  v.pitchCoef = t60Coef (0.08f, sr);
                    v.decayCoef = t60Coef (0.5f * decayMul, sr);
                    v.noiseEnv = 0.15f; v.noiseCoef = t60Coef (0.01f, sr);
                    break;
                case 3: // closed hat
                    v.decayCoef = t60Coef (0.001f, sr);
                    v.noiseEnv = 0.8f;  v.noiseCoef = t60Coef (0.06f * decayMul, sr);
                    break;
                default: // open hat
                    v.decayCoef = t60Coef (0.001f, sr);
                    v.noiseEnv = 0.7f;  v.noiseCoef = t60Coef (0.35f * decayMul, sr);
                    break;
            }
            break;
        }

        case 7: // Chip: little pitch blip
            v.pitchEnv = 1.0f;
            v.pitchCoef = t60Coef (0.03f, sr);
            break;

        default:
            break;
    }
}

float Synth::renderVoice (Voice& v) noexcept
{
    const double sr_ = sr;
    const float bright = params.brightness;
    float out = 0.0f;

    // Amplitude envelope
    float amp;
    if (v.attackPos < 1.0f)
    {
        v.attackPos = juce::jmin (1.0f, v.attackPos + v.attackInc);
        amp = v.attackPos * v.level;
    }
    else
    {
        if (v.released)
            v.level *= v.releaseCoef;
        else
            v.level = v.sustain + (v.level - v.sustain) * v.decayCoef;
        amp = v.level;
    }

    if (v.sustaining && ! v.released && v.age >= v.releaseAt)
        v.released = true;

    switch (v.engine)
    {
        case 0: // Glass FM: bell-like 3.5 ratio + octave shimmer
        {
            const double dt = (double) v.freq / sr_;
            v.modEnv *= v.modCoef;
            const float index = (0.6f + 5.0f * bright) * v.vel * v.modEnv;
            const float mod = std::sin ((float) (kTwoPiD * v.phase[1]));
            const float car = std::sin ((float) (kTwoPiD * v.phase[0]) + index * mod);
            const float shimmer = std::sin ((float) (kTwoPiD * v.phase[2])) * 0.18f * v.modEnv;
            out = car + shimmer;
            v.phase[0] += dt; v.phase[1] += dt * 3.5; v.phase[2] += dt * 2.0;
            wrap (v.phase[0]); wrap (v.phase[1]); wrap (v.phase[2]);
            break;
        }

        case 1: // Pluck
        {
            const int size = (int) v.line.size();
            const int mask = size - 1;
            float readPos = (float) v.writePos - v.ksDelay;
            while (readPos < 0.0f) readPos += (float) size;
            const int i0 = (int) readPos;
            const float frac = readPos - (float) i0;
            const float a = v.line[(size_t) (i0 & mask)];
            const float b = v.line[(size_t) ((i0 + 1) & mask)];
            const float y = a + (b - a) * frac;
            v.ksState += (y - v.ksState) * v.ksLowpass;
            v.line[(size_t) v.writePos] = v.ksState * v.ksFeedback;
            v.writePos = (v.writePos + 1) & mask;
            out = y * 1.6f;
            break;
        }

        case 2: // Mallet
        {
            static const double ratios[] { 1.0, 3.99, 10.65, 2.01 };
            const double dt = (double) v.freq / sr_;
            float s = 0.0f;
            for (size_t i = 0; i < 4; ++i)
            {
                if (v.partialEnv[i] <= 1.0e-5f) continue;
                s += std::sin ((float) (kTwoPiD * v.phase[i])) * v.partialEnv[i];
                v.partialEnv[i] *= v.partialCoef[i];
                v.phase[i] += dt * ratios[i];
                wrap (v.phase[i]);
            }
            v.noiseEnv *= v.noiseCoef;
            s += nextNoise (v) * v.noiseEnv;
            out = s;
            amp = juce::jmin (1.0f, v.attackPos); // partials carry their own decay
            v.level = v.partialEnv[0] + v.partialEnv[1] + v.noiseEnv;
            break;
        }

        case 3: // Analog: two detuned saws through a resonant low-pass with filter envelope
        {
            const double dt = (double) v.freq / sr_;
            const float s = saw (v.phase[0], dt) + saw (v.phase[1], dt * 1.006) * 0.7f;
            v.phase[0] += dt; v.phase[1] += dt * 1.006;
            wrap (v.phase[0]); wrap (v.phase[1]);
            v.modEnv *= v.modCoef;
            const float cutoff = v.freq * (1.2f + (1.0f + 14.0f * bright) * v.modEnv * v.vel);
            float band = 0.0f;
            out = svf (s * 0.5f, cutoff, 1.4f + bright * 2.0f, sr_, v.ic1, v.ic2, band);
            break;
        }

        case 4: // Halo pad: detuned sine stack with slow shimmer
        {
            static const double ratios[] { 1.0, 1.0031, 0.9969, 2.0012, 0.5, 3.0 };
            static const float amps[]    { 0.5f, 0.35f, 0.35f, 0.22f, 0.25f, 0.08f };
            const double dt = (double) v.freq / sr_;
            float s = 0.0f;
            for (size_t i = 0; i < 6; ++i)
            {
                const float a = i >= 3 ? amps[i] * (0.4f + bright) : amps[i];
                s += std::sin ((float) (kTwoPiD * v.phase[i])) * a;
                v.phase[i] += dt * ratios[i];
                wrap (v.phase[i]);
            }
            const float trem = 1.0f + 0.12f * std::sin ((float) ((double) v.age / sr_ * 3.1));
            out = s * trem;
            break;
        }

        case 5: // Drums
        {
            v.noiseEnv *= v.noiseCoef;
            const float n = nextNoise (v);
            if (v.kind <= 2)
            {
                v.pitchEnv *= v.pitchCoef;
                const double dt = (double) (v.freq * (1.0f + v.pitchEnv)) / sr_;
                const float tone = std::sin ((float) (kTwoPiD * v.phase[0]));
                v.phase[0] += dt; wrap (v.phase[0]);
                float band = 0.0f;
                svf (n, v.kind == 1 ? 3500.0f + bright * 4000.0f : 1200.0f, 0.8f, sr_, v.ic1, v.ic2, band);
                const float noiseAmt = v.kind == 1 ? 0.8f : 0.6f;
                out = tone * amp * (v.kind == 1 ? 0.6f : 1.0f) + band * v.noiseEnv * noiseAmt;
                amp = 1.0f;
                v.level = juce::jmax (v.level, v.noiseEnv);
            }
            else
            {
                // hats: high-passed noise (difference of noise and its low-pass)
                v.lp += (n - v.lp) * (0.25f + 0.2f * (1.0f - bright));
                const float hpN = n - v.lp;
                out = hpN * v.noiseEnv * 0.9f;
                amp = 1.0f;
                v.level = v.noiseEnv;
            }
            break;
        }

        case 6: // Wind: resonant band-pass noise with a ghost of the fundamental
        {
            const double dt = (double) v.freq / sr_;
            float band = 0.0f;
            svf (nextNoise (v), v.freq, 8.0f + 40.0f * bright, sr_, v.ic1, v.ic2, band);
            const float tone = std::sin ((float) (kTwoPiD * v.phase[0])) * 0.25f;
            v.phase[0] += dt; wrap (v.phase[0]);
            out = band * 2.2f + tone;
            break;
        }

        case 7: // Chip: band-limited pulse with vibrato
        {
            v.pitchEnv *= v.pitchCoef;
            const float t = (float) ((double) v.age / sr_);
            const float vib = t > 0.12f ? 1.0f + 0.006f * std::sin (t * 34.0f) : 1.0f;
            const double dt = (double) (v.freq * vib * (1.0f + v.pitchEnv * 0.5f)) / sr_;
            const double duty = 0.5 - 0.375 * (double) bright;
            out = pulse (v.phase[0], dt, duty) * 0.7f;
            v.phase[0] += dt; wrap (v.phase[0]);
            break;
        }

        default:
            break;
    }

    ++v.age;

    const bool finished = (v.released || ! v.sustaining || v.sustain <= 0.0f)
                          && v.attackPos >= 1.0f && v.level < 1.0e-4f;
    if (finished || v.age > (juce::int64) (sr_ * 30.0))
        v.active = false;

    return out * amp * v.vel;
}

void Synth::render (float* left, float* right, int start, int num) noexcept
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int i = start; i < start + num; ++i)
        {
            const float s = renderVoice (v);
            left[i]  += s * v.gainL;
            right[i] += s * v.gainR;
            if (! v.active) break;
        }
    }
}

} // namespace ric
