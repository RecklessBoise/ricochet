#pragma once

#include "Scene.h"

namespace ric
{

struct Vec2
{
    float x = 0.0f, y = 0.0f;
    Vec2 operator+ (Vec2 o) const noexcept { return { x + o.x, y + o.y }; }
    Vec2 operator- (Vec2 o) const noexcept { return { x - o.x, y - o.y }; }
    Vec2 operator* (float s) const noexcept { return { x * s, y * s }; }
    float dot (Vec2 o) const noexcept      { return x * o.x + y * o.y; }
    float lengthSq() const noexcept        { return x * x + y * y; }
    float length() const noexcept          { return std::sqrt (lengthSq()); }
};

struct Ball
{
    bool  alive   = false;
    int   id      = 0;
    int   emitter = -1;
    float x = 0, y = 0, vx = 0, vy = 0;
    float radius  = 0.012f;
    float age     = 0.0f;
    float life    = 0.0f;
    int   lastNote = -1;
    std::array<juce::uint32, (kMaxObstacles + 31) / 32> touching {};

    bool isTouching (int i) const noexcept { return (touching[(size_t) (i >> 5)] >> (i & 31)) & 1u; }
    void setTouching (int i, bool t) noexcept
    {
        auto& w = touching[(size_t) (i >> 5)];
        const auto bit = 1u << (i & 31);
        w = t ? (w | bit) : (w & ~bit);
    }
};

struct HitEvent
{
    int   obstacle     = -1;
    int   note         = 60;
    int   engine       = 0;
    float velocity     = 1.0f;
    float gain         = 1.0f;
    float pan          = 0.0f;
    float x = 0, y = 0;
    int   sampleOffset = 0;
    double beat        = 0.0;
};

/** Geometry of an obstacle in world space, rebuilt whenever the scene changes. */
struct CompiledObstacle
{
    static constexpr int kMaxVerts = 24;

    bool  active    = false;
    bool  circle    = false;
    bool  closed    = true;
    float cx = 0, cy = 0;
    float radius    = 0;   // circles and broad phase
    float thickness = 0;   // open shapes
    int   numVerts  = 0;
    std::array<Vec2, kMaxVerts> v {};
};

/** The MIDI note an obstacle plays, taking the world's pitch mode and scale into account. */
int effectiveNote (const World& w, const Obstacle& o) noexcept;

/** Builds the polygon outline for an obstacle (local -> world). Shared with the UI. */
void buildOutline (const Obstacle& o, float rotation, CompiledObstacle& out) noexcept;

/** Deterministic 2D marble simulation. Real-time safe: no allocation after construction. */
class Simulation
{
public:
    Simulation();

    void setScene (const Scene& s) noexcept;
    const Scene& getScene() const noexcept { return scene; }
    void reset() noexcept;

    /** Advance the world by numSamples. Hits are appended to out (up to maxOut).
        beatStart is the musical position at the first sample of the block. */
    int process (int numSamples, double sampleRate, double bpm, double beatStart,
                 HitEvent* out, int maxOut) noexcept;

    /** Spawn a ball from every emitter whose rate is "MIDI note". */
    void triggerMidiEmitters() noexcept;
    /** Spawn a ball from a specific emitter now. */
    void fireEmitter (int index) noexcept;

    const std::array<Ball, kMaxBalls>& getBalls() const noexcept { return balls; }
    const std::array<CompiledObstacle, kMaxObstacles>& getCompiled() const noexcept { return compiled; }
    const std::array<float, kMaxObstacles>& getRotations() const noexcept { return rotations; }
    int numAliveBalls() const noexcept;

    static constexpr double kStepsPerSecond = 720.0;

private:
    void compile (int index) noexcept;
    void spawn (int emitterIndex) noexcept;
    void step (float dt, int sampleOffset, double beat, HitEvent* out, int maxOut, int& numOut) noexcept;
    void collideObstacles (Ball& b, int sampleOffset, double beat, HitEvent* out, int maxOut, int& numOut) noexcept;
    void collideBoundary (Ball& b) noexcept;
    void collideBalls() noexcept;
    int  mapNote (const Obstacle& o) const noexcept;

    Scene scene;
    std::array<Ball, kMaxBalls> balls {};
    std::array<CompiledObstacle, kMaxObstacles> compiled {};
    std::array<float, kMaxObstacles> rotations {};
    std::array<float, kMaxObstacles> lastHitTime {};
    std::array<bool, kMaxEmitters> onceFired {};
    std::array<bool, kMaxEmitters> pendingFire {};
    std::array<double, kMaxEmitters> lastSlot {};

    juce::Random random;
    double simTime = 0.0;
    float speed = 1.0f;
    int nextBallId = 1;
    int stepCarry = 0;

public:
    void setSpeed (float s) noexcept { speed = juce::jlimit (0.0f, 4.0f, s); }
    double getTime() const noexcept { return simTime; }
};

} // namespace ric
