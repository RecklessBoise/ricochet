#pragma once

#include "SceneCanvas.h"

class TopBar : public juce::Component,
               private juce::Timer,
               private juce::ChangeListener
{
public:
    TopBar (RicochetProcessor&, SceneCanvas&);
    ~TopBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (const juce::File&)> onNewRecording;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void showBounceMenu();
    void toggleRecord();
    void updateButtons();

    RicochetProcessor& processor;
    SceneCanvas& canvas;

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    juce::TextButton randomButton { "Random" }, mutateButton { "Mutate" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::TextButton playButton { "Play" }, resetButton { "Restart" };
    juce::TextButton recButton { "Rec" }, bounceButton { "Bounce" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttachment;
    std::unique_ptr<juce::AlertWindow> saveDialog;

    juce::Rectangle<int> logoArea;
    float logoPhase = 0.0f;
    int factoryIndex = 0;
};
