#include "Theme.h"

namespace Theme
{

juce::Colour noteColour (int midiNote, float alpha)
{
    const int pc = ((midiNote % 12) + 12) % 12;
    const int fifthsIndex = (pc * 7) % 12;               // C G D A E B F# ... around the wheel
    const float hue = std::fmod (0.47f + (float) fifthsIndex / 12.0f, 1.0f);
    const int octave = midiNote / 12 - 1;
    const float bright = juce::jlimit (0.55f, 1.0f, 0.62f + 0.07f * (float) (octave - 2));
    const float sat = juce::jlimit (0.45f, 0.9f, 0.9f - 0.05f * (float) (octave - 3));
    return juce::Colour::fromHSV (hue, sat, bright, alpha);
}

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::TabbedComponent::backgroundColourId, panel);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, panelHi);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, outline);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (outline);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, angle, true);
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (panelHi);
    g.fillEllipse (juce::Rectangle<float> (radius * 1.2f, radius * 1.2f).withCentre (c));
    const juce::Point<float> tip (c.x + std::sin (angle) * radius * 0.5f, c.y - std::cos (angle) * radius * 0.5f);
    g.setColour (text);
    g.drawLine ({ c.getPointOnCircumference (radius * 0.18f, angle), tip }, 2.0f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float,
                                    float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto colour = s.findColour (juce::Slider::trackColourId);
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (outline);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (colour.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ pos, cy }));
    g.setColour (text);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ pos, cy }));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base,
                                        bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down) c = c.brighter (0.2f);
    else if (over) c = c.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (b.getToggleState() ? c.brighter (0.4f) : outline);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto pill = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (0.9f) : outline);
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (on ? bg : (over ? text : textDim));
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? pill.getRight() - 8.0f : pill.getX() + 8.0f, pill.getCentreY() }));
    g.setColour (text);
    g.setFont (font (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (w, h).toFloat().reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textDim);
    g.fillPath (arrow);
}

void LookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const bool front = b.isFrontTab();
    if (front)
    {
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.removeFromBottom (2.0f).reduced (12.0f, 0.0f), 1.0f);
    }
    g.setColour (front ? text : (over ? text : textDim));
    g.setFont (font (13.0f, front));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

} // namespace Theme
