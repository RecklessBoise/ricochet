#include "RenderEngine.h"

namespace ric
{

void RenderEngine::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);
    sr = sampleRate;
    synth.prepare (sr);
    fx.prepare (sr);
    reset();
}

void RenderEngine::reset() noexcept
{
    sim.reset();
    synth.reset();
    fx.reset();
    numPending = 0;
    numNoteOffs = 0;
    numBlockHits = 0;
    beat = 0.0;
    samplePos = 0;
}

void RenderEngine::setScene (const Scene& s) noexcept
{
    sim.setScene (s);
    world = s.world;
}

void RenderEngine::previewNote (int engine, int note) noexcept
{
    previewEngine = engine;
    previewNoteNumber = note;
}

void RenderEngine::process (float* left, float* right, int numSamples, double bpm, double hostBeat,
                            juce::MidiBuffer* midiOut, const juce::MidiBuffer* midiIn) noexcept
{
    bpm = juce::jlimit (20.0, 400.0, bpm);
    if (hostBeat >= 0.0)
        beat = hostBeat;

    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    SynthParams sp;
    sp.brightness = sound.brightness;
    sp.decay      = sound.decay;
    sp.attack     = sound.attack;
    sp.noteLength = sound.noteLength;
    synth.setParams (sp);
    sim.setSpeed (sound.speed);

    if (midiIn != nullptr)
        for (const auto meta : *midiIn)
            if (meta.getMessage().isNoteOn())
                sim.triggerMidiEmitters();

    // 1. Physics -> hits
    numBlockHits = 0;
    if (running)
        numBlockHits = sim.process (numSamples, sr, bpm, beat, blockHits.data(), kMaxHitsPerBlock);

    // 2. Hits -> scheduled notes (optionally quantised to the grid)
    const double q = quantizeBeats (world.quantize);
    const double samplesPerBeat = 60.0 / bpm * sr;
    for (int i = 0; i < numBlockHits && numPending < (int) pending.size(); ++i)
    {
        const auto& h = blockHits[(size_t) i];
        juce::int64 t = samplePos + h.sampleOffset;
        if (q > 0.0)
        {
            const double target = std::ceil (h.beat / q - 1.0e-6) * q;
            t += (juce::int64) std::llround ((target - h.beat) * samplesPerBeat);
        }
        auto& p = pending[(size_t) numPending++];
        p.time = t;
        p.note = h.note;
        p.engine = h.engine;
        p.velocity = h.velocity;
        p.pan = h.pan;
        p.gain = h.gain;
    }

    if (previewEngine >= 0 && numPending < (int) pending.size())
    {
        auto& p = pending[(size_t) numPending++];
        p = Pending();
        p.time = samplePos;
        p.note = previewNoteNumber;
        p.engine = previewEngine;
        p.velocity = 0.8f;
        previewEngine = -1;
    }

    // 3. Collect what is due in this block
    const juce::int64 blockEnd = samplePos + numSamples;
    int numDue = 0;
    for (int i = 0; i < numPending;)
    {
        if (pending[(size_t) i].time < blockEnd)
        {
            due[(size_t) numDue] = pending[(size_t) i];
            dueOffsets[(size_t) numDue] = (int) juce::jlimit<juce::int64> (0, numSamples - 1, pending[(size_t) i].time - samplePos);
            ++numDue;
            pending[(size_t) i] = pending[(size_t) --numPending];
        }
        else
        {
            ++i;
        }
    }

    // insertion sort by offset (small n)
    for (int i = 1; i < numDue; ++i)
    {
        auto e = due[(size_t) i];
        auto o = dueOffsets[(size_t) i];
        int j = i - 1;
        while (j >= 0 && dueOffsets[(size_t) j] > o)
        {
            due[(size_t) (j + 1)] = due[(size_t) j];
            dueOffsets[(size_t) (j + 1)] = dueOffsets[(size_t) j];
            --j;
        }
        due[(size_t) (j + 1)] = e;
        dueOffsets[(size_t) (j + 1)] = o;
    }

    // 4. Render synth between events, emit MIDI
    const auto noteLenSamples = (juce::int64) (juce::jmax (0.02f, sound.noteLength) * (float) sr);
    int cursor = 0;
    for (int i = 0; i < numDue; ++i)
    {
        const int off = dueOffsets[(size_t) i];
        if (off > cursor)
        {
            synth.render (left, right, cursor, off - cursor);
            cursor = off;
        }
        const auto& e = due[(size_t) i];
        synth.noteOn (e.engine, e.note, e.velocity, e.pan, e.gain);

        if (midiOut != nullptr)
        {
            const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (e.velocity * 127.0f));
            midiOut->addEvent (juce::MidiMessage::noteOn (1, e.note, vel), off);
            if (numNoteOffs < (int) noteOffs.size())
                noteOffs[(size_t) numNoteOffs++] = { samplePos + off + noteLenSamples, e.note };
        }
    }
    if (cursor < numSamples)
        synth.render (left, right, cursor, numSamples - cursor);

    if (midiOut != nullptr)
    {
        for (int i = 0; i < numNoteOffs;)
        {
            if (noteOffs[(size_t) i].time < blockEnd)
            {
                const int off = (int) juce::jlimit<juce::int64> (0, numSamples - 1, noteOffs[(size_t) i].time - samplePos);
                midiOut->addEvent (juce::MidiMessage::noteOff (1, noteOffs[(size_t) i].note), off);
                noteOffs[(size_t) i] = noteOffs[(size_t) --numNoteOffs];
            }
            else
            {
                ++i;
            }
        }
    }

    // 5. Master FX
    FxParams fp;
    fp.drive        = sound.drive;
    fp.delayMix     = sound.delayMix;
    fp.feedback     = sound.feedback;
    fp.delaySeconds = delayDivisionBeats (sound.delayDiv) * 60.0 / bpm;
    fp.reverbMix    = sound.reverbMix;
    fp.roomSize     = sound.roomSize;
    fp.master       = sound.master;
    fx.process (left, right, numSamples, fp);

    samplePos = blockEnd;
    if (hostBeat < 0.0)
        beat += (double) numSamples / samplesPerBeat;
}

} // namespace ric
