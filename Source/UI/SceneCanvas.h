#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

/** What is selected in the scene, shared by the canvas and the inspector. */
struct Selection : public juce::ChangeBroadcaster
{
    enum class Kind { None, Obstacle, Emitter };
    Kind kind = Kind::None;
    int index = -1;

    void select (Kind k, int i)
    {
        if (k != kind || i != index)
        {
            kind = k;
            index = i;
            sendChangeMessage();
        }
    }
    void clear() { select (Kind::None, -1); }
    bool isObstacle() const noexcept { return kind == Kind::Obstacle && index >= 0; }
    bool isEmitter() const noexcept  { return kind == Kind::Emitter && index >= 0; }
};

enum class Tool { Select = 0, Circle, Rect, Triangle, Diamond, Polygon, Star, Line, Arc, Emitter, NumTools };

/** Step a note to the next scale degree up (dir > 0) or down. */
int stepNoteInScale (int note, int dir, const ric::World& w);

//==============================================================================
class SceneCanvas : public juce::Component,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    SceneCanvas (RicochetProcessor&, Selection&);
    ~SceneCanvas() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void setTool (Tool t);
    Tool getTool() const noexcept { return tool; }

    void deleteSelection();
    void duplicateSelection();

    /** The colour of the most recent notes, used by other panels for ambience. */
    juce::Colour getAmbientColour() const noexcept { return ambient; }

private:
    class ToolButton;

    enum class Drag { None, Move, Resize, Rotate, Aim, MoveEmitter };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::Point<float> toScreen (float x, float y) const noexcept;
    juce::Point<float> toWorld (juce::Point<float> screen) const noexcept;
    float worldToScreen (float d) const noexcept { return d * area.getWidth(); }

    juce::Path obstaclePath (int index, float rotation) const;
    int hitObstacle (juce::Point<float> world) const;
    int hitEmitter (juce::Point<float> world) const;
    juce::Point<float> resizeHandle (const ric::Obstacle&) const;
    juce::Point<float> rotateHandle (const ric::Obstacle&, float rotation) const;
    juce::Point<float> aimHandle (const ric::Emitter&) const;

    void addAt (juce::Point<float> world, Tool t);
    void showContextMenu (juce::Point<float> world);
    void editScene (const std::function<void (ric::Scene&)>& fn, bool undo);

    void drawBackground (juce::Graphics&);
    void drawBoundary (juce::Graphics&);
    void drawObstacles (juce::Graphics&);
    void drawEmitters (juce::Graphics&);
    void drawBalls (juce::Graphics&);
    void drawSelection (juce::Graphics&);
    void drawHud (juce::Graphics&);

    RicochetProcessor& processor;
    Selection& selection;
    Tool tool = Tool::Select;
    juce::OwnedArray<ToolButton> toolButtons;

    juce::Rectangle<float> area;
    LiveSnapshot live;
    std::array<UiHit, 512> hitScratch {};
    std::array<float, ric::kMaxObstacles> flash {};
    std::array<int, ric::kMaxObstacles> flashNote {};

    struct Ripple { float x, y, age, strength; juce::Colour colour; };
    std::vector<Ripple> ripples;

    struct Trail
    {
        int id = 0;
        int count = 0, head = 0;
        int lastFrame = -1;
        int note = -1;
        std::array<juce::Point<float>, 18> pts {};
    };
    std::array<Trail, ric::kMaxBalls> trails {};
    int frame = 0;

    juce::Colour ambient { Theme::accent };
    float ambientEnergy = 0.0f;

    Drag drag = Drag::None;
    juce::Point<float> dragOffset;
    float rotateOffset = 0.0f;
    bool dragStartedEdit = false;
    int lastEngine = 2;
    juce::Point<float> hoverWorld { -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneCanvas)
};
