#include "Presets.h"
#include "Randomizer.h"

namespace ric
{

namespace
{
    constexpr int kVariants = 12;

    const char* const kWords[kVariants] { "Dawn", "Ember", "Tide", "Neon", "Velvet", "Frost",
                                          "Amber", "Lunar", "Quartz", "Saffron", "Cobalt", "Halcyon" };

    // Musical scales cycled through the variants (indices into scaleNames()).
    const int kScaleCycle[kVariants] { 1, 2, 7, 3, 8, 5, 12, 6, 10, 14, 9, 13 };

    int cyc (std::initializer_list<int> items, int v)
    {
        return *(items.begin() + (size_t) (v % (int) items.size()));
    }

    int degNote (const Scene& s, int degree, int octave)
    {
        return scaleDegreeToNote (degree, s.world.scale, s.world.root, octave);
    }

    void baseVariant (Scene& s, int v)
    {
        s.world.scale = kScaleCycle[v % kVariants];
        s.world.root  = (v * 7) % 12;
        s.seed        = (juce::uint32) (1000 + v);
    }

    void soundFor (SoundSettings& s, int engine, int v)
    {
        s = SoundSettings();
        switch (engine)
        {
            case 0: s.brightness = 0.55f; s.decay = 0.55f; s.reverbMix = 0.35f; s.roomSize = 0.78f; s.delayMix = 0.18f; s.delayDiv = 2; break;
            case 1: s.brightness = 0.6f;  s.decay = 0.5f;  s.reverbMix = 0.25f; s.delayMix = 0.15f; s.delayDiv = 3; break;
            case 2: s.brightness = 0.5f;  s.decay = 0.5f;  s.reverbMix = 0.3f;  s.delayMix = 0.1f; break;
            case 3: s.brightness = 0.5f;  s.decay = 0.4f;  s.noteLength = 0.18f; s.reverbMix = 0.2f; s.delayMix = 0.25f; s.delayDiv = 3; s.feedback = 0.4f; s.drive = 0.2f; break;
            case 4: s.attack = 0.35f; s.noteLength = 1.4f; s.decay = 0.6f; s.reverbMix = 0.6f; s.roomSize = 0.9f; s.delayMix = 0.25f; s.delayDiv = 4; break;
            case 5: s.decay = 0.5f; s.reverbMix = 0.12f; s.roomSize = 0.4f; s.delayMix = 0.05f; s.drive = 0.25f; break;
            case 6: s.attack = 0.1f; s.noteLength = 0.6f; s.reverbMix = 0.5f; s.roomSize = 0.85f; s.delayMix = 0.2f; break;
            case 7: s.brightness = 0.5f; s.noteLength = 0.12f; s.reverbMix = 0.15f; s.delayMix = 0.25f; s.delayDiv = 1; break;
            default: break;
        }
        s.brightness = juce::jlimit (0.1f, 0.95f, s.brightness + (float) ((v % 3) - 1) * 0.1f);
    }

    Obstacle ob (Shape shape, float x, float y, float size, int note, int engine, float rotation = 0.0f)
    {
        Obstacle o;
        o.shape = shape; o.x = x; o.y = y; o.size = size;
        o.note = note; o.engine = engine; o.rotation = rotation;
        return o;
    }

    Emitter em (float x, float y, float angle, float speed, Rate rate, int maxBalls, float life,
                float spread = 0.0f, float ballSize = 0.012f)
    {
        Emitter e;
        e.x = x; e.y = y; e.angle = angle; e.speed = speed; e.rate = rate;
        e.maxBalls = maxBalls; e.life = life; e.spread = spread; e.ballSize = ballSize;
        return e;
    }

    bool clearOf (const Scene& s, float x, float y, float r)
    {
        for (auto& o : s.obstacles)
            if (o.active)
            {
                const float dx = o.x - x, dy = o.y - y, m = o.size + r + 0.03f;
                if (dx * dx + dy * dy < m * m) return false;
            }
        for (auto& e : s.emitters)
            if (e.active)
            {
                const float dx = e.x - x, dy = e.y - y, m = r + 0.07f;
                if (dx * dx + dy * dy < m * m) return false;
            }
        return true;
    }

    //==========================================================================
    NamedScene pegboard (int v)
    {
        NamedScene n { "Pegboard", "Cascades", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        const int eng = cyc ({ 2, 1, 0, 7 }, v);
        s.world.gravity = 0.8f + 0.1f * (float) (v % 3);
        s.world.wallBounce = 0.6f;

        const int rows = 5 + v % 3;
        for (int r = 0; r < rows; ++r)
        {
            const int count = 3 + r;
            const float y = 0.28f + 0.62f * (float) r / (float) (rows - 1);
            const float spacing = 0.78f / (float) (rows + 2);
            for (int i = 0; i < count; ++i)
            {
                const float x = 0.5f + ((float) i - (float) (count - 1) * 0.5f) * spacing;
                auto o = ob (v % 2 ? Shape::Circle : Shape::Polygon, x, y, 0.018f + 0.005f * (float) (v % 2),
                             degNote (s, i + (rows - r), 3), eng, kPi / 6.0f);
                o.sides = 6;
                o.bounce = 0.55f;
                s.addObstacle (o);
            }
        }
        s.addEmitter (em (0.5f, 0.06f, kPi * 0.5f, 0.12f, v % 3 == 0 ? Rate::Quarter : Rate::Eighth, 10, 7.0f, 0.3f));
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene orbit (int v)
    {
        NamedScene n { "Orbit", "Orbits", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.boundary = Boundary::Circle;
        const int eng = cyc ({ 0, 4, 2, 6 }, v);
        const Shape shapes[] { Shape::Circle, Shape::Triangle, Shape::Diamond, Shape::Star };
        const int count = 7 + v % 5;
        const float radius = 0.27f + 0.03f * (float) (v % 3);
        for (int i = 0; i < count; ++i)
        {
            const float a = kTwoPi * (float) i / (float) count;
            auto o = ob (shapes[(i + v) % 4], 0.5f + std::cos (a) * radius, 0.5f + std::sin (a) * radius,
                         0.035f, degNote (s, i, 4), eng, a + kPi * 0.5f);
            o.sides = 5;
            s.addObstacle (o);
        }
        if (v % 2)
            for (int i = 0; i < 3; ++i)
            {
                const float a = kTwoPi * (float) i / 3.0f + 0.5f;
                s.addObstacle (ob (Shape::Arc, 0.5f + std::cos (a) * 0.14f, 0.5f + std::sin (a) * 0.14f,
                                   0.04f, degNote (s, -i - 2, 3), eng == 4 ? 2 : 4, a + kPi * 0.5f));
            }
        // aim straight at one obstacle so the orbit always starts singing
        const float a0 = kTwoPi * (float) (v % count) / (float) count;
        s.addEmitter (em (0.5f, 0.5f, a0, 0.35f, Rate::Half, 4, 10.0f));
        s.addEmitter (em (0.5f, 0.5f, a0 + kPi * 0.9f, 0.3f, v % 3 == 0 ? Rate::QuarterTriplet : Rate::OneBar, 4, 10.0f));
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene grid (int v)
    {
        NamedScene n { "Grid", "Rhythmic", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        const int eng = cyc ({ 3, 7, 1, 2 }, v);
        s.world.quantize = v % 3 == 0 ? Quantize::Eighth : Quantize::Sixteenth;
        const int cols = 4 + v % 2, rows = 4;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                auto o = ob (v % 2 ? Shape::Diamond : Shape::Rect,
                             0.18f + (float) c * 0.64f / (float) (cols - 1),
                             0.22f + (float) r * 0.6f / (float) (rows - 1),
                             0.03f, degNote (s, c + (rows - 1 - r) * 2, 3), eng, kPi * 0.25f);
                o.aspect = 1.0f;
                s.addObstacle (o);
            }
        s.addEmitter (em (0.07f, 0.07f, 0.62f + (float) v * 0.05f, 0.55f, Rate::Once, 1, 0.0f));
        if (v % 2)
            s.addEmitter (em (0.93f, 0.5f, 2.6f, 0.45f, Rate::Once, 1, 0.0f));
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene spinner (int v)
    {
        NamedScene n { "Spinner", "Mechanical", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        const int eng = cyc ({ 3, 0, 7, 1 }, v);
        s.world.ballCollisions = (v % 2) == 1;

        auto star = ob (Shape::Star, 0.5f, 0.5f, 0.14f, degNote (s, 0, 2), eng);
        star.sides = 5 + v % 3;
        star.spin = 0.5f + 0.25f * (float) (v % 4);
        s.addObstacle (star);

        const float corners[4][2] { { 0.2f, 0.2f }, { 0.8f, 0.2f }, { 0.8f, 0.8f }, { 0.2f, 0.8f } };
        const int degrees[4] { 2, 4, 7, 9 };
        for (int i = 0; i < 4; ++i)
            s.addObstacle (ob (Shape::Circle, corners[i][0], corners[i][1], 0.05f, degNote (s, degrees[i], 4), eng));

        if (v % 2 == 0)
            for (int i = 0; i < 2; ++i)
            {
                auto a = ob (Shape::Arc, 0.5f, i == 0 ? 0.15f : 0.85f, 0.07f, degNote (s, 5 + i * 3, 4), eng == 0 ? 2 : 0);
                a.aspect = 0.35f;
                a.spin = i == 0 ? -0.8f : 0.8f;
                s.addObstacle (a);
            }

        s.addEmitter (em (0.06f, 0.5f, 0.1f, 0.5f, Rate::Half, 5, 8.0f));
        s.addEmitter (em (0.94f, 0.5f, kPi + 0.1f, 0.5f, v % 3 == 0 ? Rate::QuarterTriplet : Rate::OneBar, 5, 8.0f));
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene rain (int v)
    {
        NamedScene n { "Rain", "Ambient", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.boundary = Boundary::Open;
        s.world.gravity = 0.45f + 0.05f * (float) (v % 3);
        const int eng = cyc ({ 6, 0, 4, 2 }, v);
        juce::Random r ((juce::int64) v * 31 + 5);
        const int count = 6 + v % 4;
        for (int i = 0; i < count; ++i)
        {
            auto o = ob (Shape::Line, 0.12f + (float) i * 0.76f / (float) (count - 1), 0.45f + 0.4f * r.nextFloat(),
                         0.06f, degNote (s, (i * 2) % (scaleLength (s.world.scale) * 2), 4), eng,
                         (r.nextFloat() - 0.5f) * 1.0f);
            o.bounce = 0.7f;
            s.addObstacle (o);
        }
        for (int i = 0; i < 3; ++i)
            s.addObstacle (ob (Shape::Circle, 0.25f + 0.25f * (float) i, 0.3f + 0.04f * (float) (i % 2), 0.022f,
                               degNote (s, 7 + i, 4), eng == 0 ? 2 : 0));
        const Rate rates[] { Rate::Eighth, Rate::QuarterTriplet, Rate::Quarter };
        for (int i = 0; i < 3; ++i)
            s.addEmitter (em (0.2f + 0.3f * (float) i, 0.05f, kPi * 0.5f, 0.03f, rates[(i + v) % 3], 6, 6.0f, 0.6f, 0.01f));
        soundFor (s.sound, eng, v);
        s.sound.reverbMix = juce::jmax (s.sound.reverbMix, 0.45f);
        return n;
    }

    NamedScene drums (int v)
    {
        NamedScene n { "Drum Box", "Percussion", {} };
        auto& s = n.scene;
        s.seed = (juce::uint32) (2000 + v);
        s.world.scale = 0;
        s.world.root = 0;
        s.world.quantize = Quantize::Sixteenth;
        constexpr int D = 5;
        const float sh = 0.02f * (float) ((v % 5) - 2);

        s.addObstacle (ob (Shape::Circle, 0.3f + sh, 0.85f, 0.06f, 36, D));
        s.addObstacle (ob (Shape::Circle, 0.7f - sh, 0.85f, 0.06f, 36, D));
        auto sn1 = ob (Shape::Rect, 0.1f, 0.5f + sh, 0.07f, 40, D); sn1.aspect = 0.3f; s.addObstacle (sn1);
        auto sn2 = ob (Shape::Rect, 0.9f, 0.5f - sh, 0.07f, 40, D); sn2.aspect = 0.3f; s.addObstacle (sn2);
        for (int i = 0; i < 4; ++i)
            s.addObstacle (ob (Shape::Circle, 0.2f + 0.2f * (float) i, 0.14f, 0.025f, i == 3 && v % 2 ? 47 : 45, D));
        s.addObstacle (ob (Shape::Diamond, 0.3f, 0.55f, 0.035f, 54, D));
        s.addObstacle (ob (Shape::Diamond, 0.7f, 0.55f, 0.035f, 55, D));

        const int bassEngine = v % 2 ? 3 : 1;
        const int bassNotes[] { 0, 3, 5, 7, 10 };
        s.addObstacle (ob (Shape::Polygon, 0.5f, 0.68f, 0.045f, 36 + bassNotes[v % 5], bassEngine));

        s.addEmitter (em (0.5f, 0.35f, 0.9f + (float) v * 0.13f, 0.6f, Rate::Once, 1, 0.0f));
        if (v % 2)
            s.addEmitter (em (0.25f, 0.35f, -0.7f, 0.5f, Rate::Once, 1, 0.0f));
        if (v % 3 == 2)
            s.addEmitter (em (0.75f, 0.33f, kPi * 0.6f, 0.4f, Rate::OneBar, 2, 4.0f));
        soundFor (s.sound, D, v);
        return n;
    }

    NamedScene pinball (int v)
    {
        NamedScene n { "Pinball", "Chaos", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.boundary = Boundary::Hexagon;
        s.world.gravity = 0.35f;
        s.world.wallBounce = 0.9f;
        s.world.ballCollisions = true;
        const int palettes[4][2] { { 0, 3 }, { 1, 7 }, { 2, 5 }, { 4, 6 } };
        const auto& pal = palettes[v % 4];
        s.addEmitter (em (0.5f, 0.14f, kPi * 0.5f + 0.4f, 0.5f, Rate::Half, 6, 9.0f));
        s.addEmitter (em (0.2f, 0.4f, -0.3f, 0.6f, Rate::OneBar, 3, 9.0f));

        juce::Random r ((juce::int64) v * 97 + 1);
        const Shape shapes[] { Shape::Circle, Shape::Triangle, Shape::Star, Shape::Rect, Shape::Polygon, Shape::Arc };
        int placed = 0;
        for (int tries = 0; placed < 11 && tries < 300; ++tries)
        {
            const float a = r.nextFloat() * kTwoPi, rad = 0.36f * std::sqrt (r.nextFloat());
            const float x = 0.5f + std::cos (a) * rad, y = 0.5f + std::sin (a) * rad;
            const float size = 0.025f + 0.03f * r.nextFloat();
            if (! clearOf (s, x, y, size)) continue;
            auto o = ob (shapes[r.nextInt (6)], x, y, size, degNote (s, r.nextInt (12), 3), pal[r.nextInt (2)], r.nextFloat() * kTwoPi);
            o.sides = 5 + r.nextInt (3);
            o.aspect = o.shape == Shape::Arc ? 0.4f : 0.5f + r.nextFloat();
            o.bounce = 1.0f + 0.15f * r.nextFloat();
            if (r.nextFloat() < 0.25f) o.spin = (r.nextFloat() - 0.5f) * 2.0f;
            s.addObstacle (o);
            ++placed;
        }
        soundFor (s.sound, pal[0], v);
        return n;
    }

    NamedScene minimal (int v)
    {
        NamedScene n { "Minimal", "Minimal", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        const int eng = cyc ({ 0, 1, 2, 3, 4, 6, 7 }, v);
        juce::Random r ((juce::int64) v * 13 + 3);
        s.addEmitter (em (0.5f, 0.12f, 0.37f + (float) v * 0.21f, 0.4f, Rate::Once, 1, 0.0f, 0.0f, 0.014f));
        const int count = 2 + v % 3;
        const Shape shapes[] { Shape::Circle, Shape::Rect, Shape::Triangle, Shape::Line };
        int placed = 0;
        for (int tries = 0; placed < count && tries < 200; ++tries)
        {
            const float x = 0.2f + 0.6f * r.nextFloat(), y = 0.3f + 0.55f * r.nextFloat();
            const float size = 0.05f + 0.05f * r.nextFloat();
            if (! clearOf (s, x, y, size)) continue;
            auto o = ob (shapes[(placed + v) % 4], x, y, size, degNote (s, placed * 2, 4), eng, r.nextFloat() * kPi);
            o.aspect = 0.5f;
            s.addObstacle (o);
            ++placed;
        }
        soundFor (s.sound, eng, v);
        s.sound.delayMix = 0.3f;
        s.sound.feedback = 0.5f;
        return n;
    }

    NamedScene spiral (int v)
    {
        NamedScene n { "Spiral", "Melodic", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.boundary = Boundary::Circle;
        s.world.pitchMode = PitchMode::BySize;
        s.world.range = 3;
        s.world.octave = 3;
        const int eng = cyc ({ 2, 0, 1, 7 }, v);
        s.addEmitter (em (0.5f, 0.07f, kPi * 0.5f + 0.2f, 0.4f, Rate::Quarter, 5, 8.0f));
        const int count = 14 + v % 6;
        for (int i = 0; i < count; ++i)
        {
            const float t = (float) i / (float) count;
            const float rad = 0.05f + 0.33f * std::sqrt (t);
            const float a = (float) i * 2.39996f + (float) v;
            const float x = 0.5f + std::cos (a) * rad, y = 0.5f + std::sin (a) * rad;
            const float size = 0.052f - 0.032f * t;
            if (! clearOf (s, x, y, size * 0.6f)) continue;
            auto o = ob (i % 3 == 0 ? Shape::Polygon : Shape::Circle, x, y, size, 60, eng, a);
            o.sides = 6;
            s.addObstacle (o);
        }
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene pads (int v)
    {
        NamedScene n { "Slow Bloom", "Ambient", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        const int eng = v % 3 == 2 ? 6 : 4;
        juce::Random r ((juce::int64) v * 71 + 9);
        s.addEmitter (em (0.1f, 0.1f, 0.6f + 0.1f * (float) v, 0.15f, v % 2 ? Rate::TwoBars : Rate::OneBar, 4, 20.0f));
        s.addEmitter (em (0.9f, 0.9f, kPi + 0.5f, 0.12f, Rate::TwoBars, 3, 20.0f));
        const int count = 3 + v % 3;
        int placed = 0;
        for (int tries = 0; placed < count && tries < 200; ++tries)
        {
            const float x = 0.2f + 0.6f * r.nextFloat(), y = 0.2f + 0.6f * r.nextFloat();
            const float size = 0.08f + 0.05f * r.nextFloat();
            if (! clearOf (s, x, y, size)) continue;
            auto o = ob (placed % 2 ? Shape::Arc : Shape::Circle, x, y, size, degNote (s, placed * 2 + (v % 2), 3), eng, r.nextFloat() * kTwoPi);
            o.aspect = 0.45f;
            o.spin = placed % 2 ? 0.15f : 0.0f;
            s.addObstacle (o);
            ++placed;
        }
        // first emitter aims at the first bloom
        s.emitters[0].angle = std::atan2 (s.obstacles[0].y - s.emitters[0].y, s.obstacles[0].x - s.emitters[0].x);
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene ladder (int v)
    {
        NamedScene n { "Ladder", "Melodic", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.gravity = 0.9f;
        s.world.wallBounce = 0.5f;
        s.world.boundary = v % 2 ? Boundary::Open : Boundary::Box;
        const int eng = cyc ({ 1, 2, 7, 0 }, v);
        const bool descending = (v / 2) % 2 == 1;
        for (int i = 0; i < 8; ++i)
        {
            auto o = ob (Shape::Line, 0.14f + (float) i * 0.1f, 0.22f + (float) i * 0.085f, 0.045f,
                         degNote (s, descending ? 7 - i : i, 4), eng, 0.35f);
            o.bounce = 0.6f;
            s.addObstacle (o);
        }
        const Rate rates[] { Rate::Half, Rate::Quarter, Rate::OneBar };
        s.addEmitter (em (0.12f, 0.06f, kPi * 0.5f, 0.05f, rates[v % 3], 6, 8.0f));
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene hexbells (int v)
    {
        NamedScene n { "Hex Bells", "Melodic", {} };
        auto& s = n.scene;
        baseVariant (s, v);
        s.world.boundary = Boundary::Hexagon;
        s.world.pitchMode = PitchMode::ByHeight;
        s.world.range = 2;
        const int eng = cyc ({ 0, 2, 4, 1 }, v);
        for (int k = 0; k < 6; ++k)
        {
            const float a = kTwoPi * (float) k / 6.0f + kPi / 6.0f;
            s.addObstacle (ob (Shape::Triangle, 0.5f + std::cos (a) * 0.3f, 0.5f + std::sin (a) * 0.3f, 0.045f, 60, eng, a + kPi * 0.5f));
        }
        auto c = ob (Shape::Polygon, 0.5f, 0.5f, 0.07f, 60, eng);
        c.sides = 6;
        c.spin = v % 2 ? 0.3f : 0.0f;
        s.addObstacle (c);
        for (int i = 0; i < 3; ++i)
        {
            const float a = kTwoPi * (float) i / 3.0f + 0.2f * (float) v;
            s.addEmitter (em (0.5f + std::cos (a) * 0.17f, 0.5f + std::sin (a) * 0.17f, a + 0.9f, 0.35f, Rate::Once, 1, 0.0f));
        }
        soundFor (s.sound, eng, v);
        return n;
    }

    NamedScene dice (int v)
    {
        RandomOptions opt;
        opt.density = 0.3f + 0.05f * (float) (v % 10);
        opt.chaos = 0.2f + 0.06f * (float) (v % 12);
        NamedScene n;
        n.name = "Dice #" + juce::String (v + 1).paddedLeft ('0', 2);
        n.category = "Random";
        n.scene = randomScene ((juce::uint32) (777 + v * 7), opt, Scene());
        return n;
    }

    std::vector<NamedScene> build()
    {
        using Gen = NamedScene (*) (int);
        const Gen families[] { pegboard, orbit, grid, spinner, rain, drums, pinball, minimal, spiral, pads, ladder, hexbells };

        std::vector<NamedScene> all;
        for (auto gen : families)
            for (int v = 0; v < kVariants; ++v)
            {
                auto p = gen (v);
                p.name = p.name + " " + kWords[v];
                all.push_back (std::move (p));
            }
        for (int v = 0; v < 24; ++v)
            all.push_back (dice (v));
        return all;
    }
}

const std::vector<NamedScene>& factoryPresets()
{
    static const std::vector<NamedScene> presets = build();
    return presets;
}

juce::StringArray factoryCategories()
{
    juce::StringArray cats;
    for (auto& p : factoryPresets())
        cats.addIfNotAlreadyThere (p.category);
    return cats;
}

NamedScene defaultScene()
{
    return factoryPresets().front();
}

juce::File userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("Ricochet").getChildFile ("Presets");
}

std::vector<NamedScene> loadUserPresets()
{
    std::vector<NamedScene> out;
    const auto folder = userPresetFolder();
    if (! folder.isDirectory()) return out;

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.ricochet");
    files.sort();
    for (auto& f : files)
    {
        NamedScene ns;
        if (sceneFromJson (f.loadFileAsString(), ns))
        {
            ns.category = "User";
            if (ns.name.isEmpty()) ns.name = f.getFileNameWithoutExtension();
            out.push_back (std::move (ns));
        }
    }
    return out;
}

bool saveUserPreset (const NamedScene& ns, juce::File* writtenFile)
{
    const auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (juce::File::createLegalFileName (ns.name.isEmpty() ? "Untitled" : ns.name) + ".ricochet");
    NamedScene copy = ns;
    copy.category = "User";
    if (writtenFile != nullptr) *writtenFile = file;
    return file.replaceWithText (sceneToJson (copy));
}

} // namespace ric
