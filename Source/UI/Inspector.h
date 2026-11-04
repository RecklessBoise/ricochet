#pragma once

#include "SceneCanvas.h"

/** A label + horizontal slider row. */
class SliderRow : public juce::Component
{
public:
    SliderRow (const juce::String& name, double min, double max, double step = 0.0);
    void resized() override;

    juce::Slider slider;
    juce::Label label;
};

/** A label + combo box row. */
class ComboRow : public juce::Component
{
public:
    ComboRow (const juce::String& name, const juce::StringArray& items);
    void resized() override;

    juce::ComboBox box;
    juce::Label label;
};

//==============================================================================
class ObjectPage : public juce::Component
{
public:
    ObjectPage (RicochetProcessor&, Selection&, SceneCanvas&);
    void refresh();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void editObstacle (const std::function<void (ric::Obstacle&)>&, bool undo);
    void editEmitter (const std::function<void (ric::Emitter&)>&, bool undo);
    void wire (SliderRow&, std::function<void (double)> obstacleFn, std::function<void (double)> emitterFn);
    void layoutRows (const juce::Array<juce::Component*>& rows, juce::Rectangle<int> area);

    RicochetProcessor& processor;
    Selection& selection;
    SceneCanvas& canvas;

    juce::Label title, hint;
    ComboRow shape { "Shape", ric::shapeNames() };
    ComboRow engine { "Sound", ric::engineNames() };
    SliderRow note { "Note", 12, 120, 1 };
    SliderRow size { "Size", 0.012, 0.35 };
    SliderRow aspect { "Stretch", 0.1, 6.0 };
    SliderRow sides { "Sides", 3, 12, 1 };
    SliderRow rotation { "Rotation", -180, 180, 1 };
    SliderRow spin { "Spin", -3.0, 3.0 };
    SliderRow bounce { "Bounce", 0.0, 1.5 };
    SliderRow gain { "Volume", 0.0, 1.5 };
    juce::ToggleButton mute { "Mute" };

    ComboRow rate { "Rate", ric::rateNames() };
    SliderRow angle { "Direction", -180, 180, 1 };
    SliderRow speed { "Speed", 0.02, 3.0 };
    SliderRow maxBalls { "Max balls", 1, 32, 1 };
    SliderRow life { "Life (s)", 0.0, 30.0, 0.5 };
    SliderRow spread { "Spread", 0.0, 90.0, 1 };
    SliderRow ballSize { "Ball size", 0.004, 0.04 };

    juce::TextButton preview { "Preview" }, fire { "Fire" }, duplicate { "Duplicate" }, remove { "Delete" };
};

//==============================================================================
class WorldPage : public juce::Component
{
public:
    explicit WorldPage (RicochetProcessor&);
    void refresh();
    void resized() override;

private:
    void edit (const std::function<void (ric::World&)>&, bool undo);

    RicochetProcessor& processor;
    ComboRow boundary { "Space", ric::boundaryNames() };
    SliderRow gravity { "Gravity", 0.0, 3.0 };
    SliderRow gravityAngle { "Gravity dir", 0, 360, 1 };
    SliderRow friction { "Friction", 0.0, 1.0 };
    SliderRow wallBounce { "Wall bounce", 0.0, 1.2 };
    juce::ToggleButton collisions { "Balls collide" };
    ComboRow pitchMode { "Pitch from", ric::pitchModeNames() };
    ComboRow scale { "Scale", ric::scaleNames() };
    ComboRow root { "Root", ric::rootNames() };
    SliderRow octave { "Octave", 1, 7, 1 };
    SliderRow range { "Range (oct)", 1, 5, 1 };
    ComboRow quantize { "Quantize", ric::quantizeNames() };
    juce::TextButton clearAll { "Clear scene" };
};

//==============================================================================
class SoundPage : public juce::Component
{
public:
    explicit SoundPage (RicochetProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    RicochetProcessor& processor;
    juce::OwnedArray<Knob> knobs;
    juce::ComboBox delayDiv;
    juce::Label delayDivLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> delayDivAttachment;
    juce::ToggleButton sync { "Sync to host transport" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
};

//==============================================================================
class RandomPage : public juce::Component, private juce::Timer
{
public:
    explicit RandomPage (RicochetProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void push();

    RicochetProcessor& processor;
    SliderRow density { "Density", 0.0, 1.0 };
    SliderRow chaos { "Chaos", 0.0, 1.0 };
    SliderRow mutateAmount { "Mutation", 0.05, 1.0 };
    juce::ToggleButton lockObstacles { "Keep obstacles" }, lockEmitters { "Keep emitters" },
                       lockSounds { "Keep sounds" }, lockWorld { "Keep world" }, keepScale { "Keep scale" };
    juce::TextButton randomize { "Randomize" }, mutate { "Mutate" };
    juce::Label seed;
};

//==============================================================================
class Inspector : public juce::Component,
                  private juce::ChangeListener
{
public:
    Inspector (RicochetProcessor&, Selection&, SceneCanvas&);
    ~Inspector() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    RicochetProcessor& processor;
    Selection& selection;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    ObjectPage* objectPage = nullptr;
    WorldPage* worldPage = nullptr;
};
