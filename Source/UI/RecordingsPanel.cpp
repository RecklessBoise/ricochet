#include "RecordingsPanel.h"

class RecordingsPanel::Tile : public juce::Component,
                              public juce::SettableTooltipClient,
                              private juce::ChangeListener
{
public:
    Tile (RecordingsPanel& o, const juce::File& f)
        : owner (o), file (f), thumb (256, o.formatManager, o.thumbCache)
    {
        thumb.addChangeListener (this);
        thumb.setSource (new juce::FileInputSource (file));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag into your DAW or the Finder. Right-click for more.\n" + file.getFullPathName());
    }

    ~Tile() override { thumb.removeChangeListener (this); }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (isMouseOver() ? Theme::panelHi.brighter (0.05f) : Theme::panelHi);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (isMouseOver() ? Theme::accent.withAlpha (0.7f) : Theme::outline);
        g.drawRoundedRectangle (r, 9.0f, 1.0f);

        auto inner = r.reduced (8.0f, 6.0f);
        auto top = inner.removeFromTop (16.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (11.0f, true));
        g.drawText (file.getFileNameWithoutExtension().fromFirstOccurrenceOf ("_", false, false),
                    top, juce::Justification::centredLeft, true);
        const double len = thumb.getTotalLength();
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f));
        g.drawText (juce::String::formatted ("%d:%02d", (int) len / 60, (int) len % 60), top, juce::Justification::centredRight);

        // waveform tinted with a colour derived from the name, so takes are easy to tell apart
        const auto hue = (float) (file.getFileName().hashCode() & 0xff) / 255.0f;
        const auto col = juce::Colour::fromHSV (hue, 0.6f, 0.95f, 1.0f);
        if (len > 0.0)
        {
            g.setColour (col.withAlpha (0.85f));
            thumb.drawChannels (g, inner.toNearestInt(), 0.0, len, 1.0f);
        }
        else
        {
            g.setColour (Theme::textDim);
            g.drawText ("...", inner, juce::Justification::centred);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStarted = false;
        if (! e.mods.isPopupMenu()) return;

        juce::PopupMenu m;
        juce::Component::SafePointer<Tile> safe (this);
        m.addItem ("Show in Finder", [f = file] { f.revealToUser(); });
        m.addItem ("Export as...", [safe] { if (safe != nullptr) safe->owner.exportFile (safe->file); });
        m.addItem ("Move to trash", [safe]
        {
            if (safe == nullptr) return;
            safe->file.moveToTrash();
            auto& o = safe->owner;
            juce::MessageManager::callAsync ([&o] { o.rescan(); });
        });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStarted || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 5) return;
        dragStarted = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { file.revealToUser(); }

    RecordingsPanel& owner;
    juce::File file;
    juce::AudioThumbnail thumb;
    bool dragStarted = false;
};

//==============================================================================
RecordingsPanel::RecordingsPanel (RicochetProcessor& p) : processor (p)
{
    formatManager.registerBasicFormats();
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
    addAndMakeVisible (openFolder);
    openFolder.setTooltip ("Open the recordings folder");
    openFolder.onClick = []
    {
        ric::recordingsFolder().createDirectory();
        ric::recordingsFolder().revealToUser();
    };
    rescan();
    startTimerHz (15);
}

RecordingsPanel::~RecordingsPanel()
{
    tiles.clear();
}

void RecordingsPanel::rescan()
{
    auto files = ric::recordingsFolder().findChildFiles (juce::File::findFiles, false, "*.wav");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });
    tiles.clear();
    for (int i = 0; i < juce::jmin (24, files.size()); ++i)
        content.addAndMakeVisible (tiles.add (new Tile (*this, files[i])));
    layoutTiles();
}

void RecordingsPanel::addRecording (const juce::File&)
{
    rescan();
    viewport.setViewPosition (0, 0);
}

void RecordingsPanel::exportFile (const juce::File& f)
{
    chooser = std::make_unique<juce::FileChooser> ("Export recording",
                                                   juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (f.getFileName()),
                                                   "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [f] (const juce::FileChooser& fc)
                          {
                              const auto dest = fc.getResult();
                              if (dest != juce::File())
                                  f.copyFileTo (dest.withFileExtension ("wav"));
                          });
}

void RecordingsPanel::timerCallback()
{
    pulse += 0.15f;
    if (processor.getRecorder().isRecording())
        repaint (0, 0, 140, getHeight());
}

void RecordingsPanel::layoutTiles()
{
    const int h = juce::jmax (40, viewport.getHeight() - 8);
    const int w = 190;
    content.setSize (juce::jmax (viewport.getWidth(), tiles.size() * w), h);
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (i * w, 0, w, h);
}

void RecordingsPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);

    auto r = getLocalBounds().reduced (12, 8);
    auto left = r.removeFromLeft (120);
    g.setFont (Theme::font (13.0f, true));
    g.setColour (Theme::text);
    g.drawText ("Takes", left.removeFromTop (20), juce::Justification::centredLeft);

    if (processor.getRecorder().isRecording())
    {
        const float a = 0.5f + 0.5f * std::sin (pulse);
        g.setColour (Theme::rec.withAlpha (0.4f + 0.6f * a));
        g.fillEllipse ((float) left.getX(), (float) left.getY() + 5.0f, 10.0f, 10.0f);
        g.setColour (Theme::rec);
        g.setFont (Theme::font (11.0f, true));
        g.drawText ("recording", left.removeFromTop (20).withTrimmedLeft (16), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        g.drawFittedText (tiles.isEmpty() ? "Press Rec or Bounce,\nthen drag takes out."
                                          : "Drag a take into\nyour DAW / Finder.",
                          left.removeFromTop (34), juce::Justification::topLeft, 2);
    }
}

void RecordingsPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto left = r.removeFromLeft (120);
    openFolder.setBounds (left.removeFromBottom (24).removeFromLeft (80));
    viewport.setBounds (r);
    layoutTiles();
}
