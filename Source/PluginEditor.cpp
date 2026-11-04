#include "PluginEditor.h"

RicochetEditor::RicochetEditor (RicochetProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    addAndMakeVisible (topBar);
    addAndMakeVisible (canvas);
    addAndMakeVisible (inspector);
    addAndMakeVisible (recordings);

    topBar.onNewRecording = [this] (const juce::File& f) { recordings.addRecording (f); };

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (1200, 700, 2400, 1600);
    setSize (1260, 820);
}

RicochetEditor::~RicochetEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void RicochetEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
}

void RicochetEditor::resized()
{
    auto r = getLocalBounds();
    topBar.setBounds (r.removeFromTop (54));
    r.reduce (8, 8);
    recordings.setBounds (r.removeFromBottom (96));
    r.removeFromBottom (8);
    inspector.setBounds (r.removeFromRight (juce::jlimit (330, 420, r.getWidth() / 3)));
    r.removeFromRight (8);
    canvas.setBounds (r);
}

bool RicochetEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) proc.redo(); else proc.undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'R')
    {
        proc.randomize();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* p = proc.apvts.getParameter (ParamIDs::running))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        return true;
    }
    return false;
}
