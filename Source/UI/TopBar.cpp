#include "TopBar.h"

TopBar::TopBar (RicochetProcessor& p, SceneCanvas& c) : processor (p), canvas (c)
{
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton, &randomButton, &mutateButton,
                     &undoButton, &redoButton, &playButton, &resetButton, &recButton, &bounceButton, &syncButton })
        addAndMakeVisible (b);

    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    randomButton.onClick = [this] { processor.randomize(); };
    mutateButton.onClick = [this] { processor.mutate(); };
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    resetButton.onClick = [this] { processor.resetSimulation(); };
    recButton.onClick = [this] { toggleRecord(); };
    bounceButton.onClick = [this] { showBounceMenu(); };

    playButton.setClickingTogglesState (true);
    playAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::running, playButton);

    syncButton.setClickingTogglesState (true);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::syncHost, syncButton);
    syncButton.setTooltip ("DAW Sync: play / stop follow your DAW, and the tempo and bar grid follow the DAW tempo.\n"
                           "Each DAW play restarts the scene from the same state.");

    tempoSlider.setSliderStyle (juce::Slider::LinearBar);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 90, 28);
    tempoSlider.setColour (juce::Slider::trackColourId, Theme::accent.withAlpha (0.22f));
    tempoSlider.setColour (juce::Slider::backgroundColourId, Theme::panelHi);
    tempoSlider.setTooltip ("Internal tempo (used when DAW Sync is off). Drag or double-click to type.");
    tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::tempo, tempoSlider);
    addAndMakeVisible (tempoSlider);

    tempoLabel.setJustificationType (juce::Justification::centred);
    tempoLabel.setFont (Theme::font (13.0f, true));
    tempoLabel.setColour (juce::Label::textColourId, Theme::accent);
    tempoLabel.setColour (juce::Label::backgroundColourId, Theme::panelHi);
    tempoLabel.setTooltip ("Tempo from your DAW");
    addChildComponent (tempoLabel);

    randomButton.setColour (juce::TextButton::buttonColourId, Theme::accent2.withAlpha (0.45f));
    recButton.setColour (juce::TextButton::buttonOnColourId, Theme::rec);
    recButton.setClickingTogglesState (false);

    presetButton.setTooltip ("Browse presets");
    randomButton.setTooltip ("New random scene (settings in the Random tab)");
    mutateButton.setTooltip ("Small random variation of the current scene");
    resetButton.setTooltip ("Restart the simulation from the initial state");
    recButton.setTooltip ("Record what plays to a WAV file");
    bounceButton.setTooltip ("Render N bars offline to a WAV file");

    const auto& presets = ric::factoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == processor.getPresetName()) factoryIndex = (int) i;

    processor.addChangeListener (this);
    updateButtons();
    startTimerHz (20);
}

TopBar::~TopBar()
{
    processor.removeChangeListener (this);
}

void TopBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateButtons();
}

void TopBar::updateButtons()
{
    presetButton.setButtonText (processor.getPresetName() + juce::String (juce::CharPointer_UTF8 ("   \xe2\x96\xbe")));
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    updateTransportDisplay();
}

void TopBar::timerCallback()
{
    logoPhase += 0.02f;
    auto& rec = processor.getRecorder();
    const bool recording = rec.isRecording();
    recButton.setToggleState (recording, juce::dontSendNotification);
    if (recording)
    {
        const int secs = (int) rec.getRecordedSeconds();
        recButton.setButtonText (juce::String::formatted ("Stop %d:%02d", secs / 60, secs % 60));
    }
    else
    {
        recButton.setButtonText ("Rec");
    }

    if (processor.isBouncing())
        bounceButton.setButtonText (juce::String (juce::roundToInt (processor.getBounceProgress() * 100.0f)) + " %");
    else
    {
        bounceButton.setButtonText ("Bounce");
        const auto f = processor.takeFinishedBounce();
        if (f.existsAsFile() && onNewRecording) onNewRecording (f);
    }
    updateTransportDisplay();
    repaint (logoArea);
}

void TopBar::updateTransportDisplay()
{
    const bool syncOn = syncButton.getToggleState();
    const bool following = processor.isFollowingHost();

    tempoSlider.setVisible (! following);
    tempoLabel.setVisible (following);
    if (following)
        tempoLabel.setText ("DAW " + juce::String (processor.getCurrentBpm(), 1) + " BPM", juce::dontSendNotification);

    // While the DAW drives the transport the local Play button is only a status light.
    playButton.setEnabled (! following);
    if (following)
        playButton.setButtonText (processor.isHostPlaying() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 DAW"))
                                                            : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 DAW")));
    else
        playButton.setButtonText (playButton.getToggleState() ? "Pause" : "Play");

    syncButton.setButtonText (syncOn && ! processor.hostHasTransport() ? "DAW Sync (no host)" : "DAW Sync");
}

void TopBar::toggleRecord()
{
    auto& rec = processor.getRecorder();
    if (rec.isRecording())
    {
        const auto f = rec.stop();
        if (f.existsAsFile() && onNewRecording) onNewRecording (f);
    }
    else if (! rec.start())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Reckless Ricochet",
                                                "Could not create a recording file in\n" + ric::recordingsFolder().getFullPathName());
    }
}

void TopBar::showBounceMenu()
{
    if (processor.isBouncing()) return;
    juce::PopupMenu m;
    m.addSectionHeader ("Render offline to WAV");
    for (int bars : { 2, 4, 8, 16, 32, 64 })
        m.addItem (juce::String (bars) + " bars", [this, bars] { processor.startBounce (bars); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bounceButton));
}

void TopBar::stepPreset (int delta)
{
    const auto& presets = ric::factoryPresets();
    factoryIndex = (factoryIndex + delta + (int) presets.size()) % (int) presets.size();
    processor.loadPreset (presets[(size_t) factoryIndex]);
}

void TopBar::showPresetMenu()
{
    processor.refreshUserPresets();
    juce::PopupMenu menu;
    const auto& presets = ric::factoryPresets();

    for (const auto& cat : ric::factoryCategories())
    {
        juce::PopupMenu sub;
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].category == cat)
                sub.addItem (presets[i].name, true, presets[i].name == processor.getPresetName(), [this, i]
                {
                    factoryIndex = (int) i;
                    processor.loadPreset (ric::factoryPresets()[i]);
                });
        menu.addSubMenu (cat, sub);
    }

    juce::PopupMenu user;
    const auto& ups = processor.getUserPresets();
    for (size_t i = 0; i < ups.size(); ++i)
        user.addItem (ups[i].name, [this, ns = ups[i]] { processor.loadPreset (ns); });
    if (ups.empty()) user.addItem ("(none yet - use Save)", false, false, nullptr);
    menu.addSeparator();
    menu.addSubMenu ("User", user);
    menu.addItem ("Open user preset folder", []
    {
        ric::userPresetFolder().createDirectory();
        ric::userPresetFolder().revealToUser();
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton));
}

void TopBar::savePreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the preset:", juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", processor.getPresetName());
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TopBar> safe (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveDialog == nullptr) return;
        const auto name = safe->saveDialog->getTextEditorContents ("name").trim();
        if (result == 1 && name.isNotEmpty() && ! safe->processor.saveCurrentAsUserPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Reckless Ricochet", "Could not save the preset.");
        safe->saveDialog.reset();
    }), false);
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    // Logo: "RECKLESS" small above "RICOCHET", letters lit by the current ambient (note) colour.
    const juce::String word ("RICOCHET");
    const auto amb = canvas.getAmbientColour();
    auto r = logoArea.toFloat();
    auto top = r.removeFromTop (r.getHeight() * 0.36f);
    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::accent2.interpolatedWith (amb, 0.3f));
    g.drawText ("R E C K L E S S", top, juce::Justification::centredLeft);
    g.setFont (Theme::font (20.0f, true));
    const float cw = r.getWidth() / (float) word.length();
    for (int i = 0; i < word.length(); ++i)
    {
        const float wave = 0.5f + 0.5f * std::sin (logoPhase * 3.0f + (float) i * 0.6f);
        const auto col = Theme::accent.interpolatedWith (amb, wave).interpolatedWith (Theme::accent2, 0.25f * (1.0f - wave));
        g.setColour (col);
        g.drawText (juce::String::charToString (word[i]), juce::Rectangle<float> (r.getX() + cw * (float) i, r.getY(), cw, r.getHeight()),
                    juce::Justification::centred);
    }

    // a little bouncing marble under the logo
    const float t = std::fmod (logoPhase, 1.0f);
    const float bx = r.getX() + r.getWidth() * t;
    const float by = r.getBottom() - 4.0f - 8.0f * std::abs (std::sin (t * juce::MathConstants<float>::pi * 4.0f));
    g.setColour (amb);
    g.fillEllipse (bx - 3.0f, by - 3.0f, 6.0f, 6.0f);

    g.setColour (Theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (10, 9);
    logoArea = r.removeFromLeft (150);
    r.removeFromLeft (10);

    auto place = [&r] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w).reduced (2, 0)); };
    place (prevButton, 30);
    place (presetButton, juce::jlimit (150, 240, r.getWidth() / 5));
    place (nextButton, 30);
    place (saveButton, 56);
    r.removeFromLeft (10);
    place (randomButton, 72);
    place (mutateButton, 66);
    r.removeFromLeft (10);
    place (undoButton, 50);
    place (redoButton, 50);

    auto right = r;
    auto placeR = [&right] (juce::Component& c, int w) { c.setBounds (right.removeFromRight (w).reduced (2, 0)); };
    placeR (bounceButton, 74);
    placeR (recButton, 84);
    right.removeFromRight (8);
    placeR (resetButton, 70);
    placeR (playButton, 64);
    right.removeFromRight (6);
    auto tempoArea = right.removeFromRight (100).reduced (2, 2);
    tempoSlider.setBounds (tempoArea);
    tempoLabel.setBounds (tempoArea);
    placeR (syncButton, 84);
}
