#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace ric
{

constexpr int kMaxObstacles = 128;
constexpr int kMaxEmitters  = 16;
constexpr int kMaxBalls     = 64;
constexpr int kNumEngines   = 8;
constexpr float kPi         = 3.14159265358979f;
constexpr float kTwoPi      = 6.28318530717959f;

//==============================================================================
enum class Shape : int
{
    Circle = 0,
    Rect,
    Triangle,
    Diamond,
    Polygon,
    Star,
    Line,
    Arc,
    NumShapes
};

enum class Boundary : int { Box = 0, Circle, Hexagon, Open, NumBoundaries };

enum class PitchMode : int { Manual = 0, BySize, ByHeight, NumModes };

/** How often an emitter launches a ball. */
enum class Rate : int
{
    Once = 0,
    Midi,
    TwoBars,
    OneBar,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    QuarterTriplet,
    EighthTriplet,
    DottedQuarter,
    NumRates
};

enum class Quantize : int { Off = 0, Eighth, Sixteenth, ThirtySecond, NumQuantize };

//==============================================================================
struct Obstacle
{
    bool  active   = false;
    Shape shape    = Shape::Circle;
    float x        = 0.5f;   // normalised [0,1], y grows downward
    float y        = 0.5f;
    float size     = 0.06f;  // outer radius / half-extent
    float aspect   = 1.0f;   // Rect: width/height; Arc: span fraction of a full turn
    float rotation = 0.0f;   // radians
    float spin     = 0.0f;   // radians per second
    int   sides    = 5;      // Polygon sides / Star points
    int   note     = 60;     // MIDI note (Manual pitch mode)
    int   engine   = 0;      // sound engine index
    float bounce   = 1.0f;   // restitution
    float gain     = 1.0f;
    bool  muted    = false;
};

struct Emitter
{
    bool  active   = false;
    float x        = 0.5f;
    float y        = 0.1f;
    float angle    = kPi * 0.5f;  // radians, 0 = right, pi/2 = down
    float speed    = 0.6f;        // normalised units per second
    Rate  rate     = Rate::Quarter;
    int   maxBalls = 8;
    float life     = 8.0f;        // seconds, 0 = forever
    float spread   = 0.0f;        // random angle jitter (radians)
    float ballSize = 0.012f;
};

struct World
{
    Boundary  boundary       = Boundary::Box;
    float     gravity        = 0.0f;      // units / s^2
    float     gravityAngle   = kPi * 0.5f;
    float     friction       = 0.0f;      // velocity damping per second
    float     wallBounce     = 1.0f;
    bool      ballCollisions = false;
    PitchMode pitchMode      = PitchMode::Manual;
    int       scale          = 1;         // index into scaleNames()
    int       root           = 0;         // 0 = C
    int       octave         = 4;         // base octave for BySize / ByHeight
    int       range          = 3;         // octaves spanned by BySize / ByHeight
    Quantize  quantize       = Quantize::Off;
};

/** Sound / performance values. Live values come from the plug-in parameters;
    this struct only travels with presets and the offline renderer. */
struct SoundSettings
{
    float master     = 0.75f;
    float brightness = 0.6f;
    float decay      = 0.5f;
    float attack     = 0.0f;
    float noteLength = 0.25f;  // seconds
    float reverbMix  = 0.25f;
    float roomSize   = 0.65f;
    float delayMix   = 0.12f;
    int   delayDiv   = 2;      // index into delayDivisionNames()
    float feedback   = 0.35f;
    float drive      = 0.1f;
    float speed      = 1.0f;   // simulation time scale
};

/** Plain, fixed-size scene description. Safe to copy on the audio thread. */
struct Scene
{
    std::array<Obstacle, kMaxObstacles> obstacles {};
    std::array<Emitter,  kMaxEmitters>  emitters  {};
    World         world;
    SoundSettings sound;
    juce::uint32  seed = 1;

    int numActiveObstacles() const noexcept;
    int numActiveEmitters()  const noexcept;
    int addObstacle (const Obstacle&) noexcept;   // returns index or -1
    int addEmitter  (const Emitter&)  noexcept;
    void clear() noexcept;
};

struct NamedScene
{
    juce::String name;
    juce::String category;
    Scene        scene;
};

//==============================================================================
// Names for UI and serialisation
const juce::StringArray& shapeNames();
const juce::StringArray& boundaryNames();
const juce::StringArray& pitchModeNames();
const juce::StringArray& rateNames();
const juce::StringArray& quantizeNames();
const juce::StringArray& scaleNames();
const juce::StringArray& engineNames();
const juce::StringArray& delayDivisionNames();
const juce::StringArray& rootNames();

double rateBeats (Rate) noexcept;              // <= 0 means not clock driven
double delayDivisionBeats (int index) noexcept;
double quantizeBeats (Quantize) noexcept;      // <= 0 means off

/** Snap a MIDI note to the closest scale degree (ties resolve downward). */
int quantizeToScale (int note, int scaleIndex, int root) noexcept;
/** The n-th note of the scale counting from the root at the given octave. */
int scaleDegreeToNote (int degree, int scaleIndex, int root, int octave) noexcept;
int scaleLength (int scaleIndex) noexcept;
juce::String noteName (int midiNote);

//==============================================================================
juce::var    sceneToVar (const Scene&);
bool         sceneFromVar (const juce::var&, Scene&);
juce::String sceneToJson (const NamedScene&);
bool         sceneFromJson (const juce::String&, NamedScene&);

} // namespace ric
