#include "Inspector.h"

using namespace ric;

namespace
{
    constexpr int kRow = 28;
    constexpr float kRadToDeg = 57.29578f;

    void styleLabel (juce::Label& l, bool dim = true)
    {
        l.setFont (Theme::font (12.0f));
        l.setColour (juce::Label::textColourId, dim ? Theme::textDim : Theme::text);
        l.setJustificationType (juce::Justification::centredLeft);
    }
}

//==============================================================================
SliderRow::SliderRow (const juce::String& name, double min, double max, double step)
{
    label.setText (name, juce::dontSendNotification);
    styleLabel (label);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
    slider.setRange (min, max, step);
    if (step == 0.0) slider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (label);
    addAndMakeVisible (slider);
}

void SliderRow::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (86));
    slider.setBounds (r);
}

ComboRow::ComboRow (const juce::String& name, const juce::StringArray& items)
{
    label.setText (name, juce::dontSendNotification);
    styleLabel (label);
    box.addItemList (items, 1);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void ComboRow::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (86));
    box.setBounds (r.reduced (0, 2));
}

//==============================================================================
ObjectPage::ObjectPage (RicochetProcessor& p, Selection& s, SceneCanvas& c)
    : processor (p), selection (s), canvas (c)
{
    title.setFont (Theme::font (16.0f, true));
    title.setColour (juce::Label::textColourId, Theme::text);
    hint.setFont (Theme::font (13.0f));
    hint.setColour (juce::Label::textColourId, Theme::textDim);
    hint.setJustificationType (juce::Justification::topLeft);
    hint.setText ("Pick a shape in the left toolbar and click in the space to drop an obstacle. "
                  "Use the arrow tool (E) to add ball emitters.\n\n"
                  "Drag objects to move them, the square handle resizes, the round handle rotates. "
                  "Drag an emitter's arrow to aim it - its length is the launch speed.\n\n"
                  "Mouse wheel over an obstacle changes its note, Shift + wheel its size. "
                  "Double-click to preview, right-click for more.", juce::dontSendNotification);
    addAndMakeVisible (title);
    addAndMakeVisible (hint);

    for (auto* comp : std::initializer_list<juce::Component*> { &shape, &engine, &note, &size, &aspect, &sides, &rotation,
                                                                &spin, &bounce, &gain, &mute, &rate, &angle, &speed,
                                                                &maxBalls, &life, &spread, &ballSize, &preview, &fire,
                                                                &duplicate, &remove })
        addChildComponent (comp);

    note.slider.textFromValueFunction = [] (double v) { return noteName ((int) v); };
    note.slider.valueFromTextFunction = [] (const juce::String& t) { return (double) t.getIntValue(); };
    life.slider.textFromValueFunction = [] (double v) { return v <= 0.0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : juce::String (v, 1) + " s"; };

    shape.box.onChange = [this]
    {
        const int i = shape.box.getSelectedItemIndex();
        editObstacle ([i] (Obstacle& o) { o.shape = (Shape) i; }, true);
        refresh();
    };
    engine.box.onChange = [this]
    {
        const int i = engine.box.getSelectedItemIndex();
        editObstacle ([i] (Obstacle& o) { o.engine = i; }, true);
        if (selection.isObstacle())
        {
            const auto& sc = processor.getScene();
            const auto& o = sc.obstacles[(size_t) selection.index];
            processor.previewNote (o.engine, effectiveNote (sc.world, o));
        }
    };
    rate.box.onChange = [this]
    {
        const int i = rate.box.getSelectedItemIndex();
        editEmitter ([i] (Emitter& e) { e.rate = (Rate) i; }, true);
    };
    mute.onClick = [this] { const bool m = mute.getToggleState(); editObstacle ([m] (Obstacle& o) { o.muted = m; }, true); };

    wire (note,     [this] (double v) { editObstacle ([v] (Obstacle& o) { o.note = (int) v; }, false); }, nullptr);
    wire (size,     [this] (double v) { editObstacle ([v] (Obstacle& o) { o.size = (float) v; }, false); }, nullptr);
    wire (aspect,   [this] (double v) { editObstacle ([v] (Obstacle& o) { o.aspect = (float) v; }, false); }, nullptr);
    wire (sides,    [this] (double v) { editObstacle ([v] (Obstacle& o) { o.sides = (int) v; }, false); }, nullptr);
    wire (rotation, [this] (double v) { editObstacle ([v] (Obstacle& o) { o.rotation = (float) v / kRadToDeg; }, false); }, nullptr);
    wire (spin,     [this] (double v) { editObstacle ([v] (Obstacle& o) { o.spin = (float) v; }, false); }, nullptr);
    wire (bounce,   [this] (double v) { editObstacle ([v] (Obstacle& o) { o.bounce = (float) v; }, false); }, nullptr);
    wire (gain,     [this] (double v) { editObstacle ([v] (Obstacle& o) { o.gain = (float) v; }, false); }, nullptr);
    wire (angle,    nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.angle = (float) v / kRadToDeg; }, false); });
    wire (speed,    nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.speed = (float) v; }, false); });
    wire (maxBalls, nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.maxBalls = (int) v; }, false); });
    wire (life,     nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.life = (float) v; }, false); });
    wire (spread,   nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.spread = (float) v / kRadToDeg; }, false); });
    wire (ballSize, nullptr, [this] (double v) { editEmitter ([v] (Emitter& e) { e.ballSize = (float) v; }, false); });

    note.slider.onDragEnd = [this]
    {
        if (! selection.isObstacle()) return;
        const auto& sc = processor.getScene();
        const auto& o = sc.obstacles[(size_t) selection.index];
        processor.previewNote (o.engine, effectiveNote (sc.world, o));
    };

    preview.onClick = [this]
    {
        if (! selection.isObstacle()) return;
        const auto& sc = processor.getScene();
        const auto& o = sc.obstacles[(size_t) selection.index];
        processor.previewNote (o.engine, effectiveNote (sc.world, o));
    };
    fire.onClick = [this] { if (selection.isEmitter()) processor.fireEmitter (selection.index); };
    duplicate.onClick = [this] { canvas.duplicateSelection(); };
    remove.onClick = [this] { canvas.deleteSelection(); };
    remove.setColour (juce::TextButton::buttonColourId, Theme::rec.withAlpha (0.35f));

    refresh();
}

void ObjectPage::wire (SliderRow& row, std::function<void (double)> obstacleFn, std::function<void (double)> emitterFn)
{
    row.slider.onDragStart = [this] { processor.pushUndo(); };
    row.slider.onValueChange = [this, &row, obstacleFn, emitterFn]
    {
        const double v = row.slider.getValue();
        if (selection.isObstacle() && obstacleFn) obstacleFn (v);
        else if (selection.isEmitter() && emitterFn) emitterFn (v);
    };
}

void ObjectPage::editObstacle (const std::function<void (Obstacle&)>& fn, bool undo)
{
    if (! selection.isObstacle()) return;
    auto s = processor.getScene();
    fn (s.obstacles[(size_t) selection.index]);
    processor.setScene (s, undo);
}

void ObjectPage::editEmitter (const std::function<void (Emitter&)>& fn, bool undo)
{
    if (! selection.isEmitter()) return;
    auto s = processor.getScene();
    fn (s.emitters[(size_t) selection.index]);
    processor.setScene (s, undo);
}

void ObjectPage::refresh()
{
    const auto& sc = processor.getScene();
    const bool isObs = selection.isObstacle();
    const bool isEm = selection.isEmitter();

    for (auto* comp : std::initializer_list<juce::Component*> { &shape, &engine, &note, &size, &bounce, &gain, &rotation, &spin, &mute, &preview })
        comp->setVisible (isObs);
    for (auto* comp : std::initializer_list<juce::Component*> { &rate, &angle, &speed, &maxBalls, &life, &spread, &ballSize, &fire })
        comp->setVisible (isEm);
    duplicate.setVisible (isObs || isEm);
    remove.setVisible (isObs || isEm);
    hint.setVisible (! isObs && ! isEm);

    if (isObs)
    {
        const auto& o = sc.obstacles[(size_t) selection.index];
        title.setText (shapeNames()[(int) o.shape] + "  " + noteName (effectiveNote (sc.world, o)), juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, Theme::noteColour (effectiveNote (sc.world, o)));
        shape.box.setSelectedItemIndex ((int) o.shape, juce::dontSendNotification);
        engine.box.setSelectedItemIndex (o.engine, juce::dontSendNotification);
        note.slider.setValue (o.note, juce::dontSendNotification);
        note.setEnabled (sc.world.pitchMode == PitchMode::Manual);
        note.label.setText (sc.world.pitchMode == PitchMode::Manual ? "Note" : "Note (auto)", juce::dontSendNotification);
        size.slider.setValue (o.size, juce::dontSendNotification);
        aspect.slider.setValue (o.aspect, juce::dontSendNotification);
        aspect.label.setText (o.shape == Shape::Arc ? "Arc span" : "Stretch", juce::dontSendNotification);
        aspect.setVisible (o.shape == Shape::Rect || o.shape == Shape::Arc);
        aspect.slider.setRange (o.shape == Shape::Arc ? 0.05 : 0.1, o.shape == Shape::Arc ? 1.0 : 6.0, 0.0);
        aspect.slider.setValue (o.aspect, juce::dontSendNotification);
        sides.slider.setValue (o.sides, juce::dontSendNotification);
        sides.label.setText (o.shape == Shape::Star ? "Points" : "Sides", juce::dontSendNotification);
        sides.setVisible (o.shape == Shape::Polygon || o.shape == Shape::Star);
        float rotDeg = std::fmod (o.rotation * kRadToDeg, 360.0f);
        if (rotDeg > 180.0f) rotDeg -= 360.0f;
        if (rotDeg < -180.0f) rotDeg += 360.0f;
        rotation.slider.setValue (rotDeg, juce::dontSendNotification);
        spin.slider.setValue (o.spin, juce::dontSendNotification);
        bounce.slider.setValue (o.bounce, juce::dontSendNotification);
        gain.slider.setValue (o.gain, juce::dontSendNotification);
        mute.setToggleState (o.muted, juce::dontSendNotification);
    }
    else
    {
        aspect.setVisible (false);
        sides.setVisible (false);
    }

    if (isEm)
    {
        const auto& e = sc.emitters[(size_t) selection.index];
        title.setText ("Ball emitter", juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, Theme::accent);
        rate.box.setSelectedItemIndex ((int) e.rate, juce::dontSendNotification);
        float a = std::fmod (e.angle * kRadToDeg, 360.0f);
        if (a > 180.0f) a -= 360.0f;
        if (a < -180.0f) a += 360.0f;
        angle.slider.setValue (a, juce::dontSendNotification);
        speed.slider.setValue (e.speed, juce::dontSendNotification);
        maxBalls.slider.setValue (e.maxBalls, juce::dontSendNotification);
        life.slider.setValue (e.life, juce::dontSendNotification);
        spread.slider.setValue (e.spread * kRadToDeg, juce::dontSendNotification);
        ballSize.slider.setValue (e.ballSize, juce::dontSendNotification);
    }

    if (! isObs && ! isEm)
    {
        title.setText ("Nothing selected", juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, Theme::text);
    }
    resized();
}

void ObjectPage::layoutRows (const juce::Array<juce::Component*>& rows, juce::Rectangle<int> area)
{
    for (auto* r : rows)
        if (r->isVisible())
            r->setBounds (area.removeFromTop (kRow + 2));
}

void ObjectPage::paint (juce::Graphics&) {}

void ObjectPage::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);

    auto buttons = r.removeFromBottom (32);
    const int bw = buttons.getWidth() / 3;
    if (selection.isObstacle()) preview.setBounds (buttons.removeFromLeft (bw).reduced (2));
    if (selection.isEmitter()) fire.setBounds (buttons.removeFromLeft (bw).reduced (2));
    duplicate.setBounds (buttons.removeFromLeft (bw).reduced (2));
    remove.setBounds (buttons.reduced (2));

    hint.setBounds (r);
    layoutRows ({ &shape, &engine, &note, &size, &aspect, &sides, &rotation, &spin, &bounce, &gain, &mute,
                  &rate, &angle, &speed, &maxBalls, &life, &spread, &ballSize }, r);
}

//==============================================================================
WorldPage::WorldPage (RicochetProcessor& p) : processor (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &boundary, &gravity, &gravityAngle, &friction, &wallBounce,
                                                             &collisions, &pitchMode, &scale, &root, &octave, &range,
                                                             &quantize, &clearAll })
        addAndMakeVisible (c);

    auto bindCombo = [this] (ComboRow& row, std::function<void (World&, int)> fn)
    {
        row.box.onChange = [this, &row, fn]
        {
            const int i = row.box.getSelectedItemIndex();
            edit ([fn, i] (World& w) { fn (w, i); }, true);
        };
    };
    auto bindSlider = [this] (SliderRow& row, std::function<void (World&, double)> fn)
    {
        row.slider.onDragStart = [this] { processor.pushUndo(); };
        row.slider.onValueChange = [this, &row, fn]
        {
            const double v = row.slider.getValue();
            edit ([fn, v] (World& w) { fn (w, v); }, false);
        };
    };

    bindCombo (boundary,  [] (World& w, int i) { w.boundary = (Boundary) i; });
    bindCombo (pitchMode, [] (World& w, int i) { w.pitchMode = (PitchMode) i; });
    bindCombo (scale,     [] (World& w, int i) { w.scale = i; });
    bindCombo (root,      [] (World& w, int i) { w.root = i; });
    bindCombo (quantize,  [] (World& w, int i) { w.quantize = (Quantize) i; });
    bindSlider (gravity,      [] (World& w, double v) { w.gravity = (float) v; });
    bindSlider (gravityAngle, [] (World& w, double v) { w.gravityAngle = (float) v / kRadToDeg; });
    bindSlider (friction,     [] (World& w, double v) { w.friction = (float) v; });
    bindSlider (wallBounce,   [] (World& w, double v) { w.wallBounce = (float) v; });
    bindSlider (octave,       [] (World& w, double v) { w.octave = (int) v; });
    bindSlider (range,        [] (World& w, double v) { w.range = (int) v; });
    collisions.onClick = [this] { const bool b = collisions.getToggleState(); edit ([b] (World& w) { w.ballCollisions = b; }, true); };

    clearAll.onClick = [this]
    {
        auto s = processor.getScene();
        s.clear();
        processor.setScene (s, true);
        processor.resetSimulation();
    };
    clearAll.setColour (juce::TextButton::buttonColourId, Theme::rec.withAlpha (0.35f));
    refresh();
}

void WorldPage::edit (const std::function<void (World&)>& fn, bool undo)
{
    auto s = processor.getScene();
    fn (s.world);
    processor.setScene (s, undo);
}

void WorldPage::refresh()
{
    const auto& w = processor.getScene().world;
    boundary.box.setSelectedItemIndex ((int) w.boundary, juce::dontSendNotification);
    gravity.slider.setValue (w.gravity, juce::dontSendNotification);
    gravityAngle.slider.setValue (std::fmod (w.gravityAngle * kRadToDeg + 360.0f, 360.0f), juce::dontSendNotification);
    friction.slider.setValue (w.friction, juce::dontSendNotification);
    wallBounce.slider.setValue (w.wallBounce, juce::dontSendNotification);
    collisions.setToggleState (w.ballCollisions, juce::dontSendNotification);
    pitchMode.box.setSelectedItemIndex ((int) w.pitchMode, juce::dontSendNotification);
    scale.box.setSelectedItemIndex (w.scale, juce::dontSendNotification);
    root.box.setSelectedItemIndex (w.root, juce::dontSendNotification);
    octave.slider.setValue (w.octave, juce::dontSendNotification);
    range.slider.setValue (w.range, juce::dontSendNotification);
    quantize.box.setSelectedItemIndex ((int) w.quantize, juce::dontSendNotification);
    const bool autoPitch = w.pitchMode != PitchMode::Manual;
    octave.setEnabled (autoPitch);
    range.setEnabled (autoPitch);
}

void WorldPage::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    clearAll.setBounds (r.removeFromBottom (30).removeFromRight (130));
    for (auto* c : std::initializer_list<juce::Component*> { &boundary, &gravity, &gravityAngle, &friction, &wallBounce,
                                                             &collisions, &pitchMode, &scale, &root, &octave, &range, &quantize })
        c->setBounds (r.removeFromTop (kRow + 3));
}

//==============================================================================
SoundPage::SoundPage (RicochetProcessor& p) : processor (p)
{
    const std::pair<const char*, const char*> params[] {
        { ParamIDs::master, "Master" }, { ParamIDs::brightness, "Tone" }, { ParamIDs::decay, "Decay" },
        { ParamIDs::attack, "Attack" }, { ParamIDs::noteLength, "Length" }, { ParamIDs::drive, "Drive" },
        { ParamIDs::reverbMix, "Reverb" }, { ParamIDs::roomSize, "Room" }, { ParamIDs::delayMix, "Delay" },
        { ParamIDs::feedback, "Feedback" }, { ParamIDs::speed, "Speed" }
    };
    int i = 0;
    for (auto& [id, name] : params)
    {
        auto* k = knobs.add (new Knob());
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId,
                             juce::Colour::fromHSV (std::fmod (0.47f + (float) i * 0.07f, 1.0f), 0.65f, 0.95f, 1.0f));
        k->label.setText (name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (Theme::font (12.0f, true));
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, k->slider);
        ++i;
    }

    delayDiv.addItemList (delayDivisionNames(), 1);
    addAndMakeVisible (delayDiv);
    delayDivAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, ParamIDs::delayDiv, delayDiv);
    delayDivLabel.setText ("Delay time", juce::dontSendNotification);
    delayDivLabel.setFont (Theme::font (12.0f));
    delayDivLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (delayDivLabel);

    addAndMakeVisible (sync);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::syncHost, sync);
}

void SoundPage::paint (juce::Graphics& g)
{
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f));
    g.drawFittedText ("Each obstacle picks its own engine (Object tab). These controls shape them all.",
                      getLocalBounds().reduced (12, 6).removeFromBottom (32), juce::Justification::centredLeft, 2);
}

void SoundPage::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    r.removeFromBottom (36);
    auto bottom = r.removeFromBottom (60);
    auto divRow = bottom.removeFromTop (28);
    delayDivLabel.setBounds (divRow.removeFromLeft (86));
    delayDiv.setBounds (divRow.removeFromLeft (120).reduced (0, 2));
    sync.setBounds (bottom.removeFromTop (28));

    const int cols = 3;
    const int rows = (knobs.size() + cols - 1) / cols;
    const int cw = r.getWidth() / cols;
    const int ch = juce::jmin (110, r.getHeight() / juce::jmax (1, rows));
    for (int i = 0; i < knobs.size(); ++i)
    {
        juce::Rectangle<int> cell (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
        knobs[i]->label.setBounds (cell.removeFromTop (16));
        knobs[i]->slider.setBounds (cell.reduced (6, 0));
    }
}

//==============================================================================
RandomPage::RandomPage (RicochetProcessor& p) : processor (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &density, &chaos, &mutateAmount, &lockObstacles, &lockEmitters,
                                                             &lockSounds, &lockWorld, &keepScale, &randomize, &mutate, &seed })
        addAndMakeVisible (c);

    const auto& o = processor.randomOptions;
    density.slider.setValue (o.density, juce::dontSendNotification);
    chaos.slider.setValue (o.chaos, juce::dontSendNotification);
    mutateAmount.slider.setValue (0.35, juce::dontSendNotification);
    lockObstacles.setToggleState (o.lockObstacles, juce::dontSendNotification);
    lockEmitters.setToggleState (o.lockEmitters, juce::dontSendNotification);
    lockSounds.setToggleState (o.lockSounds, juce::dontSendNotification);
    lockWorld.setToggleState (o.lockWorld, juce::dontSendNotification);
    keepScale.setToggleState (o.keepScale, juce::dontSendNotification);

    density.slider.onValueChange = [this] { push(); };
    chaos.slider.onValueChange = [this] { push(); };
    for (auto* t : { &lockObstacles, &lockEmitters, &lockSounds, &lockWorld, &keepScale })
        t->onClick = [this] { push(); };

    randomize.setColour (juce::TextButton::buttonColourId, Theme::accent2.withAlpha (0.55f));
    randomize.onClick = [this] { processor.randomize(); };
    mutate.onClick = [this] { processor.mutate ((float) mutateAmount.slider.getValue()); };
    seed.setFont (Theme::font (12.0f));
    seed.setColour (juce::Label::textColourId, Theme::textDim);
    startTimerHz (4);
}

void RandomPage::push()
{
    auto& o = processor.randomOptions;
    o.density = (float) density.slider.getValue();
    o.chaos = (float) chaos.slider.getValue();
    o.lockObstacles = lockObstacles.getToggleState();
    o.lockEmitters = lockEmitters.getToggleState();
    o.lockSounds = lockSounds.getToggleState();
    o.lockWorld = lockWorld.getToggleState();
    o.keepScale = keepScale.getToggleState();
}

void RandomPage::timerCallback()
{
    seed.setText ("Last seed: " + juce::String::toHexString ((int) processor.getLastSeed()).toUpperCase(), juce::dontSendNotification);
}

void RandomPage::paint (juce::Graphics& g)
{
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f));
    g.drawFittedText ("Randomize builds a whole new scene. Tick what you want to keep. "
                      "Mutate nudges the current scene. Every result can be saved as a preset.",
                      getLocalBounds().reduced (12, 6).removeFromBottom (40), juce::Justification::centredLeft, 3);
}

void RandomPage::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromBottom (44);
    for (auto* c : std::initializer_list<juce::Component*> { &density, &chaos, &lockObstacles, &lockEmitters, &lockSounds, &lockWorld, &keepScale })
        c->setBounds (r.removeFromTop (kRow + 3));
    r.removeFromTop (8);
    randomize.setBounds (r.removeFromTop (40).reduced (0, 2));
    r.removeFromTop (12);
    mutateAmount.setBounds (r.removeFromTop (kRow + 3));
    mutate.setBounds (r.removeFromTop (34).reduced (0, 2));
    seed.setBounds (r.removeFromTop (24));
}

//==============================================================================
Inspector::Inspector (RicochetProcessor& p, Selection& s, SceneCanvas& c)
    : processor (p), selection (s)
{
    objectPage = new ObjectPage (p, s, c);
    worldPage = new WorldPage (p);
    tabs.addTab ("Object", Theme::panel, objectPage, true);
    tabs.addTab ("World", Theme::panel, worldPage, true);
    tabs.addTab ("Sound", Theme::panel, new SoundPage (p), true);
    tabs.addTab ("Random", Theme::panel, new RandomPage (p), true);
    tabs.setTabBarDepth (34);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);
    processor.addChangeListener (this);
    selection.addChangeListener (this);
}

Inspector::~Inspector()
{
    processor.removeChangeListener (this);
    selection.removeChangeListener (this);
}

void Inspector::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &selection && (selection.isObstacle() || selection.isEmitter()))
        tabs.setCurrentTabIndex (0);
    objectPage->refresh();
    worldPage->refresh();
}

void Inspector::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
}

void Inspector::resized()
{
    tabs.setBounds (getLocalBounds().reduced (4));
}
