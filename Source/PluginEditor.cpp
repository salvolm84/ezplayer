#include "PluginEditor.h"
#include "UI/DragTypes.h"
#include "UI/RemapEditor.h"
#include "UI/SettingsPanel.h"

//==============================================================================
class GroovePlayerEditor::Overlay : public juce::Component
{
public:
    Overlay (std::unique_ptr<juce::Component> c, int w, int h) : content (std::move (c)), maxW (w), maxH (h)
    {
        addAndMakeVisible (*content);
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black.withAlpha (0.55f)); }

    void resized() override
    {
        content->setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (maxW, getWidth() - 40),
                                                                    juce::jmin (maxH, getHeight() - 40)));
    }

    void mouseDown (const juce::MouseEvent&) override { if (onClose) onClose(); }

    std::function<void()> onClose;

private:
    std::unique_ptr<juce::Component> content;
    int maxW, maxH;
};

//==============================================================================
GroovePlayerEditor::GroovePlayerEditor (GroovePlayerProcessor& p)
    : AudioProcessorEditor (p), proc (p), browser (p), transport (p), lane (p),
      velocityAttachment (p.params, "velocity", velocity),
      thruAttachment (p.params, "thru", thruToggle)
{
    setLookAndFeel (&lnf);

    titleLabel.setText ("GROOVEPLAYER", juce::dontSendNotification);
    titleLabel.setFont (theme::font (19.0f, true));
    titleLabel.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (titleLabel);

    velocityLabel.setText ("Velocity", juce::dontSendNotification);
    velocityLabel.setFont (theme::font (12.5f));
    velocityLabel.setColour (juce::Label::textColourId, theme::dim);
    velocityLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (velocityLabel);

    velocity.setSliderStyle (juce::Slider::LinearHorizontal);
    velocity.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    velocity.setTooltip ("Scales the velocity of every played / exported note");
    addAndMakeVisible (velocity);

    for (int ch = 1; ch <= 16; ++ch)
        channelBox.addItem ("Ch " + juce::String (ch), ch);
    channelBox.setTooltip ("MIDI channel of the generated notes");
    channelBox.onChange = [this]
    {
        if (auto* param = proc.params.getParameter ("channel"))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) channelBox.getSelectedId()));
    };
    addAndMakeVisible (channelBox);

    thruToggle.setTooltip ("Pass incoming MIDI through to the output");
    addAndMakeVisible (thruToggle);

    remapButton.setTooltip ("Edit the output note mapping (applied to playback and drag & drop)");
    remapButton.onClick = [this]
    {
        auto ed = std::make_unique<RemapEditor> (proc);
        ed->onClose = [this] { closeOverlay(); };
        showOverlay (std::move (ed), 860, 720);
    };
    addAndMakeVisible (remapButton);

    settingsButton.onClick = [this]
    {
        auto sp = std::make_unique<SettingsPanel> (proc);
        sp->onClose = [this] { closeOverlay(); };
        showOverlay (std::move (sp), 820, 560);
    };
    addAndMakeVisible (settingsButton);

    browser.onAddToSong = [this] (int grooveIndex) { lane.addGrooveAtEnd (grooveIndex); };
    addAndMakeVisible (browser);

    transport.onPlay = [this] { togglePlay(); };
    transport.onRewind = [this] { proc.engine.seek (0.0); };
    transport.onUndo = [this] { proc.song.undo(); };
    transport.onRedo = [this] { proc.song.redo(); };
    transport.onDelete = [this] { lane.deleteSelected(); };
    transport.onDuplicate = [this] { lane.duplicateSelected(); };
    transport.onClear = [this] { proc.song.clear(); };
    transport.onExport = [this] { exportMidi (false); };
    transport.onZoom = [this] (double f) { lane.zoomBy (f); };
    transport.onSnapChanged = [this] (int id) { lane.setSnapMode (id); };
    transport.setSnap (lane.getSnapMode());
    addAndMakeVisible (transport);

    lane.onPreviewGroove = [this] (tt::Groove::Ptr g) { proc.startPreview (g); };
    lane.onExportClipsRequest = [this] (const juce::File&) { exportMidi (true); };
    lane.onTogglePlay = [this] { togglePlay(); };
    addAndMakeVisible (lane);

    proc.getDrumMapListeners().add (this);
    updateRemapButton();

    setResizable (true, true);
    setResizeLimits (980, 640, 2600, 1700);
    setSize ((int) proc.uiState.getProperty ("width", 1180), (int) proc.uiState.getProperty ("height", 780));

    startTimerHz (30);
}

GroovePlayerEditor::~GroovePlayerEditor()
{
    proc.getDrumMapListeners().remove (this);
    overlay.reset();
    setLookAndFeel (nullptr);
}

void GroovePlayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::panel);
    g.fillRect (getLocalBounds().removeFromTop (44));
    g.setColour (theme::outline);
    g.fillRect (0, 44, getWidth(), 1);
}

void GroovePlayerEditor::resized()
{
    proc.uiState.setProperty ("width", getWidth(), nullptr);
    proc.uiState.setProperty ("height", getHeight(), nullptr);

    auto r = getLocalBounds();
    auto header = r.removeFromTop (44).reduced (10, 8);
    titleLabel.setBounds (header.removeFromLeft (170));

    settingsButton.setBounds (header.removeFromRight (80));
    header.removeFromRight (6);
    remapButton.setBounds (header.removeFromRight (230));
    header.removeFromRight (12);
    thruToggle.setBounds (header.removeFromRight (92));
    header.removeFromRight (6);
    channelBox.setBounds (header.removeFromRight (72));
    header.removeFromRight (12);
    velocity.setBounds (header.removeFromRight (170));
    velocityLabel.setBounds (header.removeFromRight (60));

    auto laneH = juce::jlimit (130, 220, getHeight() / 5);
    lane.setBounds (r.removeFromBottom (laneH));
    transport.setBounds (r.removeFromBottom (38));
    browser.setBounds (r.reduced (10, 8));

    if (overlay != nullptr)
        overlay->setBounds (getLocalBounds());
}

//==============================================================================
void GroovePlayerEditor::showOverlay (std::unique_ptr<juce::Component> c, int w, int h)
{
    overlay = std::make_unique<Overlay> (std::move (c), w, h);
    overlay->onClose = [this] { closeOverlay(); };
    addAndMakeVisible (*overlay);
    overlay->setBounds (getLocalBounds());
}

void GroovePlayerEditor::closeOverlay()
{
    // Deferred: this is usually called from a button inside the overlay.
    juce::Component::SafePointer<GroovePlayerEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->overlay.reset(); });
}

void GroovePlayerEditor::togglePlay()
{
    if (proc.engine.internalPlaying.load())
    {
        proc.engine.setInternalPlaying (false);
    }
    else if (! proc.engine.hostPlaying.load())
    {
        proc.stopPreview();
        proc.engine.setInternalPlaying (true);
    }
}

void GroovePlayerEditor::updateRemapButton()
{
    remapButton.setButtonText ("Remap: " + proc.getDrumMap().name);
    remapButton.setToggleState (! proc.getDrumMap().isIdentity(), juce::dontSendNotification);
}

void GroovePlayerEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateRemapButton();
}

void GroovePlayerEditor::timerCallback()
{
    transport.update();
    lane.updatePlayhead();
    browser.updatePlayState();

    auto ch = (int) proc.params.getRawParameterValue ("channel")->load();
    if (channelBox.getSelectedId() != ch)
        channelBox.setSelectedId (ch, juce::dontSendNotification);
}

//==============================================================================
std::vector<tt::TimelineNote> GroovePlayerEditor::notesForDrag (const juce::var& desc, double& length, juce::String& name)
{
    auto type = drag::typeOf (desc);
    length = 0.0;

    if (type == drag::groove)
    {
        juce::String err;
        auto g = proc.app->loadGroove (drag::indexOf (desc), err);
        if (g == nullptr)
            return {};

        tt::SongClip c;
        c.length = g->lengthBeats;
        c.groove = g;
        length = g->lengthBeats;
        name = g->name;

        // Toontrack names like "Verse 02" are more useful with their groove family.
        if (auto index = proc.app->getIndex())
        {
            auto i = drag::indexOf (desc);
            if (juce::isPositiveAndBelow (i, (int) index->grooves.size()))
            {
                auto& f = index->folders[(size_t) index->grooves[(size_t) i].folder];
                if (f.section.isNotEmpty())
                    name = f.section + " - " + name;
            }
        }

        return tt::Song::flatten ({ c }, 0.0);
    }

    std::vector<tt::SongClip> clips;
    if (type == drag::clips)
    {
        auto sel = lane.getSelection();
        for (auto& c : proc.song.getClips())
            if (std::find (sel.begin(), sel.end(), c.id) != sel.end())
                clips.push_back (c);
    }
    else if (type == drag::song)
    {
        clips = proc.song.getClips();
    }

    if (clips.empty())
        return {};

    auto start = type == drag::song ? 0.0 : clips.front().start;
    double end = 0.0;
    for (auto& c : clips)
    {
        start = juce::jmin (start, c.start);
        end = juce::jmax (end, c.end());
    }

    length = end - start;
    name = type == drag::song ? juce::String ("GroovePlayer Song")
         : clips.size() == 1 && clips.front().groove != nullptr ? clips.front().groove->name
                                                                 : juce::String ("GroovePlayer Parts");
    return tt::Song::flatten (clips, start);
}

bool GroovePlayerEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                               juce::StringArray& files, bool& canMoveFiles)
{
    double length = 0.0;
    juce::String name;
    auto notes = notesForDrag (details.description, length, name);
    if (notes.empty() || length <= 0.0)
        return false;

    auto file = tt::writeTempMidiForDrag (name, notes, length, proc.makeExportOptions (name));
    if (! file.existsAsFile())
        return false;

    files.add (file.getFullPathName());
    canMoveFiles = false;
    return true;
}

void GroovePlayerEditor::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    lane.repaint();
}

void GroovePlayerEditor::exportMidi (bool selectionOnly)
{
    double length = 0.0;
    juce::String name;
    auto notes = notesForDrag (drag::make (selectionOnly ? drag::clips : drag::song), length, name);
    if (notes.empty())
        return;

    auto initial = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                       .getChildFile (tt::makeSafeFileName (name) + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI", initial, "*.mid");

    juce::Component::SafePointer<GroovePlayerEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, notes, length, name] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (safe == nullptr || file == juce::File())
            return;

        if (! file.hasFileExtension ("mid;midi"))
            file = file.withFileExtension ("mid");

        if (! tt::writeMidiFile (file, notes, length, safe->proc.makeExportOptions (name)))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed",
                                                    "Could not write " + file.getFullPathName());
    });
}
