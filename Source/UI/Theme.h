#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    // Base palette: deep ink background with luminous accents.
    const juce::Colour bg        { 0xff0a0c16 };
    const juce::Colour panel     { 0xff121626 };
    const juce::Colour panelHi   { 0xff1b2036 };
    const juce::Colour outline   { 0xff2a3150 };
    const juce::Colour text      { 0xffe7e9f5 };
    const juce::Colour textDim   { 0xff8a90ad };
    const juce::Colour accent    { 0xff7cf2d2 };
    const juce::Colour accent2   { 0xffff6fb5 };
    const juce::Colour rec       { 0xffff4d5e };

    /** Note -> colour. Pitch classes follow the circle of fifths so consonant
        notes sit next to each other on the hue wheel; octave sets luminance. */
    juce::Colour noteColour (int midiNote, float alpha = 1.0f);

    juce::Font font (float height, bool bold = false);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                               float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (14.0f, (float) h * 0.5f), true); }
        juce::Font getLabelFont (juce::Label&) override { return font (13.0f); }
        juce::Font getPopupMenuFont() override { return font (14.0f); }
        void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
        int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 80; }
    };
}
