#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/RenderEngine.h"
#include "Core/Presets.h"
#include "Core/Randomizer.h"
#include "Core/OfflineRenderer.h"
#include "Recorder.h"
#include <vector>

namespace ParamIDs
{
    inline constexpr const char* master     = "master";
    inline constexpr const char* brightness = "brightness";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* noteLength = "noteLength";
    inline constexpr const char* reverbMix  = "reverbMix";
    inline constexpr const char* roomSize   = "roomSize";
    inline constexpr const char* delayMix   = "delayMix";
    inline constexpr const char* delayDiv   = "delayDiv";
    inline constexpr const char* feedback   = "feedback";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* speed      = "speed";
    inline constexpr const char* running    = "running";
    inline constexpr const char* syncHost   = "syncHost";
}

/** What the UI needs to draw the moving parts, copied from the audio thread each block. */
struct LiveSnapshot
{
    struct BallView { int id; float x, y, radius; int lastNote; };
    std::array<BallView, ric::kMaxBalls> balls {};
    int numBalls = 0;
    std::array<float, ric::kMaxObstacles> rotations {};
    double beat = 0.0;
    double bpm = 120.0;
    int voices = 0;
};

struct UiHit { int obstacle; int note; float velocity; float x, y; };

class RicochetProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster
{
public:
    RicochetProcessor();
    ~RicochetProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reckless Ricochet"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Scene editing (message thread)
    const ric::Scene& getScene() const noexcept { return scene; }
    void setScene (const ric::Scene& s, bool recordUndo);
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    void loadPreset (const ric::NamedScene& preset);
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetCategory() const { return presetCategory; }
    void setPresetName (const juce::String& n) { presetName = n; sendChangeMessage(); }
    bool saveCurrentAsUserPreset (const juce::String& name);
    const std::vector<ric::NamedScene>& getUserPresets() const noexcept { return userPresets; }
    void refreshUserPresets();

    ric::RandomOptions randomOptions;
    void randomize();
    void mutate (float amount = 0.35f);
    juce::uint32 getLastSeed() const noexcept { return lastSeed; }

    void resetSimulation()                 { resetRequested = true; }
    void previewNote (int engineIndex, int note) { previewRequest = (engineIndex << 8) | (note & 0xff); }
    void fireEmitter (int index)            { fireRequest = index; }

    ric::SoundSettings currentSound() const;

    //==========================================================================
    // Live data for the UI
    void copySnapshot (LiveSnapshot& out);
    int popHits (UiHit* dest, int max);

    // Recording & bounce
    Recorder& getRecorder() noexcept { return recorder; }
    bool startBounce (double bars);
    bool isBouncing() const noexcept { return bouncing.load(); }
    float getBounceProgress() const noexcept { return bounceProgress.load(); }
    juce::File takeFinishedBounce();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applySoundToParameters (const ric::SoundSettings&);
    void renderChunk (float* l, float* r, int n, double bpm, double hostBeat,
                      juce::MidiBuffer& midiOut, const juce::MidiBuffer* midiIn, int midiOffset);

    // message-thread state
    ric::Scene scene;
    juce::String presetName { "Init" }, presetCategory { "Factory" };
    std::vector<ric::Scene> undoStack, redoStack;
    std::vector<ric::NamedScene> userPresets;
    int currentProgram = 0;
    juce::uint32 lastSeed = 1;

    // message -> audio
    juce::SpinLock sceneLock;
    ric::Scene pendingScene;
    std::atomic<bool> sceneDirty { true };
    std::atomic<bool> resetRequested { true };
    std::atomic<int> previewRequest { -1 };
    std::atomic<int> fireRequest { -1 };

    // audio thread
    ric::RenderEngine engine;
    juce::AudioBuffer<float> temp;
    juce::MidiBuffer chunkMidi, midiOutBuffer;
    bool wasHostPlaying = false;

    // audio -> message
    juce::SpinLock snapshotLock;
    LiveSnapshot snapshot;
    juce::AbstractFifo hitFifo { 1024 };
    std::array<UiHit, 1024> hitBuffer {};

    Recorder recorder;

    std::atomic<bool> bouncing { false };
    std::atomic<float> bounceProgress { 0.0f };
    juce::CriticalSection bounceLock;
    juce::File finishedBounce;
    std::unique_ptr<juce::Thread> bounceThread;

    std::atomic<float>* pMaster = nullptr; std::atomic<float>* pBrightness = nullptr;
    std::atomic<float>* pDecay = nullptr;  std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pNoteLen = nullptr; std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pRoom = nullptr;   std::atomic<float>* pDelayMix = nullptr;
    std::atomic<float>* pDelayDiv = nullptr; std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pDrive = nullptr;  std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pRunning = nullptr; std::atomic<float>* pSync = nullptr;

    double lastBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RicochetProcessor)
};
