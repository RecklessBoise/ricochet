#include "Scene.h"

namespace ric
{

//==============================================================================
int Scene::numActiveObstacles() const noexcept
{
    int n = 0;
    for (auto& o : obstacles) n += o.active ? 1 : 0;
    return n;
}

int Scene::numActiveEmitters() const noexcept
{
    int n = 0;
    for (auto& e : emitters) n += e.active ? 1 : 0;
    return n;
}

int Scene::addObstacle (const Obstacle& o) noexcept
{
    for (size_t i = 0; i < obstacles.size(); ++i)
        if (! obstacles[i].active)
        {
            obstacles[i] = o;
            obstacles[i].active = true;
            return (int) i;
        }
    return -1;
}

int Scene::addEmitter (const Emitter& e) noexcept
{
    for (size_t i = 0; i < emitters.size(); ++i)
        if (! emitters[i].active)
        {
            emitters[i] = e;
            emitters[i].active = true;
            return (int) i;
        }
    return -1;
}

void Scene::clear() noexcept
{
    for (auto& o : obstacles) o = Obstacle();
    for (auto& e : emitters)  e = Emitter();
}

//==============================================================================
const juce::StringArray& shapeNames()
{
    static const juce::StringArray s { "Circle", "Rectangle", "Triangle", "Diamond", "Polygon", "Star", "Line", "Arc" };
    return s;
}

const juce::StringArray& boundaryNames()
{
    static const juce::StringArray s { "Box", "Circle", "Hexagon", "Open" };
    return s;
}

const juce::StringArray& pitchModeNames()
{
    static const juce::StringArray s { "Per obstacle", "By size", "By height" };
    return s;
}

const juce::StringArray& rateNames()
{
    static const juce::StringArray s { "Once", "MIDI note", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16",
                                       "1/4 T", "1/8 T", "1/4 ." };
    return s;
}

const juce::StringArray& quantizeNames()
{
    static const juce::StringArray s { "Off", "1/8", "1/16", "1/32" };
    return s;
}

namespace
{
    struct ScaleDef { const char* name; std::array<int, 12> steps; int length; };

    const std::array<ScaleDef, 16>& scales()
    {
        static const std::array<ScaleDef, 16> s {{
            { "Chromatic",        { 0,1,2,3,4,5,6,7,8,9,10,11 }, 12 },
            { "Major",            { 0,2,4,5,7,9,11 },            7 },
            { "Minor",            { 0,2,3,5,7,8,10 },            7 },
            { "Dorian",           { 0,2,3,5,7,9,10 },            7 },
            { "Phrygian",         { 0,1,3,5,7,8,10 },            7 },
            { "Lydian",           { 0,2,4,6,7,9,11 },            7 },
            { "Mixolydian",       { 0,2,4,5,7,9,10 },            7 },
            { "Pentatonic major", { 0,2,4,7,9 },                 5 },
            { "Pentatonic minor", { 0,3,5,7,10 },                5 },
            { "Blues",            { 0,3,5,6,7,10 },              6 },
            { "Harmonic minor",   { 0,2,3,5,7,8,11 },            7 },
            { "Whole tone",       { 0,2,4,6,8,10 },              6 },
            { "Hirajoshi",        { 0,2,3,7,8 },                 5 },
            { "In sen",           { 0,1,5,7,10 },                5 },
            { "Double harmonic",  { 0,1,4,5,7,8,11 },            7 },
            { "Lydian dominant",  { 0,2,4,6,7,9,10 },            7 },
        }};
        return s;
    }

    const ScaleDef& scaleAt (int index) noexcept
    {
        return scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, index)];
    }
}

const juce::StringArray& scaleNames()
{
    static const juce::StringArray s = []
    {
        juce::StringArray a;
        for (auto& d : scales()) a.add (d.name);
        return a;
    }();
    return s;
}

const juce::StringArray& engineNames()
{
    static const juce::StringArray s { "Glass FM", "Pluck", "Mallet", "Analog", "Halo Pad", "Drums", "Wind", "Chip" };
    return s;
}

const juce::StringArray& delayDivisionNames()
{
    static const juce::StringArray s { "1/16", "1/8 T", "1/8", "1/8 .", "1/4", "1/4 .", "1/2" };
    return s;
}

const juce::StringArray& rootNames()
{
    static const juce::StringArray s { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return s;
}

double rateBeats (Rate r) noexcept
{
    switch (r)
    {
        case Rate::TwoBars:        return 8.0;
        case Rate::OneBar:         return 4.0;
        case Rate::Half:           return 2.0;
        case Rate::Quarter:        return 1.0;
        case Rate::Eighth:         return 0.5;
        case Rate::Sixteenth:      return 0.25;
        case Rate::QuarterTriplet: return 2.0 / 3.0;
        case Rate::EighthTriplet:  return 1.0 / 3.0;
        case Rate::DottedQuarter:  return 1.5;
        case Rate::Once:
        case Rate::Midi:
        case Rate::NumRates:
        default:                   return 0.0;
    }
}

double delayDivisionBeats (int index) noexcept
{
    static const double d[] { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
    return d[juce::jlimit (0, 6, index)];
}

double quantizeBeats (Quantize q) noexcept
{
    switch (q)
    {
        case Quantize::Eighth:       return 0.5;
        case Quantize::Sixteenth:    return 0.25;
        case Quantize::ThirtySecond: return 0.125;
        case Quantize::Off:
        case Quantize::NumQuantize:
        default:                     return 0.0;
    }
}

int scaleLength (int scaleIndex) noexcept
{
    return scaleAt (scaleIndex).length;
}

int quantizeToScale (int note, int scaleIndex, int root) noexcept
{
    const auto& sc = scaleAt (scaleIndex);
    note = juce::jlimit (0, 127, note);

    int best = note, bestDist = 1000;
    for (int candidate = note - 6; candidate <= note + 6; ++candidate)
    {
        const int pc = ((candidate - root) % 12 + 12) % 12;
        bool inScale = false;
        for (int i = 0; i < sc.length; ++i)
            inScale = inScale || sc.steps[(size_t) i] == pc;

        const int dist = std::abs (candidate - note);
        if (inScale && (dist < bestDist || (dist == bestDist && candidate < best)))
        {
            best = candidate;
            bestDist = dist;
        }
    }
    return juce::jlimit (0, 127, best);
}

int scaleDegreeToNote (int degree, int scaleIndex, int root, int octave) noexcept
{
    const auto& sc = scaleAt (scaleIndex);
    const int len = sc.length;
    const int oct = (degree >= 0) ? degree / len : -((-degree + len - 1) / len);
    const int idx = degree - oct * len;
    return juce::jlimit (0, 127, (octave + 1 + oct) * 12 + root + sc.steps[(size_t) idx]);
}

juce::String noteName (int midiNote)
{
    return rootNames()[midiNote % 12] + juce::String (midiNote / 12 - 1);
}

//==============================================================================
namespace
{
    template <typename E>
    E enumFrom (const juce::var& v, E fallback, int count)
    {
        if (v.isVoid()) return fallback;
        return (E) juce::jlimit (0, count - 1, (int) v);
    }

    float fl (const juce::var& v, float fallback)  { return v.isVoid() ? fallback : (float) (double) v; }
    int   in (const juce::var& v, int fallback)    { return v.isVoid() ? fallback : (int) v; }
    bool  bo (const juce::var& v, bool fallback)   { return v.isVoid() ? fallback : (bool) v; }
}

juce::var sceneToVar (const Scene& s)
{
    auto* root = new juce::DynamicObject();
    juce::var rootVar (root);
    root->setProperty ("version", 1);
    root->setProperty ("seed", (juce::int64) s.seed);

    {
        auto* w = new juce::DynamicObject();
        const auto& wd = s.world;
        w->setProperty ("boundary", (int) wd.boundary);
        w->setProperty ("gravity", wd.gravity);
        w->setProperty ("gravityAngle", wd.gravityAngle);
        w->setProperty ("friction", wd.friction);
        w->setProperty ("wallBounce", wd.wallBounce);
        w->setProperty ("ballCollisions", wd.ballCollisions);
        w->setProperty ("pitchMode", (int) wd.pitchMode);
        w->setProperty ("scale", wd.scale);
        w->setProperty ("root", wd.root);
        w->setProperty ("octave", wd.octave);
        w->setProperty ("range", wd.range);
        w->setProperty ("quantize", (int) wd.quantize);
        root->setProperty ("world", juce::var (w));
    }

    {
        auto* so = new juce::DynamicObject();
        const auto& sd = s.sound;
        so->setProperty ("master", sd.master);
        so->setProperty ("brightness", sd.brightness);
        so->setProperty ("decay", sd.decay);
        so->setProperty ("attack", sd.attack);
        so->setProperty ("noteLength", sd.noteLength);
        so->setProperty ("reverbMix", sd.reverbMix);
        so->setProperty ("roomSize", sd.roomSize);
        so->setProperty ("delayMix", sd.delayMix);
        so->setProperty ("delayDiv", sd.delayDiv);
        so->setProperty ("feedback", sd.feedback);
        so->setProperty ("drive", sd.drive);
        so->setProperty ("speed", sd.speed);
        root->setProperty ("sound", juce::var (so));
    }

    juce::Array<juce::var> obs;
    for (auto& o : s.obstacles)
    {
        if (! o.active) continue;
        auto* d = new juce::DynamicObject();
        d->setProperty ("shape", (int) o.shape);
        d->setProperty ("x", o.x);
        d->setProperty ("y", o.y);
        d->setProperty ("size", o.size);
        d->setProperty ("aspect", o.aspect);
        d->setProperty ("rotation", o.rotation);
        d->setProperty ("spin", o.spin);
        d->setProperty ("sides", o.sides);
        d->setProperty ("note", o.note);
        d->setProperty ("engine", o.engine);
        d->setProperty ("bounce", o.bounce);
        d->setProperty ("gain", o.gain);
        d->setProperty ("muted", o.muted);
        obs.add (juce::var (d));
    }
    root->setProperty ("obstacles", obs);

    juce::Array<juce::var> ems;
    for (auto& e : s.emitters)
    {
        if (! e.active) continue;
        auto* d = new juce::DynamicObject();
        d->setProperty ("x", e.x);
        d->setProperty ("y", e.y);
        d->setProperty ("angle", e.angle);
        d->setProperty ("speed", e.speed);
        d->setProperty ("rate", (int) e.rate);
        d->setProperty ("maxBalls", e.maxBalls);
        d->setProperty ("life", e.life);
        d->setProperty ("spread", e.spread);
        d->setProperty ("ballSize", e.ballSize);
        ems.add (juce::var (d));
    }
    root->setProperty ("emitters", ems);

    return rootVar;
}

bool sceneFromVar (const juce::var& v, Scene& s)
{
    if (! v.isObject())
        return false;

    Scene out;
    out.seed = (juce::uint32) (juce::int64) v.getProperty ("seed", 1);

    const auto w = v["world"];
    if (w.isObject())
    {
        auto& wd = out.world;
        wd.boundary       = enumFrom (w["boundary"], Boundary::Box, (int) Boundary::NumBoundaries);
        wd.gravity        = fl (w["gravity"], wd.gravity);
        wd.gravityAngle   = fl (w["gravityAngle"], wd.gravityAngle);
        wd.friction       = fl (w["friction"], wd.friction);
        wd.wallBounce     = fl (w["wallBounce"], wd.wallBounce);
        wd.ballCollisions = bo (w["ballCollisions"], wd.ballCollisions);
        wd.pitchMode      = enumFrom (w["pitchMode"], PitchMode::Manual, (int) PitchMode::NumModes);
        wd.scale          = juce::jlimit (0, scaleNames().size() - 1, in (w["scale"], wd.scale));
        wd.root           = juce::jlimit (0, 11, in (w["root"], wd.root));
        wd.octave         = juce::jlimit (0, 8, in (w["octave"], wd.octave));
        wd.range          = juce::jlimit (1, 6, in (w["range"], wd.range));
        wd.quantize       = enumFrom (w["quantize"], Quantize::Off, (int) Quantize::NumQuantize);
    }

    const auto so = v["sound"];
    if (so.isObject())
    {
        auto& sd = out.sound;
        sd.master     = fl (so["master"], sd.master);
        sd.brightness = fl (so["brightness"], sd.brightness);
        sd.decay      = fl (so["decay"], sd.decay);
        sd.attack     = fl (so["attack"], sd.attack);
        sd.noteLength = fl (so["noteLength"], sd.noteLength);
        sd.reverbMix  = fl (so["reverbMix"], sd.reverbMix);
        sd.roomSize   = fl (so["roomSize"], sd.roomSize);
        sd.delayMix   = fl (so["delayMix"], sd.delayMix);
        sd.delayDiv   = juce::jlimit (0, delayDivisionNames().size() - 1, in (so["delayDiv"], sd.delayDiv));
        sd.feedback   = fl (so["feedback"], sd.feedback);
        sd.drive      = fl (so["drive"], sd.drive);
        sd.speed      = fl (so["speed"], sd.speed);
    }

    if (auto* arr = v["obstacles"].getArray())
    {
        for (auto& d : *arr)
        {
            Obstacle o;
            o.shape    = enumFrom (d["shape"], Shape::Circle, (int) Shape::NumShapes);
            o.x        = juce::jlimit (0.0f, 1.0f, fl (d["x"], o.x));
            o.y        = juce::jlimit (0.0f, 1.0f, fl (d["y"], o.y));
            o.size     = juce::jlimit (0.01f, 0.5f, fl (d["size"], o.size));
            o.aspect   = juce::jlimit (0.05f, 20.0f, fl (d["aspect"], o.aspect));
            o.rotation = fl (d["rotation"], o.rotation);
            o.spin     = fl (d["spin"], o.spin);
            o.sides    = juce::jlimit (3, 12, in (d["sides"], o.sides));
            o.note     = juce::jlimit (0, 127, in (d["note"], o.note));
            o.engine   = juce::jlimit (0, kNumEngines - 1, in (d["engine"], o.engine));
            o.bounce   = juce::jlimit (0.0f, 2.0f, fl (d["bounce"], o.bounce));
            o.gain     = juce::jlimit (0.0f, 2.0f, fl (d["gain"], o.gain));
            o.muted    = bo (d["muted"], o.muted);
            if (out.addObstacle (o) < 0) break;
        }
    }

    if (auto* arr = v["emitters"].getArray())
    {
        for (auto& d : *arr)
        {
            Emitter e;
            e.x        = juce::jlimit (0.0f, 1.0f, fl (d["x"], e.x));
            e.y        = juce::jlimit (0.0f, 1.0f, fl (d["y"], e.y));
            e.angle    = fl (d["angle"], e.angle);
            e.speed    = juce::jlimit (0.0f, 4.0f, fl (d["speed"], e.speed));
            e.rate     = enumFrom (d["rate"], Rate::Quarter, (int) Rate::NumRates);
            e.maxBalls = juce::jlimit (1, kMaxBalls, in (d["maxBalls"], e.maxBalls));
            e.life     = juce::jlimit (0.0f, 120.0f, fl (d["life"], e.life));
            e.spread   = juce::jlimit (0.0f, kPi, fl (d["spread"], e.spread));
            e.ballSize = juce::jlimit (0.004f, 0.05f, fl (d["ballSize"], e.ballSize));
            if (out.addEmitter (e) < 0) break;
        }
    }

    s = out;
    return true;
}

juce::String sceneToJson (const NamedScene& ns)
{
    auto* root = new juce::DynamicObject();
    juce::var v (root);
    root->setProperty ("format", "ricochet");
    root->setProperty ("name", ns.name);
    root->setProperty ("category", ns.category);
    root->setProperty ("scene", sceneToVar (ns.scene));
    return juce::JSON::toString (v);
}

bool sceneFromJson (const juce::String& json, NamedScene& ns)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject())
        return false;

    NamedScene out;
    out.name     = v.getProperty ("name", "Untitled").toString();
    out.category = v.getProperty ("category", "User").toString();
    if (! sceneFromVar (v["scene"], out.scene))
        return false;

    ns = out;
    return true;
}

} // namespace ric
