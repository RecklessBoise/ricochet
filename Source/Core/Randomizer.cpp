#include "Randomizer.h"
#include "Physics.h"

namespace ric
{

namespace
{
    float range (juce::Random& r, float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }

    template <typename T, size_t N>
    T pick (juce::Random& r, const T (&items)[N]) { return items[(size_t) r.nextInt ((int) N)]; }

    bool overlaps (const Scene& s, float x, float y, float radius)
    {
        for (auto& o : s.obstacles)
            if (o.active)
            {
                const float dx = o.x - x, dy = o.y - y;
                const float min = o.size + radius + 0.025f;
                if (dx * dx + dy * dy < min * min) return true;
            }
        for (auto& e : s.emitters)
            if (e.active)
            {
                const float dx = e.x - x, dy = e.y - y;
                const float min = radius + 0.06f;
                if (dx * dx + dy * dy < min * min) return true;
            }
        return false;
    }

    bool insideBoundary (Boundary b, float x, float y, float radius)
    {
        switch (b)
        {
            case Boundary::Circle:
            case Boundary::Hexagon:
            {
                const float dx = x - 0.5f, dy = y - 0.5f;
                const float lim = (b == Boundary::Circle ? 0.5f : 0.43f) - radius - 0.03f;
                return dx * dx + dy * dy < lim * lim;
            }
            case Boundary::Box:
            case Boundary::Open:
            case Boundary::NumBoundaries:
            default:
                return x > radius + 0.03f && x < 1.0f - radius - 0.03f
                    && y > radius + 0.03f && y < 1.0f - radius - 0.03f;
        }
    }

    void randomiseWorld (juce::Random& r, World& w, const RandomOptions& opt)
    {
        const Boundary boundaries[] { Boundary::Box, Boundary::Box, Boundary::Circle, Boundary::Hexagon, Boundary::Open };
        w.boundary = pick (r, boundaries);
        const bool gravityOn = r.nextFloat() < 0.3f + opt.chaos * 0.5f || w.boundary == Boundary::Open;
        w.gravity = gravityOn ? range (r, 0.2f, 0.6f + opt.chaos * 0.8f) : 0.0f;
        w.gravityAngle = r.nextFloat() < 0.85f ? kPi * 0.5f : range (r, 0.0f, kTwoPi);
        w.friction = r.nextFloat() < 0.3f ? range (r, 0.0f, 0.15f) : 0.0f;
        w.wallBounce = gravityOn ? range (r, 0.6f, 0.95f) : 1.0f;
        w.ballCollisions = r.nextFloat() < opt.chaos * 0.5f;
        if (! opt.keepScale)
        {
            const int musical[] { 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 4, 11 };
            w.scale = pick (r, musical);
            w.root = r.nextInt (12);
        }
        const float pm = r.nextFloat();
        w.pitchMode = pm < 0.7f ? PitchMode::Manual : pm < 0.85f ? PitchMode::BySize : PitchMode::ByHeight;
        w.octave = 3 + r.nextInt (2);
        w.range = 2 + r.nextInt (2);
        const Quantize qs[] { Quantize::Off, Quantize::Off, Quantize::Sixteenth, Quantize::Eighth, Quantize::ThirtySecond };
        w.quantize = pick (r, qs);
    }

    void randomiseSound (juce::Random& r, SoundSettings& s)
    {
        s.brightness = range (r, 0.25f, 0.9f);
        s.decay      = range (r, 0.25f, 0.8f);
        s.attack     = r.nextFloat() < 0.75f ? 0.0f : range (r, 0.05f, 0.5f);
        s.noteLength = range (r, 0.1f, 1.2f);
        s.reverbMix  = range (r, 0.1f, 0.6f);
        s.roomSize   = range (r, 0.4f, 0.92f);
        s.delayMix   = r.nextFloat() < 0.6f ? range (r, 0.05f, 0.35f) : 0.0f;
        s.delayDiv   = r.nextInt (delayDivisionNames().size());
        s.feedback   = range (r, 0.15f, 0.6f);
        s.drive      = r.nextFloat() < 0.5f ? range (r, 0.0f, 0.4f) : 0.0f;
    }

    void randomiseEmitters (juce::Random& r, Scene& s, const RandomOptions& opt)
    {
        for (auto& e : s.emitters) e = Emitter();

        const int count = 1 + r.nextInt (2) + (r.nextFloat() < opt.chaos ? 1 : 0);
        const Rate rates[] { Rate::Quarter, Rate::Eighth, Rate::Half, Rate::OneBar, Rate::QuarterTriplet,
                             Rate::DottedQuarter, Rate::Once, Rate::Sixteenth, Rate::TwoBars };
        const bool gravityDown = s.world.gravity > 0.0f && std::abs (s.world.gravityAngle - kPi * 0.5f) < 0.1f;

        for (int i = 0; i < count; ++i)
        {
            Emitter e;
            if (gravityDown)
            {
                e.x = range (r, 0.15f, 0.85f);
                e.y = range (r, 0.05f, 0.15f);
                e.angle = kPi * 0.5f + range (r, -0.6f, 0.6f);
                e.speed = range (r, 0.05f, 0.4f);
            }
            else
            {
                const float a = range (r, 0.0f, kTwoPi);
                const float rad = range (r, 0.0f, 0.3f);
                e.x = 0.5f + std::cos (a) * rad;
                e.y = 0.5f + std::sin (a) * rad;
                e.angle = range (r, 0.0f, kTwoPi);
                e.speed = range (r, 0.25f, 0.6f + opt.chaos * 0.7f);
            }
            e.rate = pick (r, rates);
            e.maxBalls = e.rate == Rate::Once ? 1 : 2 + r.nextInt (8);
            e.life = e.rate == Rate::Once ? 0.0f : range (r, 3.0f, 12.0f);
            e.spread = r.nextFloat() < 0.5f ? range (r, 0.0f, 0.5f) : 0.0f;
            e.ballSize = range (r, 0.009f, 0.018f);
            s.addEmitter (e);
        }
    }

    void randomiseObstacles (juce::Random& r, Scene& s, const RandomOptions& opt, bool assignEngines)
    {
        for (auto& o : s.obstacles) o = Obstacle();

        int palette[3] { r.nextInt (kNumEngines), r.nextInt (kNumEngines), r.nextInt (kNumEngines) };
        const int paletteSize = 1 + r.nextInt (3);
        const int count = 4 + (int) (opt.density * 26.0f * range (r, 0.6f, 1.0f));
        const int layout = r.nextInt (5); // 0 scatter, 1 grid, 2 ring, 3 mirror, 4 diagonal
        const Shape shapes[] { Shape::Circle, Shape::Circle, Shape::Rect, Shape::Triangle, Shape::Diamond,
                               Shape::Polygon, Shape::Star, Shape::Line, Shape::Arc };
        const Shape mainShape = pick (r, shapes);
        const int len = scaleLength (s.world.scale);

        auto makeObstacle = [&] (float x, float y)
        {
            Obstacle o;
            o.shape    = r.nextFloat() < 0.6f ? mainShape : pick (r, shapes);
            o.x        = x;
            o.y        = y;
            o.size     = range (r, 0.02f, 0.035f + 0.06f * (1.0f - opt.density));
            o.aspect   = o.shape == Shape::Arc ? range (r, 0.2f, 0.6f) : o.shape == Shape::Rect ? range (r, 0.3f, 4.0f) : 1.0f;
            o.rotation = range (r, 0.0f, kTwoPi);
            o.spin     = r.nextFloat() < opt.chaos * 0.35f ? range (r, -1.5f, 1.5f) : 0.0f;
            o.sides    = 5 + r.nextInt (4);
            const int degree = r.nextInt (len * 2 + 1);
            o.note     = scaleDegreeToNote (degree, s.world.scale, s.world.root, 3 + r.nextInt (2));
            o.engine   = palette[r.nextInt (paletteSize)];
            o.bounce   = range (r, 0.8f, 1.0f);
            o.gain     = range (r, 0.7f, 1.0f);
            return o;
        };

        auto tryAdd = [&] (float x, float y)
        {
            auto o = makeObstacle (x, y);
            if (! insideBoundary (s.world.boundary, x, y, o.size) || overlaps (s, x, y, o.size))
                return false;
            return s.addObstacle (o) >= 0;
        };

        switch (layout)
        {
            case 1:
            {
                const int cols = juce::jlimit (2, 7, (int) std::sqrt ((float) count) + 1);
                const int rows = juce::jmax (2, count / cols);
                const float jitter = opt.chaos * 0.04f;
                for (int row = 0; row < rows; ++row)
                    for (int c = 0; c < cols; ++c)
                        tryAdd (0.15f + 0.7f * ((float) c + (row % 2 ? 0.5f : 0.0f)) / (float) cols + range (r, -jitter, jitter),
                                0.2f + 0.65f * (float) row / (float) juce::jmax (1, rows - 1) + range (r, -jitter, jitter));
                break;
            }
            case 2:
            {
                const int rings = 1 + r.nextInt (2);
                for (int ring = 0; ring < rings; ++ring)
                {
                    const int n = juce::jmax (3, count / rings);
                    const float rad = 0.18f + 0.14f * (float) ring;
                    const float off = range (r, 0.0f, kTwoPi);
                    for (int i = 0; i < n; ++i)
                    {
                        const float a = off + kTwoPi * (float) i / (float) n;
                        tryAdd (0.5f + std::cos (a) * rad, 0.5f + std::sin (a) * rad);
                    }
                }
                break;
            }
            case 3:
            {
                for (int i = 0, tries = 0; i < count / 2 && tries < 200; ++tries)
                {
                    const float x = range (r, 0.08f, 0.48f), y = range (r, 0.15f, 0.92f);
                    auto o = makeObstacle (x, y);
                    if (! insideBoundary (s.world.boundary, x, y, o.size) || overlaps (s, x, y, o.size)
                        || overlaps (s, 1.0f - x, y, o.size))
                        continue;
                    auto m = o;
                    m.x = 1.0f - x;
                    m.rotation = -o.rotation;
                    m.note = quantizeToScale (o.note + 7, s.world.scale, s.world.root);
                    s.addObstacle (o);
                    s.addObstacle (m);
                    ++i;
                }
                break;
            }
            case 4:
            {
                for (int i = 0; i < count; ++i)
                {
                    const float t = (float) i / (float) juce::jmax (1, count - 1);
                    tryAdd (0.12f + 0.76f * t + range (r, -0.03f, 0.03f),
                            0.2f + 0.7f * (r.nextBool() ? t : 1.0f - t) + range (r, -0.05f, 0.05f));
                }
                break;
            }
            default:
                for (int i = 0, tries = 0; i < count && tries < 400; ++tries)
                    if (tryAdd (range (r, 0.05f, 0.95f), range (r, 0.12f, 0.95f)))
                        ++i;
                break;
        }

        // never end up with an empty scene
        for (int tries = 0; s.numActiveObstacles() < 3 && tries < 200; ++tries)
            tryAdd (range (r, 0.1f, 0.9f), range (r, 0.25f, 0.9f));

        if (! assignEngines)
            for (auto& o : s.obstacles)
                o.engine = 0;
    }
}

namespace
{
    int countHits (const Scene& s, double seconds)
    {
        Simulation sim;
        sim.setScene (s);
        sim.reset();
        std::array<HitEvent, 128> hits;
        int total = 0;
        constexpr double sr = 24000.0;
        double beat = 0.0;
        for (int b = 0; b < (int) (seconds * sr / 1024.0) && total == 0; ++b)
        {
            total += sim.process (1024, sr, 120.0, beat, hits.data(), (int) hits.size());
            beat += 1024.0 * 2.0 / sr;
        }
        return total;
    }
}

void ensurePlayable (Scene& s)
{
    if (countHits (s, 10.0) > 0) return;

    for (int attempt = 0; attempt < kMaxEmitters; ++attempt)
    {
        auto& e = s.emitters[(size_t) attempt];
        if (! e.active) continue;

        // aim the emitter at its nearest obstacle
        float best = 1.0e9f;
        for (auto& o : s.obstacles)
        {
            if (! o.active || o.muted) continue;
            const float dx = o.x - e.x, dy = o.y - e.y, d = dx * dx + dy * dy;
            if (d < best) { best = d; e.angle = std::atan2 (dy, dx); }
        }
        e.speed = juce::jmax (e.speed, 0.25f);
        if (e.rate == Rate::Midi) e.rate = Rate::Quarter;
        if (countHits (s, 10.0) > 0) return;
    }
}

Scene randomScene (juce::uint32 seed, const RandomOptions& opt, const Scene& current)
{
    juce::Random r ((juce::int64) seed * 7919 + 13);
    Scene s = current;
    s.seed = seed;

    if (! opt.lockWorld)
        randomiseWorld (r, s.world, opt);
    if (! opt.lockSounds)
        randomiseSound (r, s.sound);

    if (! opt.lockEmitters)
        randomiseEmitters (r, s, opt);

    if (! opt.lockObstacles)
    {
        // remember engines when sounds are locked
        std::array<int, kMaxObstacles> engines {};
        for (size_t i = 0; i < engines.size(); ++i) engines[i] = current.obstacles[i].engine;

        randomiseObstacles (r, s, opt, true);

        if (opt.lockSounds)
        {
            int k = 0;
            const int prev = current.numActiveObstacles();
            for (auto& o : s.obstacles)
                if (o.active)
                    o.engine = prev > 0 ? engines[(size_t) (k++ % prev)] : 0;
        }
    }
    else if (! opt.lockSounds)
    {
        int palette[2] { r.nextInt (kNumEngines), r.nextInt (kNumEngines) };
        for (auto& o : s.obstacles)
            if (o.active) o.engine = palette[r.nextInt (2)];
    }

    // emitters must not be buried inside a locked obstacle field
    if (s.numActiveEmitters() == 0)
    {
        Emitter e;
        e.x = 0.5f; e.y = 0.1f; e.angle = kPi * 0.5f + 0.3f;
        s.addEmitter (e);
    }
    ensurePlayable (s);
    return s;
}

Scene mutateScene (const Scene& scene, juce::uint32 seed, float amount)
{
    juce::Random r ((juce::int64) seed * 104729 + 7);
    amount = juce::jlimit (0.0f, 1.0f, amount);
    Scene s = scene;
    s.seed = seed;

    for (auto& o : s.obstacles)
    {
        if (! o.active) continue;
        o.x = juce::jlimit (0.03f, 0.97f, o.x + range (r, -0.05f, 0.05f) * amount);
        o.y = juce::jlimit (0.03f, 0.97f, o.y + range (r, -0.05f, 0.05f) * amount);
        o.size = juce::jlimit (0.012f, 0.25f, o.size * (1.0f + range (r, -0.25f, 0.25f) * amount));
        o.rotation += range (r, -0.6f, 0.6f) * amount;
        if (r.nextFloat() < amount * 0.4f)
        {
            const int step = r.nextBool() ? 2 : -2;
            o.note = quantizeToScale (juce::jlimit (24, 108, o.note + step), s.world.scale, s.world.root);
        }
        if (r.nextFloat() < amount * 0.1f)
            o.engine = r.nextInt (kNumEngines);
    }

    for (auto& e : s.emitters)
    {
        if (! e.active) continue;
        e.angle += range (r, -0.35f, 0.35f) * amount;
        e.speed = juce::jlimit (0.03f, 2.5f, e.speed * (1.0f + range (r, -0.2f, 0.2f) * amount));
    }
    return s;
}

} // namespace ric
