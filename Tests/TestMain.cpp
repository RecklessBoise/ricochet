#include "Core/Scene.h"
#include "Core/Physics.h"
#include "Core/Presets.h"
#include "Core/Randomizer.h"
#include "Core/RenderEngine.h"
#include "Core/OfflineRenderer.h"
#include "Synth/Synth.h"
#include "Core/Transport.h"

using namespace ric;

namespace
{
    constexpr double kSr = 48000.0;

    int simulateHits (const Scene& scene, double seconds, std::vector<HitEvent>* hitsOut = nullptr,
                      int blockSize = 512, bool* finite = nullptr)
    {
        Simulation sim;
        sim.setScene (scene);
        sim.reset();
        std::array<HitEvent, 256> hits;
        int total = 0;
        double beat = 0.0;
        const auto blocks = (int) (seconds * kSr / blockSize);
        if (finite) *finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            const int n = sim.process (blockSize, kSr, 120.0, beat, hits.data(), (int) hits.size());
            total += n;
            if (hitsOut)
                for (int i = 0; i < n; ++i) hitsOut->push_back (hits[(size_t) i]);
            beat += blockSize * 2.0 / kSr;
            if (finite)
                for (auto& ball : sim.getBalls())
                    if (ball.alive && ! (std::isfinite (ball.x) && std::isfinite (ball.y))) *finite = false;
        }
        return total;
    }

    Scene singleWallScene (float speed)
    {
        Scene s;
        s.world.boundary = Boundary::Open;
        Obstacle wall;
        wall.shape = Shape::Line;
        wall.x = 0.5f; wall.y = 0.5f; wall.size = 0.3f;
        wall.rotation = kPi * 0.5f; // vertical line
        wall.note = 64;
        s.addObstacle (wall);
        Emitter e;
        e.x = 0.1f; e.y = 0.5f; e.angle = 0.0f; e.speed = speed; e.rate = Rate::Once; e.life = 0.0f;
        s.addEmitter (e);
        return s;
    }
}

//==============================================================================
struct ScaleTests : juce::UnitTest
{
    ScaleTests() : juce::UnitTest ("Scales", "Ricochet") {}

    void runTest() override
    {
        beginTest ("quantize to C major");
        expectEquals (quantizeToScale (61, 1, 0), 60);
        expectEquals (quantizeToScale (66, 1, 0), 65);
        expectEquals (quantizeToScale (64, 1, 0), 64);

        beginTest ("degree to note");
        expectEquals (scaleDegreeToNote (0, 1, 0, 4), 60);
        expectEquals (scaleDegreeToNote (7, 1, 0, 4), 72);
        expectEquals (scaleDegreeToNote (-1, 1, 0, 4), 59);
        expectEquals (scaleDegreeToNote (2, 7, 9, 3), 61); // A pentatonic major: A B C#
    }
};

struct PhysicsTests : juce::UnitTest
{
    PhysicsTests() : juce::UnitTest ("Physics", "Ricochet") {}

    void runTest() override
    {
        beginTest ("ball bounces off a line and triggers exactly one note");
        {
            std::vector<HitEvent> hits;
            simulateHits (singleWallScene (0.8f), 1.5, &hits);
            expectEquals ((int) hits.size(), 1);
            if (! hits.empty()) expectEquals (hits[0].note, 64);
        }

        beginTest ("fast ball does not tunnel through a thin line");
        {
            std::vector<HitEvent> hits;
            simulateHits (singleWallScene (3.0f), 0.6, &hits);
            expectEquals ((int) hits.size(), 1);
        }

        beginTest ("elastic bounce preserves speed");
        {
            Simulation sim;
            auto s = singleWallScene (0.8f);
            sim.setScene (s);
            sim.reset();
            std::array<HitEvent, 64> h;
            for (int i = 0; i < 200; ++i) sim.process (480, kSr, 120.0, 0.0, h.data(), 64);
            for (auto& b : sim.getBalls())
                if (b.alive)
                {
                    expectWithinAbsoluteError (std::sqrt (b.vx * b.vx + b.vy * b.vy), 0.8f, 0.01f);
                    expect (b.vx < 0.0f, "ball should travel back to the left");
                }
        }

        beginTest ("simulation is deterministic and block-size independent");
        {
            const auto& preset = factoryPresets()[30];
            std::vector<HitEvent> a, b;
            simulateHits (preset.scene, 6.0, &a, 512);
            simulateHits (preset.scene, 6.0, &b, 128);
            expectEquals ((int) a.size(), (int) b.size());
            bool same = a.size() == b.size();
            for (size_t i = 0; same && i < a.size(); ++i)
                same = a[i].note == b[i].note && a[i].obstacle == b[i].obstacle;
            expect (same, "hit sequences differ");
        }

        beginTest ("each shape can be hit");
        for (int sh = 0; sh < (int) Shape::NumShapes; ++sh)
        {
            Scene s;
            s.world.boundary = Boundary::Open;
            Obstacle o;
            o.shape = (Shape) sh; o.x = 0.5f; o.y = 0.5f; o.size = 0.08f; o.aspect = sh == (int) Shape::Arc ? 0.6f : 1.0f;
            o.rotation = sh == (int) Shape::Line ? kPi * 0.5f : (sh == (int) Shape::Arc ? -kPi * 0.5f : 0.0f);
            s.addObstacle (o);
            Emitter e; e.x = 0.1f; e.y = 0.5f; e.angle = 0.0f; e.speed = 0.8f; e.rate = Rate::Once; e.life = 0.0f;
            s.addEmitter (e);
            expectGreaterOrEqual (simulateHits (s, 1.5), 1, "shape " + shapeNames()[sh]);
        }
    }
};

struct PresetTests : juce::UnitTest
{
    PresetTests() : juce::UnitTest ("Presets", "Ricochet") {}

    void runTest() override
    {
        const auto& presets = factoryPresets();

        beginTest ("there are lots of presets");
        expectGreaterOrEqual ((int) presets.size(), 150);

        beginTest ("names are unique");
        {
            juce::StringArray names;
            for (auto& p : presets) names.add (p.name);
            names.removeDuplicates (false);
            expectEquals (names.size(), (int) presets.size());
        }

        beginTest ("every preset makes notes within 12 seconds and stays finite");
        for (auto& p : presets)
        {
            bool finite = true;
            const int hits = simulateHits (p.scene, 12.0, nullptr, 512, &finite);
            expect (hits > 0, p.name + " is silent");
            expect (finite, p.name + " produced NaN");
            expect (p.scene.numActiveEmitters() > 0, p.name + " has no emitter");
        }

        beginTest ("JSON round trip");
        for (size_t i = 0; i < presets.size(); i += 7)
        {
            NamedScene back;
            expect (sceneFromJson (sceneToJson (presets[i]), back));
            expectEquals (back.name, presets[i].name);
            expectEquals (back.scene.numActiveObstacles(), presets[i].scene.numActiveObstacles());
            expectEquals (back.scene.numActiveEmitters(), presets[i].scene.numActiveEmitters());
            expectEquals (sceneToJson (back), sceneToJson (presets[i]));
        }

        beginTest ("garbage JSON is rejected");
        {
            NamedScene ns;
            expect (! sceneFromJson ("not json", ns));
            expect (! sceneFromJson ("[1,2,3]", ns));
        }
    }
};

struct RandomTests : juce::UnitTest
{
    RandomTests() : juce::UnitTest ("Randomizer", "Ricochet") {}

    void runTest() override
    {
        beginTest ("same seed, same scene");
        RandomOptions opt;
        const auto a = randomScene (42, opt, Scene());
        const auto b = randomScene (42, opt, Scene());
        expectEquals (sceneToVar (a).toString().length(), sceneToVar (b).toString().length());
        expectEquals (juce::JSON::toString (sceneToVar (a)), juce::JSON::toString (sceneToVar (b)));

        beginTest ("random scenes are playable");
        int silent = 0;
        for (juce::uint32 seed = 1; seed <= 60; ++seed)
        {
            const auto s = randomScene (seed, opt, Scene());
            expectGreaterOrEqual (s.numActiveObstacles(), 3);
            expectGreaterOrEqual (s.numActiveEmitters(), 1);
            if (simulateHits (s, 12.0) == 0) ++silent;
        }
        expectLessOrEqual (silent, 3, "too many silent random scenes");

        beginTest ("locks are respected");
        {
            const auto base = factoryPresets()[5].scene;
            RandomOptions locked;
            locked.lockObstacles = true;
            locked.lockSounds = true;
            const auto r = randomScene (7, locked, base);
            for (size_t i = 0; i < base.obstacles.size(); ++i)
            {
                expect (r.obstacles[i].active == base.obstacles[i].active);
                if (base.obstacles[i].active)
                {
                    expectEquals (r.obstacles[i].x, base.obstacles[i].x);
                    expectEquals (r.obstacles[i].engine, base.obstacles[i].engine);
                }
            }
        }

        beginTest ("mutate keeps structure");
        {
            const auto base = factoryPresets()[20].scene;
            const auto m = mutateScene (base, 3, 0.5f);
            expectEquals (m.numActiveObstacles(), base.numActiveObstacles());
            expectEquals (m.numActiveEmitters(), base.numActiveEmitters());
        }
    }
};

struct AudioTests : juce::UnitTest
{
    AudioTests() : juce::UnitTest ("Audio", "Ricochet") {}

    void runTest() override
    {
        beginTest ("every engine is audible, finite and bounded");
        for (int engine = 0; engine < kNumEngines; ++engine)
        {
            for (int note : { 36, 60, 84 })
            {
                Synth synth;
                synth.prepare (kSr);
                synth.noteOn (engine, note, 1.0f, 0.0f, 1.0f);
                std::vector<float> l (24000, 0.0f), r (24000, 0.0f);
                synth.render (l.data(), r.data(), 0, 24000);
                float peak = 0.0f;
                bool finite = true;
                for (auto x : l) { peak = juce::jmax (peak, std::abs (x)); finite = finite && std::isfinite (x); }
                expect (finite, engineNames()[engine] + " not finite");
                expect (peak > 0.01f, engineNames()[engine] + " silent at note " + juce::String (note));
                expect (peak < 4.0f, engineNames()[engine] + " too loud");
            }
        }

        beginTest ("voices are released");
        {
            Synth synth;
            synth.prepare (kSr);
            for (int i = 0; i < 40; ++i) synth.noteOn (i % kNumEngines, 48 + i, 0.8f, 0.0f, 1.0f);
            expectEquals (synth.numActiveVoices(), Synth::kNumVoices);
            std::vector<float> l (512), r (512);
            for (int b = 0; b < (int) (kSr * 40 / 512); ++b) synth.render (l.data(), r.data(), 0, 512);
            expectEquals (synth.numActiveVoices(), 0);
        }

        beginTest ("offline render matches hit count and stays in range");
        {
            const auto& p = factoryPresets()[0];
            BounceSettings bs;
            bs.bars = 2;
            int hits = 0;
            auto buffer = renderScene (p.scene, p.scene.sound, bs, nullptr, &hits);
            expect (hits > 0);
            expect (buffer.getMagnitude (0, buffer.getNumSamples()) > 0.01f);
            expect (buffer.getMagnitude (0, buffer.getNumSamples()) <= 1.0f);
        }

        beginTest ("bounce writes a readable wav");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getNonexistentChildFile ("ricochet_test", ".wav");
            BounceSettings bs;
            bs.bars = 1;
            bs.tailSeconds = 0.5;
            juce::String err;
            expect (bounceSceneToWav (factoryPresets()[60].scene, factoryPresets()[60].scene.sound, bs, file, nullptr, &err), err);
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            expect (reader != nullptr);
            if (reader != nullptr)
            {
                expectEquals ((int) reader->numChannels, 2);
                expectEquals ((int) reader->bitsPerSample, 24);
                expectGreaterThan ((int) reader->lengthInSamples, (int) (kSr * 2.0));
            }
            file.deleteFile();
        }

        beginTest ("quantize delays notes onto the grid");
        {
            RenderEngine eng;
            eng.prepare (kSr, 512);
            auto s = singleWallScene (0.8f);
            s.world.quantize = Quantize::Sixteenth;
            eng.setScene (s);
            eng.reset();
            juce::MidiBuffer midi;
            std::vector<float> l (512), r (512);
            const double samplesPerSixteenth = 0.25 * 60.0 / 120.0 * kSr;
            bool found = false;
            for (int b = 0; b < 200 && ! found; ++b)
            {
                midi.clear();
                eng.process (l.data(), r.data(), 512, 120.0, -1.0, &midi, nullptr);
                for (const auto m : midi)
                    if (m.getMessage().isNoteOn())
                    {
                        const double abs = (double) b * 512.0 + m.samplePosition;
                        const double rem = std::fmod (abs, samplesPerSixteenth);
                        expect (rem < 1.5 || samplesPerSixteenth - rem < 1.5, "note not on grid: " + juce::String (rem));
                        found = true;
                    }
            }
            expect (found, "no note emitted");
        }
    }
};


struct TransportTests : juce::UnitTest
{
    TransportTests() : juce::UnitTest ("DAW Sync", "Ricochet") {}

    void runTest() override
    {
        HostInfo host;
        host.available = true; host.bpm = 97.0; host.hasPpq = true; host.ppq = 12.5;

        beginTest ("sync off: own play button and own tempo");
        {
            TransportState st;
            host.playing = true;
            auto d = decideTransport (false, false, 140.0, host, st);
            expect (! d.run);
            expectEquals (d.bpm, 140.0);
            expectEquals (d.hostBeat, -1.0);
            expect (! d.following);
            d = decideTransport (false, true, 140.0, host, st);
            expect (d.run);
        }

        beginTest ("sync on: DAW play starts, DAW stop stops, tempo follows");
        {
            TransportState st;
            host.playing = false;
            auto d = decideTransport (true, true, 140.0, host, st);
            expect (d.following);
            expect (! d.run, "must stay stopped while the DAW is stopped, even if Play is on");
            expectEquals (d.bpm, 97.0);

            host.playing = true;
            d = decideTransport (true, false, 140.0, host, st);
            expect (d.run, "DAW play must start the plug-in even if its own Play is off");
            expect (d.reset, "each DAW play restarts from the initial state");
            expectEquals (d.hostBeat, 12.5);
            expectEquals (d.bpm, 97.0);

            d = decideTransport (true, false, 140.0, host, st);
            expect (d.run);
            expect (! d.reset, "no reset while the DAW keeps playing");

            host.bpm = 174.0;
            d = decideTransport (true, false, 140.0, host, st);
            expectEquals (d.bpm, 174.0, "tempo changes in the DAW are followed live");

            host.playing = false;
            d = decideTransport (true, true, 140.0, host, st);
            expect (! d.run);
            host.playing = true;
            d = decideTransport (true, true, 140.0, host, st);
            expect (d.reset, "a new DAW play restarts again");
        }

        beginTest ("sync on without a host (standalone): runs free");
        {
            TransportState st;
            HostInfo none;
            const auto d = decideTransport (true, true, 110.0, none, st);
            expect (d.run);
            expect (! d.following);
            expectEquals (d.bpm, 110.0);
        }

        beginTest ("host beat drives emitters in sync");
        {
            RenderEngine eng;
            eng.prepare (kSr, 512);
            Scene s;
            Emitter e; e.x = 0.5f; e.y = 0.5f; e.rate = Rate::Quarter; e.life = 1.0f;
            s.addEmitter (e);
            eng.setScene (s);
            eng.reset();
            std::vector<float> l (512), r (512);
            eng.process (l.data(), r.data(), 512, 120.0, 3.90, nullptr, nullptr);
            const int first = eng.getSimulation().numAliveBalls();
            double beat = 3.90;
            for (int i = 0; i < 6; ++i)
            {
                beat += 512.0 * 2.0 / kSr;
                eng.process (l.data(), r.data(), 512, 120.0, beat, nullptr, nullptr);
            }
            expectEquals (first, 1);
            expectEquals (eng.getSimulation().numAliveBalls(), 2, "a ball must be launched on the DAW's next beat");
        }
    }
};

static TransportTests transportTests;
static ScaleTests scaleTests;
static PhysicsTests physicsTests;
static PresetTests presetTests;
static RandomTests randomTests;
static AudioTests audioTests;

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory (juce::SystemStats::getEnvironmentVariable ("RICOCHET_TEST_CATEGORY", "Ricochet"));

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}

