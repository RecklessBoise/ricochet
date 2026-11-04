#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/SceneCanvas.h"
#include "UI/Inspector.h"
#include "UI/TopBar.h"
#include "UI/RecordingsPanel.h"

class RicochetEditor : public juce::AudioProcessorEditor
{
public:
    explicit RicochetEditor (RicochetProcessor&);
    ~RicochetEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    RicochetProcessor& proc;
    Theme::LookAndFeel lnf;
    Selection selection;
    SceneCanvas canvas { proc, selection };
    Inspector inspector { proc, selection, canvas };
    TopBar topBar { proc, canvas };
    RecordingsPanel recordings { proc };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RicochetEditor)
};
