#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    class FunctionThread : public juce::Thread
    {
    public:
        FunctionThread (std::function<void()> f) : juce::Thread ("Ricochet bounce"), fn (std::move (f)) {}
        void run() override { fn(); }
    private:
        std::function<void()> fn;
    };

    juce::ParameterID pid (const char* id) { return { id, 1 }; }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RicochetProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });

    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::master), "Master", NormalisableRange<float> (0.0f, 1.0f), 0.75f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::brightness), "Brightness", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::decay), "Decay", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::attack), "Attack", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::noteLength), "Note length",
                                                       NormalisableRange<float> (0.02f, 3.0f, 0.0f, 0.4f), 0.25f,
                                                       AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::reverbMix), "Reverb", NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::roomSize), "Room", NormalisableRange<float> (0.0f, 1.0f), 0.65f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::delayMix), "Delay", NormalisableRange<float> (0.0f, 1.0f), 0.12f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::delayDiv), "Delay time", ric::delayDivisionNames(), 2));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::feedback), "Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::drive), "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::speed), "Speed",
                                                       NormalisableRange<float> (0.1f, 3.0f, 0.0f, 0.5f), 1.0f,
                                                       AudioParameterFloatAttributes().withLabel ("x")));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamIDs::running), "Play", true));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamIDs::syncHost), "Sync to host", false));
    return layout;
}

RicochetProcessor::RicochetProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RicochetParams", createLayout())
{
    pMaster     = apvts.getRawParameterValue (ParamIDs::master);
    pBrightness = apvts.getRawParameterValue (ParamIDs::brightness);
    pDecay      = apvts.getRawParameterValue (ParamIDs::decay);
    pAttack     = apvts.getRawParameterValue (ParamIDs::attack);
    pNoteLen    = apvts.getRawParameterValue (ParamIDs::noteLength);
    pReverb     = apvts.getRawParameterValue (ParamIDs::reverbMix);
    pRoom       = apvts.getRawParameterValue (ParamIDs::roomSize);
    pDelayMix   = apvts.getRawParameterValue (ParamIDs::delayMix);
    pDelayDiv   = apvts.getRawParameterValue (ParamIDs::delayDiv);
    pFeedback   = apvts.getRawParameterValue (ParamIDs::feedback);
    pDrive      = apvts.getRawParameterValue (ParamIDs::drive);
    pSpeed      = apvts.getRawParameterValue (ParamIDs::speed);
    pRunning    = apvts.getRawParameterValue (ParamIDs::running);
    pSync       = apvts.getRawParameterValue (ParamIDs::syncHost);

    refreshUserPresets();
    const auto initial = ric::defaultScene();
    scene = initial.scene;
    pendingScene = scene;
    presetName = initial.name;
    presetCategory = initial.category;
    applySoundToParameters (initial.scene.sound);
}

RicochetProcessor::~RicochetProcessor()
{
    if (bounceThread != nullptr)
        bounceThread->stopThread (30000);
}

//==============================================================================
bool RicochetProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RicochetProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    temp.setSize (2, juce::jmax (512, samplesPerBlock));
    chunkMidi.ensureSize (4096);
    midiOutBuffer.ensureSize (8192);
    recorder.prepare (sampleRate);
    sceneDirty = true;
    resetRequested = true;
}

ric::SoundSettings RicochetProcessor::currentSound() const
{
    ric::SoundSettings s;
    s.master     = pMaster->load();
    s.brightness = pBrightness->load();
    s.decay      = pDecay->load();
    s.attack     = pAttack->load();
    s.noteLength = pNoteLen->load();
    s.reverbMix  = pReverb->load();
    s.roomSize   = pRoom->load();
    s.delayMix   = pDelayMix->load();
    s.delayDiv   = juce::roundToInt (pDelayDiv->load());
    s.feedback   = pFeedback->load();
    s.drive      = pDrive->load();
    s.speed      = pSpeed->load();
    return s;
}

void RicochetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;

    // Transport
    double bpm = lastBpm;
    double hostBeat = -1.0;
    bool hostPlaying = false;
    const bool sync = pSync->load() > 0.5f;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
                if (sync && hostPlaying) hostBeat = *ppq;
        }
    lastBpm = bpm;

    // Pick up UI changes
    if (sceneDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (sceneLock);
        if (sl.isLocked())
        {
            engine.setScene (pendingScene);
            sceneDirty = false;
        }
    }

    if (sync && hostPlaying && ! wasHostPlaying)
        resetRequested = true;     // every host playback starts from the same state
    wasHostPlaying = hostPlaying;

    if (resetRequested.exchange (false))
        engine.reset();

    const int pr = previewRequest.exchange (-1);
    if (pr >= 0) engine.previewNote (pr >> 8, pr & 0xff);
    const int fr = fireRequest.exchange (-1);
    if (fr >= 0) engine.getSimulation().fireEmitter (fr);

    bool run = pRunning->load() > 0.5f;
    if (sync && ! hostPlaying) run = false;
    engine.setRunning (run);
    engine.setSound (currentSound());

    // Render in chunks that fit the scratch buffer
    auto& midiOut = midiOutBuffer;
    midiOut.clear();
    float* tl = temp.getWritePointer (0);
    float* tr = temp.getWritePointer (1);
    const int numOut = buffer.getNumChannels();

    for (int pos = 0; pos < numSamples;)
    {
        const int n = juce::jmin (numSamples - pos, temp.getNumSamples());
        chunkMidi.clear();
        const double chunkBeat = hostBeat >= 0.0 ? hostBeat + (double) pos * bpm / 60.0 / sr : -1.0;
        engine.process (tl, tr, n, bpm, chunkBeat, &chunkMidi, pos == 0 ? &midi : nullptr);
        midiOut.addEvents (chunkMidi, 0, n, pos);

        if (numOut >= 2)
        {
            buffer.copyFrom (0, pos, tl, n);
            buffer.copyFrom (1, pos, tr, n);
        }
        else if (numOut == 1)
        {
            buffer.copyFrom (0, pos, tl, n, 0.5f);
            buffer.addFrom (0, pos, tr, n, 0.5f);
        }
        recorder.process (tl, tr, n);

        // hits -> UI
        const auto* hits = engine.getBlockHits();
        const int numHits = engine.getNumBlockHits();
        if (numHits > 0)
        {
            const auto scope = hitFifo.write (numHits);
            auto fill = [&] (int start, int count, int srcOffset)
            {
                for (int i = 0; i < count; ++i)
                {
                    const auto& h = hits[srcOffset + i];
                    hitBuffer[(size_t) (start + i)] = { h.obstacle, h.note, h.velocity, h.x, h.y };
                }
            };
            fill (scope.startIndex1, scope.blockSize1, 0);
            fill (scope.startIndex2, scope.blockSize2, scope.blockSize1);
        }
        pos += n;
    }
    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    midi.swapWith (midiOut);

    // Snapshot for drawing
    const juce::SpinLock::ScopedTryLockType sl (snapshotLock);
    if (sl.isLocked())
    {
        const auto& sim = engine.getSimulation();
        int nb = 0;
        for (auto& b : sim.getBalls())
            if (b.alive)
                snapshot.balls[(size_t) nb++] = { b.id, b.x, b.y, b.radius, b.lastNote };
        snapshot.numBalls = nb;
        snapshot.rotations = sim.getRotations();
        snapshot.beat = engine.getBeat();
        snapshot.bpm = bpm;
        snapshot.voices = engine.getNumActiveVoices();
    }
}

//==============================================================================
void RicochetProcessor::copySnapshot (LiveSnapshot& out)
{
    const juce::SpinLock::ScopedLockType sl (snapshotLock);
    out = snapshot;
}

int RicochetProcessor::popHits (UiHit* dest, int max)
{
    const auto scope = hitFifo.read (juce::jmin (max, hitFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = hitBuffer[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = hitBuffer[(size_t) (scope.startIndex2 + i)];
    return n;
}

//==============================================================================
void RicochetProcessor::setScene (const ric::Scene& s, bool recordUndo)
{
    if (recordUndo) pushUndo();
    scene = s;
    {
        const juce::SpinLock::ScopedLockType sl (sceneLock);
        pendingScene = s;
    }
    sceneDirty = true;
    sendChangeMessage();
}

void RicochetProcessor::pushUndo()
{
    undoStack.push_back (scene);
    if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool RicochetProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back (scene);
    auto s = undoStack.back();
    undoStack.pop_back();
    setScene (s, false);
    return true;
}

bool RicochetProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back (scene);
    auto s = redoStack.back();
    redoStack.pop_back();
    setScene (s, false);
    return true;
}

void RicochetProcessor::applySoundToParameters (const ric::SoundSettings& s)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (ParamIDs::master, s.master);
    set (ParamIDs::brightness, s.brightness);
    set (ParamIDs::decay, s.decay);
    set (ParamIDs::attack, s.attack);
    set (ParamIDs::noteLength, s.noteLength);
    set (ParamIDs::reverbMix, s.reverbMix);
    set (ParamIDs::roomSize, s.roomSize);
    set (ParamIDs::delayMix, s.delayMix);
    set (ParamIDs::delayDiv, (float) s.delayDiv);
    set (ParamIDs::feedback, s.feedback);
    set (ParamIDs::drive, s.drive);
    set (ParamIDs::speed, s.speed);
}

void RicochetProcessor::loadPreset (const ric::NamedScene& preset)
{
    presetName = preset.name;
    presetCategory = preset.category;
    applySoundToParameters (preset.scene.sound);
    setScene (preset.scene, true);
    resetSimulation();
}

void RicochetProcessor::refreshUserPresets()
{
    userPresets = ric::loadUserPresets();
}

bool RicochetProcessor::saveCurrentAsUserPreset (const juce::String& name)
{
    ric::NamedScene ns { name, "User", scene };
    ns.scene.sound = currentSound();
    if (! ric::saveUserPreset (ns)) return false;
    refreshUserPresets();
    presetName = name;
    presetCategory = "User";
    sendChangeMessage();
    return true;
}

void RicochetProcessor::randomize()
{
    lastSeed = (juce::uint32) juce::Random::getSystemRandom().nextInt (0x7fffffff) | 1u;
    auto base = scene;
    base.sound = currentSound();
    const auto s = ric::randomScene (lastSeed, randomOptions, base);
    ric::NamedScene ns { "Random " + juce::String::toHexString ((int) lastSeed).toUpperCase(), "Random", s };
    loadPreset (ns);
}

void RicochetProcessor::mutate (float amount)
{
    lastSeed = (juce::uint32) juce::Random::getSystemRandom().nextInt (0x7fffffff) | 1u;
    setScene (ric::mutateScene (scene, lastSeed, amount), true);
}

//==============================================================================
bool RicochetProcessor::startBounce (double bars)
{
    if (bouncing.load()) return false;
    if (bounceThread != nullptr) bounceThread->stopThread (1000);

    ric::BounceSettings bs;
    bs.bars = bars;
    bs.bpm = lastBpm;
    bs.sampleRate = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    const auto s = scene;
    const auto snd = currentSound();
    bouncing = true;
    bounceProgress = 0.0f;

    bounceThread = std::make_unique<FunctionThread> ([this, s, snd, bs]
    {
        const auto file = ric::newRecordingFile ("Ricochet_Bounce");
        const bool ok = ric::bounceSceneToWav (s, snd, bs, file, &bounceProgress);
        {
            const juce::ScopedLock sl (bounceLock);
            finishedBounce = ok ? file : juce::File();
        }
        bouncing = false;
    });
    bounceThread->startThread();
    return true;
}

juce::File RicochetProcessor::takeFinishedBounce()
{
    const juce::ScopedLock sl (bounceLock);
    auto f = finishedBounce;
    finishedBounce = juce::File();
    return f;
}

//==============================================================================
int RicochetProcessor::getNumPrograms()
{
    return (int) ric::factoryPresets().size();
}

void RicochetProcessor::setCurrentProgram (int index)
{
    const auto& presets = ric::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    loadPreset (presets[(size_t) index]);
}

const juce::String RicochetProcessor::getProgramName (int index)
{
    const auto& presets = ric::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void RicochetProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    ric::NamedScene ns { presetName, presetCategory, scene };
    ns.scene.sound = currentSound();
    state.setProperty ("scene", ric::sceneToJson (ns), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RicochetProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType())) return;

    const auto json = tree.getProperty ("scene").toString();
    tree.removeProperty ("scene", nullptr);
    apvts.replaceState (tree);

    ric::NamedScene ns;
    if (json.isNotEmpty() && ric::sceneFromJson (json, ns))
    {
        presetName = ns.name;
        presetCategory = ns.category;
        setScene (ns.scene, false);
        resetSimulation();
    }
}

juce::AudioProcessorEditor* RicochetProcessor::createEditor()
{
    return new RicochetEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RicochetProcessor();
}
