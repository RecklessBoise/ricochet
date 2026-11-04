#pragma once

#include "Physics.h"
#include "../Synth/Synth.h"
#include "../Synth/Effects.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ric
{

/** Everything that turns a scene into sound: simulation, note scheduling, synth and FX.
    Used both live (by the processor) and offline (by the bounce renderer). */
class RenderEngine
{
public:
    static constexpr int kMaxHitsPerBlock = 256;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setScene (const Scene& s) noexcept;
    void setSound (const SoundSettings& s) noexcept { sound = s; }
    void setRunning (bool shouldRun) noexcept { running = shouldRun; }
    bool isRunning() const noexcept { return running; }

    /** Renders numSamples into left/right (overwrites). hostBeat < 0 means use the internal clock. */
    void process (float* left, float* right, int numSamples, double bpm, double hostBeat,
                  juce::MidiBuffer* midiOut, const juce::MidiBuffer* midiIn) noexcept;

    /** Play a note immediately (used for previewing an obstacle's sound). */
    void previewNote (int engine, int note) noexcept;

    const Simulation& getSimulation() const noexcept { return sim; }
    Simulation& getSimulation() noexcept { return sim; }
    const HitEvent* getBlockHits() const noexcept { return blockHits.data(); }
    int getNumBlockHits() const noexcept { return numBlockHits; }
    double getBeat() const noexcept { return beat; }
    juce::int64 getSamplePosition() const noexcept { return samplePos; }
    int getNumActiveVoices() const noexcept { return synth.numActiveVoices(); }

private:
    struct Pending
    {
        juce::int64 time = 0;
        int note = 60, engine = 0;
        float velocity = 1.0f, pan = 0.0f, gain = 1.0f;
    };

    struct NoteOff { juce::int64 time = 0; int note = 60; };

    Simulation sim;
    Synth synth;
    Effects fx;
    SoundSettings sound;
    World world;

    std::array<HitEvent, kMaxHitsPerBlock> blockHits {};
    int numBlockHits = 0;

    std::array<Pending, 512> pending {};
    int numPending = 0;
    std::array<NoteOff, 512> noteOffs {};
    int numNoteOffs = 0;
    std::array<Pending, 512> due {};
    std::array<int, 512> dueOffsets {};

    double sr = 48000.0;
    double beat = 0.0;
    juce::int64 samplePos = 0;
    bool running = true;
    int previewEngine = -1, previewNoteNumber = 60;
};

} // namespace ric
