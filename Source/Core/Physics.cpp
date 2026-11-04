#include "Physics.h"

namespace ric
{

namespace
{
    constexpr float kMaxSpeed      = 3.0f;
    constexpr float kLineThickness = 0.006f;
    constexpr float kMinHitSpeed   = 0.04f;
    constexpr float kRetrigger     = 0.03f; // seconds per obstacle

    Vec2 closestOnSegment (Vec2 p, Vec2 a, Vec2 b) noexcept
    {
        const Vec2 ab = b - a;
        const float len2 = ab.lengthSq();
        if (len2 < 1.0e-12f) return a;
        const float t = juce::jlimit (0.0f, 1.0f, (p - a).dot (ab) / len2);
        return a + ab * t;
    }

    bool pointInPolygon (Vec2 p, const CompiledObstacle& c) noexcept
    {
        bool inside = false;
        for (int i = 0, j = c.numVerts - 1; i < c.numVerts; j = i++)
        {
            const auto& a = c.v[(size_t) i];
            const auto& b = c.v[(size_t) j];
            if (((a.y > p.y) != (b.y > p.y))
                && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y + 1.0e-12f) + a.x))
                inside = ! inside;
        }
        return inside;
    }
}

//==============================================================================
void buildOutline (const Obstacle& o, float rotation, CompiledObstacle& out) noexcept
{
    out.active    = o.active;
    out.cx        = o.x;
    out.cy        = o.y;
    out.circle    = false;
    out.closed    = true;
    out.thickness = 0.0f;
    out.numVerts  = 0;

    const float s = o.size;
    std::array<Vec2, CompiledObstacle::kMaxVerts> local {};
    int n = 0;

    auto ring = [&] (int count, float radius, float startAngle)
    {
        for (int i = 0; i < count && n < CompiledObstacle::kMaxVerts; ++i)
        {
            const float a = startAngle + kTwoPi * (float) i / (float) count;
            local[(size_t) n++] = { radius * std::cos (a), radius * std::sin (a) };
        }
    };

    switch (o.shape)
    {
        case Shape::Circle:
            out.circle = true;
            out.radius = s;
            return;

        case Shape::Rect:
        {
            const float a  = juce::jlimit (0.05f, 20.0f, o.aspect);
            const float hw = a >= 1.0f ? s : s * a;
            const float hh = a >= 1.0f ? s / a : s;
            local[0] = { -hw, -hh }; local[1] = { hw, -hh };
            local[2] = { hw, hh };   local[3] = { -hw, hh };
            n = 4;
            break;
        }

        case Shape::Triangle: ring (3, s, -kPi * 0.5f); break;

        case Shape::Diamond:
            local[0] = { 0.0f, -s }; local[1] = { s * 0.62f, 0.0f };
            local[2] = { 0.0f, s };  local[3] = { -s * 0.62f, 0.0f };
            n = 4;
            break;

        case Shape::Polygon: ring (juce::jlimit (3, 12, o.sides), s, -kPi * 0.5f); break;

        case Shape::Star:
        {
            const int points = juce::jlimit (3, 12, o.sides);
            for (int i = 0; i < points * 2 && n < CompiledObstacle::kMaxVerts; ++i)
            {
                const float a = -kPi * 0.5f + kPi * (float) i / (float) points;
                const float r = (i % 2 == 0) ? s : s * 0.45f;
                local[(size_t) n++] = { r * std::cos (a), r * std::sin (a) };
            }
            break;
        }

        case Shape::Line:
            local[0] = { -s, 0.0f }; local[1] = { s, 0.0f };
            n = 2;
            out.closed = false;
            out.thickness = kLineThickness;
            break;

        case Shape::Arc:
        {
            const float span = kTwoPi * juce::jlimit (0.05f, 1.0f, o.aspect);
            const int count = 16;
            for (int i = 0; i < count; ++i)
            {
                const float a = -kPi * 0.5f - span * 0.5f + span * (float) i / (float) (count - 1);
                local[(size_t) n++] = { s * std::cos (a), s * std::sin (a) };
            }
            out.closed = false;
            out.thickness = kLineThickness;
            break;
        }

        case Shape::NumShapes:
        default:
            out.circle = true;
            out.radius = s;
            return;
    }

    const float cr = std::cos (rotation), sr = std::sin (rotation);
    float maxR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto& p = local[(size_t) i];
        out.v[(size_t) i] = { o.x + p.x * cr - p.y * sr, o.y + p.x * sr + p.y * cr };
        maxR = juce::jmax (maxR, p.length());
    }
    out.numVerts = n;
    out.radius = maxR + out.thickness;
}

//==============================================================================
Simulation::Simulation()
{
    reset();
}

void Simulation::setScene (const Scene& s) noexcept
{
    scene = s;
    for (int i = 0; i < kMaxObstacles; ++i)
    {
        const auto& o = scene.obstacles[(size_t) i];
        rotations[(size_t) i] = o.rotation + o.spin * (float) simTime;
        compile (i);
    }
}

void Simulation::reset() noexcept
{
    for (auto& b : balls) b = Ball();
    simTime = 0.0;
    nextBallId = 1;
    stepCarry = 0;
    random.setSeed ((juce::int64) scene.seed);
    lastHitTime.fill (-1.0f);
    onceFired.fill (false);
    pendingFire.fill (false);
    lastSlot.fill (-1.0e9);

    for (int i = 0; i < kMaxObstacles; ++i)
    {
        rotations[(size_t) i] = scene.obstacles[(size_t) i].rotation;
        compile (i);
    }
}

void Simulation::compile (int index) noexcept
{
    const auto& o = scene.obstacles[(size_t) index];
    auto& c = compiled[(size_t) index];
    if (! o.active)
    {
        c.active = false;
        return;
    }
    buildOutline (o, rotations[(size_t) index], c);
}

int Simulation::numAliveBalls() const noexcept
{
    int n = 0;
    for (auto& b : balls) n += b.alive ? 1 : 0;
    return n;
}

void Simulation::triggerMidiEmitters() noexcept
{
    for (int i = 0; i < kMaxEmitters; ++i)
        if (scene.emitters[(size_t) i].active && scene.emitters[(size_t) i].rate == Rate::Midi)
            pendingFire[(size_t) i] = true;
}

void Simulation::fireEmitter (int index) noexcept
{
    if (juce::isPositiveAndBelow (index, kMaxEmitters))
        pendingFire[(size_t) index] = true;
}

void Simulation::spawn (int emitterIndex) noexcept
{
    const auto& e = scene.emitters[(size_t) emitterIndex];

    // Respect the emitter's ball budget by recycling its oldest ball.
    int ownCount = 0, ownOldest = -1, freeSlot = -1, globalOldest = -1;
    float ownOldestAge = -1.0f, globalOldestAge = -1.0f;
    for (int i = 0; i < kMaxBalls; ++i)
    {
        const auto& b = balls[(size_t) i];
        if (! b.alive)
        {
            if (freeSlot < 0) freeSlot = i;
            continue;
        }
        if (b.age > globalOldestAge) { globalOldestAge = b.age; globalOldest = i; }
        if (b.emitter == emitterIndex)
        {
            ++ownCount;
            if (b.age > ownOldestAge) { ownOldestAge = b.age; ownOldest = i; }
        }
    }

    int slot = freeSlot;
    if (ownCount >= e.maxBalls && ownOldest >= 0) slot = ownOldest;
    if (slot < 0) slot = globalOldest;
    if (slot < 0) return;

    const float angle = e.angle + (random.nextFloat() * 2.0f - 1.0f) * e.spread;
    Ball b;
    b.alive   = true;
    b.id      = nextBallId++;
    b.emitter = emitterIndex;
    b.x       = e.x;
    b.y       = e.y;
    b.vx      = std::cos (angle) * e.speed;
    b.vy      = std::sin (angle) * e.speed;
    b.radius  = e.ballSize;
    b.life    = e.life;
    balls[(size_t) slot] = b;
}

int effectiveNote (const World& w, const Obstacle& o) noexcept
{
    const int len = scaleLength (w.scale);
    switch (w.pitchMode)
    {
        case PitchMode::BySize:
        {
            const float t = juce::jlimit (0.0f, 1.0f, (o.size - 0.015f) / (0.2f - 0.015f));
            const int degree = juce::roundToInt ((1.0f - t) * (float) (w.range * len));
            return scaleDegreeToNote (degree, w.scale, w.root, w.octave);
        }
        case PitchMode::ByHeight:
        {
            const float t = juce::jlimit (0.0f, 1.0f, 1.0f - o.y);
            const int degree = juce::roundToInt (t * (float) (w.range * len));
            return scaleDegreeToNote (degree, w.scale, w.root, w.octave);
        }
        case PitchMode::Manual:
        case PitchMode::NumModes:
        default:
            return quantizeToScale (o.note, w.scale, w.root);
    }
}

int Simulation::mapNote (const Obstacle& o) const noexcept
{
    return effectiveNote (scene.world, o);
}

//==============================================================================
void Simulation::collideObstacles (Ball& b, int sampleOffset, double beat,
                                   HitEvent* out, int maxOut, int& numOut) noexcept
{
    const Vec2 p { b.x, b.y };

    for (int i = 0; i < kMaxObstacles; ++i)
    {
        const auto& c = compiled[(size_t) i];
        if (! c.active) continue;

        const float dx = b.x - c.cx, dy = b.y - c.cy;
        const float reach = c.radius + b.radius + 0.002f;
        if (dx * dx + dy * dy > reach * reach)
        {
            b.setTouching (i, false);
            continue;
        }

        bool hit = false;
        Vec2 n;
        float pen = 0.0f;

        if (c.circle)
        {
            const float d = std::sqrt (dx * dx + dy * dy);
            if (d < c.radius + b.radius)
            {
                n = d > 1.0e-6f ? Vec2 { dx / d, dy / d } : Vec2 { 0.0f, -1.0f };
                pen = c.radius + b.radius - d;
                hit = true;
            }
        }
        else
        {
            Vec2 best;
            float bestD2 = 1.0e9f;
            int bestEdge = 0;
            const int edges = c.closed ? c.numVerts : c.numVerts - 1;
            for (int e = 0; e < edges; ++e)
            {
                const auto& a  = c.v[(size_t) e];
                const auto& bb = c.v[(size_t) ((e + 1) % c.numVerts)];
                const Vec2 q = closestOnSegment ({ b.x, b.y }, a, bb);
                const float d2 = (Vec2 { b.x, b.y } - q).lengthSq();
                if (d2 < bestD2) { bestD2 = d2; best = q; bestEdge = e; }
            }

            const float d = std::sqrt (bestD2);
            const bool inside = c.closed && pointInPolygon ({ b.x, b.y }, c);

            auto edgeNormal = [&]
            {
                const auto& a  = c.v[(size_t) bestEdge];
                const auto& bb = c.v[(size_t) ((bestEdge + 1) % c.numVerts)];
                Vec2 t = bb - a;
                const float l = juce::jmax (1.0e-6f, t.length());
                Vec2 nn { t.y / l, -t.x / l };
                // point away from the obstacle centre
                if ((Vec2 { (a.x + bb.x) * 0.5f, (a.y + bb.y) * 0.5f } - Vec2 { c.cx, c.cy }).dot (nn) < 0.0f)
                    nn = nn * -1.0f;
                return nn;
            };

            if (inside)
            {
                n = d > 1.0e-6f ? (best - Vec2 { b.x, b.y }) * (1.0f / d) : edgeNormal();
                pen = b.radius + d;
                hit = true;
            }
            else if (d < b.radius + c.thickness)
            {
                n = d > 1.0e-6f ? (Vec2 { b.x, b.y } - best) * (1.0f / d) : edgeNormal();
                pen = b.radius + c.thickness - d;
                hit = true;
            }
        }

        if (! hit)
        {
            b.setTouching (i, false);
            continue;
        }

        const auto& o = scene.obstacles[(size_t) i];
        b.x += n.x * (pen + 1.0e-5f);
        b.y += n.y * (pen + 1.0e-5f);

        const float vn = b.vx * n.x + b.vy * n.y;
        if (vn < 0.0f)
        {
            const float k = (1.0f + o.bounce) * vn;
            b.vx -= n.x * k;
            b.vy -= n.y * k;

            const float impact = -vn;
            const bool fresh = ! b.isTouching (i);
            if (fresh && impact > kMinHitSpeed
                && (simTime - (double) lastHitTime[(size_t) i]) > (double) kRetrigger)
            {
                lastHitTime[(size_t) i] = (float) simTime;
                if (! o.muted && numOut < maxOut)
                {
                    auto& h = out[numOut++];
                    h.obstacle     = i;
                    h.note         = mapNote (o);
                    h.engine       = o.engine;
                    h.velocity     = juce::jlimit (0.12f, 1.0f, impact / 1.2f);
                    h.gain         = o.gain;
                    h.pan          = juce::jlimit (-1.0f, 1.0f, (o.x * 2.0f - 1.0f) * 0.75f);
                    h.x            = b.x;
                    h.y            = b.y;
                    h.sampleOffset = sampleOffset;
                    h.beat         = beat;
                    b.lastNote     = h.note;
                }
            }
        }
        b.setTouching (i, true);
    }
    juce::ignoreUnused (p);
}

void Simulation::collideBoundary (Ball& b) noexcept
{
    const float wb = scene.world.wallBounce;
    const float r = b.radius;

    switch (scene.world.boundary)
    {
        case Boundary::Box:
            if (b.x < r)        { b.x = r;        b.vx =  std::abs (b.vx) * wb; }
            if (b.x > 1.0f - r) { b.x = 1.0f - r; b.vx = -std::abs (b.vx) * wb; }
            if (b.y < r)        { b.y = r;        b.vy =  std::abs (b.vy) * wb; }
            if (b.y > 1.0f - r) { b.y = 1.0f - r; b.vy = -std::abs (b.vy) * wb; }
            break;

        case Boundary::Circle:
        {
            const float dx = b.x - 0.5f, dy = b.y - 0.5f;
            const float d = std::sqrt (dx * dx + dy * dy);
            const float lim = 0.5f - r;
            if (d > lim && d > 1.0e-6f)
            {
                const Vec2 no { dx / d, dy / d };
                b.x = 0.5f + no.x * lim;
                b.y = 0.5f + no.y * lim;
                const float vn = b.vx * no.x + b.vy * no.y;
                if (vn > 0.0f)
                {
                    b.vx -= no.x * (1.0f + wb) * vn;
                    b.vy -= no.y * (1.0f + wb) * vn;
                }
            }
            break;
        }

        case Boundary::Hexagon:
        {
            for (int k = 0; k < 6; ++k)
            {
                const float a0 = kTwoPi * (float) k / 6.0f;
                const float a1 = kTwoPi * (float) (k + 1) / 6.0f;
                const Vec2 p0 { 0.5f + 0.5f * std::cos (a0), 0.5f + 0.5f * std::sin (a0) };
                const Vec2 p1 { 0.5f + 0.5f * std::cos (a1), 0.5f + 0.5f * std::sin (a1) };
                const Vec2 mid { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
                Vec2 ni = Vec2 { 0.5f, 0.5f } - mid;
                ni = ni * (1.0f / ni.length());
                const float sd = (Vec2 { b.x, b.y } - p0).dot (ni);
                if (sd < r)
                {
                    b.x += ni.x * (r - sd);
                    b.y += ni.y * (r - sd);
                    const float vn = b.vx * ni.x + b.vy * ni.y;
                    if (vn < 0.0f)
                    {
                        b.vx -= ni.x * (1.0f + wb) * vn;
                        b.vy -= ni.y * (1.0f + wb) * vn;
                    }
                }
            }
            break;
        }

        case Boundary::Open:
            if (b.x < -0.05f || b.x > 1.05f || b.y < -0.05f || b.y > 1.05f)
                b.alive = false;
            break;

        case Boundary::NumBoundaries:
        default:
            break;
    }
}

void Simulation::collideBalls() noexcept
{
    for (int i = 0; i < kMaxBalls; ++i)
    {
        auto& a = balls[(size_t) i];
        if (! a.alive) continue;
        for (int j = i + 1; j < kMaxBalls; ++j)
        {
            auto& b = balls[(size_t) j];
            if (! b.alive) continue;
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float minD = a.radius + b.radius;
            const float d2 = dx * dx + dy * dy;
            if (d2 >= minD * minD || d2 < 1.0e-12f) continue;

            const float d = std::sqrt (d2);
            const Vec2 n { dx / d, dy / d };
            const float push = (minD - d) * 0.5f;
            a.x -= n.x * push; a.y -= n.y * push;
            b.x += n.x * push; b.y += n.y * push;

            const float rel = (a.vx - b.vx) * n.x + (a.vy - b.vy) * n.y;
            if (rel > 0.0f)
            {
                a.vx -= n.x * rel; a.vy -= n.y * rel;
                b.vx += n.x * rel; b.vy += n.y * rel;
            }
        }
    }
}

void Simulation::step (float dt, int sampleOffset, double beat,
                       HitEvent* out, int maxOut, int& numOut) noexcept
{
    simTime += (double) dt;

    for (int i = 0; i < kMaxObstacles; ++i)
    {
        const auto& o = scene.obstacles[(size_t) i];
        if (o.active && o.spin != 0.0f)
        {
            rotations[(size_t) i] = o.rotation + o.spin * (float) simTime;
            compile (i);
        }
    }

    const auto& w = scene.world;
    const float gx = std::cos (w.gravityAngle) * w.gravity;
    const float gy = std::sin (w.gravityAngle) * w.gravity;
    const float damp = juce::jmax (0.0f, 1.0f - w.friction * dt);

    for (auto& b : balls)
    {
        if (! b.alive) continue;

        b.vx = (b.vx + gx * dt) * damp;
        b.vy = (b.vy + gy * dt) * damp;

        float sp = std::sqrt (b.vx * b.vx + b.vy * b.vy);
        if (sp > kMaxSpeed)
        {
            b.vx *= kMaxSpeed / sp;
            b.vy *= kMaxSpeed / sp;
            sp = kMaxSpeed;
        }

        const int sub = juce::jlimit (1, 8, (int) std::ceil (sp * dt / (b.radius * 0.5f)));
        const float h = dt / (float) sub;
        for (int s = 0; s < sub && b.alive; ++s)
        {
            b.x += b.vx * h;
            b.y += b.vy * h;
            collideObstacles (b, sampleOffset, beat, out, maxOut, numOut);
            collideBoundary (b);
        }

        b.age += dt;
        if (b.life > 0.0f && b.age > b.life)
            b.alive = false;
        if (! std::isfinite (b.x) || ! std::isfinite (b.y))
            b.alive = false;
    }

    if (w.ballCollisions)
        collideBalls();
}

int Simulation::process (int numSamples, double sampleRate, double bpm, double beatStart,
                         HitEvent* out, int maxOut) noexcept
{
    int numOut = 0;
    const int stepLen = juce::jmax (1, juce::roundToInt (sampleRate / kStepsPerSecond));
    const double beatsPerSample = bpm / 60.0 / sampleRate;
    const float dt = (float) ((double) stepLen / sampleRate) * speed;

    int pos = stepCarry;
    while (pos < numSamples)
    {
        const double beat = beatStart + (double) pos * beatsPerSample;

        for (int e = 0; e < kMaxEmitters; ++e)
        {
            const auto& em = scene.emitters[(size_t) e];
            if (! em.active) continue;

            bool fire = pendingFire[(size_t) e];
            pendingFire[(size_t) e] = false;

            if (em.rate == Rate::Once)
            {
                if (! onceFired[(size_t) e]) { fire = true; onceFired[(size_t) e] = true; }
            }
            else
            {
                const double rb = rateBeats (em.rate);
                if (rb > 0.0)
                {
                    const double slot = std::floor ((beat + 1.0e-9) / rb);
                    if (slot != lastSlot[(size_t) e])
                    {
                        lastSlot[(size_t) e] = slot;
                        fire = true;
                    }
                }
            }

            if (fire) spawn (e);
        }

        if (dt > 0.0f)
            step (dt, pos, beat, out, maxOut, numOut);

        pos += stepLen;
    }
    stepCarry = pos - numSamples;
    return numOut;
}

} // namespace ric
