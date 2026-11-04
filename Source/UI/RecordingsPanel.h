#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>

/** Strip of recorded takes. Each tile can be dragged straight into a DAW or the Finder. */
class RecordingsPanel : public juce::Component,
                        private juce::Timer
{
public:
    explicit RecordingsPanel (RicochetProcessor&);
    ~RecordingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void addRecording (const juce::File&);
    void rescan();

private:
    class Tile;
    void timerCallback() override;
    void layoutTiles();
    void exportFile (const juce::File&);

    RicochetProcessor& processor;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbCache { 32 };
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Tile> tiles;
    juce::TextButton openFolder { "Folder" };
    std::unique_ptr<juce::FileChooser> chooser;
    float pulse = 0.0f;
};
