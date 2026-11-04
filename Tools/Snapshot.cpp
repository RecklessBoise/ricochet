// Renders the Ricochet editor to PNG files without opening a window (used for docs and visual checks).
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    const int presetIndices[] { 0, 13, 30, 52, 80, 100, 130, 150 };
    for (int idx : presetIndices)
    {
        RicochetProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        proc.setCurrentProgram (idx);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditorIfNeeded());
        ed->setSize (1260, 820);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int frame = 0; frame < 90; ++frame)
        {
            for (int b = 0; b < 4; ++b) { midi.clear(); proc.processBlock (buf, midi); }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("ricochet_" + juce::String (idx) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << f.getFullPathName() << std::endl;
        proc.editorBeingDeleted (ed.get());
        ed.reset();
    }
    return 0;
}
