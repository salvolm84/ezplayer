#include "RemapEditor.h"

namespace
{
    juce::String gmDrumName (int n)
    {
        static const char* names[] = {
            "Acoustic Bass Drum", "Bass Drum 1", "Side Stick", "Acoustic Snare", "Hand Clap", "Electric Snare",
            "Low Floor Tom", "Closed Hi-Hat", "High Floor Tom", "Pedal Hi-Hat", "Low Tom", "Open Hi-Hat",
            "Low-Mid Tom", "Hi-Mid Tom", "Crash Cymbal 1", "High Tom", "Ride Cymbal 1", "Chinese Cymbal",
            "Ride Bell", "Tambourine", "Splash Cymbal", "Cowbell", "Crash Cymbal 2", "Vibraslap",
            "Ride Cymbal 2", "Hi Bongo", "Low Bongo", "Mute Hi Conga", "Open Hi Conga", "Low Conga",
            "High Timbale", "Low Timbale", "High Agogo", "Low Agogo", "Cabasa", "Maracas", "Short Whistle",
            "Long Whistle", "Short Guiro", "Long Guiro", "Claves", "Hi Wood Block", "Low Wood Block",
            "Mute Cuica", "Open Cuica", "Mute Triangle", "Open Triangle" };

        return n >= 35 && n <= 81 ? juce::String (names[n - 35]) : juce::String();
    }

    juce::String outputItemText (int n)
    {
        auto t = tt::noteName (n) + "  (" + juce::String (n) + ")";
        auto gm = gmDrumName (n);
        if (gm.isNotEmpty())
            t << "  GM " << gm;
        return t;
    }
}

//==============================================================================
class RemapEditor::Row : public juce::Component
{
public:
    explicit Row (RemapEditor& o) : owner (o)
    {
        out.addItem ("Mute", 1);
        for (int n = 0; n < 128; ++n)
            out.addItem (outputItemText (n), n + 2);
        out.onChange = [this]
        {
            auto id = out.getSelectedId();
            owner.setOutput (note, id == 1 ? -1 : id - 2);
        };
        addAndMakeVisible (out);
    }

    void setNote (int n)
    {
        note = n;
        auto mapped = owner.proc.getDrumMap().map[(size_t) n];
        out.setSelectedId (mapped < 0 ? 1 : mapped + 2, juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto mapped = owner.proc.getDrumMap().map[(size_t) note];
        auto changed = mapped != note;

        g.fillAll (note % 2 ? theme::rowAlt : theme::row);
        auto r = getLocalBounds().reduced (8, 0);

        g.setColour (changed ? theme::accent : theme::dim);
        g.setFont (theme::font (13.0f, true));
        g.drawText (juce::String (note), r.removeFromLeft (34), juce::Justification::centredLeft, false);
        g.setFont (theme::font (13.0f));
        g.drawText (tt::noteName (note), r.removeFromLeft (44), juce::Justification::centredLeft, false);

        auto piece = tt::kitPieceForNote (note);
        g.setColour (theme::pieceColour (piece));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) r.getX() + 4.0f, getHeight() * 0.5f }));
        r.removeFromLeft (14);

        auto name = tt::toontrackArticulationName (note);
        g.setColour (name.isEmpty() ? theme::dim : theme::text);
        g.drawText (name.isEmpty() ? juce::String ("-") : name, r.removeFromLeft (200), juce::Justification::centredLeft, true);

        g.setColour (theme::dim);
        g.drawText ("->", r.removeFromLeft (26), juce::Justification::centred, false);
    }

    void resized() override
    {
        out.setBounds (getLocalBounds().reduced (6, 2).withTrimmedLeft (8 + 34 + 44 + 14 + 200 + 26));
    }

private:
    RemapEditor& owner;
    juce::ComboBox out;
    int note = 0;
};

//==============================================================================
RemapEditor::RemapEditor (GroovePlayerProcessor& p) : proc (p)
{
    title.setText ("MIDI Remap", juce::dontSendNotification);
    title.setFont (theme::font (18.0f, true));
    addAndMakeVisible (title);

    hint.setText ("Input notes use the Toontrack (EZdrummer / Superior Drummer) map. The remap is applied to "
                  "playback, previews and every MIDI you drag into the DAW.",
                  juce::dontSendNotification);
    hint.setFont (theme::font (12.5f));
    hint.setColour (juce::Label::textColourId, theme::dim);
    hint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (hint);

    presetBox.onChange = [this]
    {
        auto i = presetBox.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (i, presets.size()))
            proc.setDrumMap (presets[i]);
    };
    addAndMakeVisible (presetBox);

    saveButton.onClick = [this] { saveAs(); };
    importButton.onClick = [this] { import(); };
    revealButton.onClick = [] { AppContext::getRemapFolder().createDirectory(); AppContext::getRemapFolder().startAsProcess(); };
    identityButton.onClick = [this] { proc.setDrumMap (tt::DrumMap()); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    changedOnly.onClick = [this] { refreshNotes(); };

    for (auto* b : { &saveButton, &importButton, &revealButton, &identityButton, &closeButton })
        addAndMakeVisible (*b);
    addAndMakeVisible (changedOnly);

    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, theme::row);
    addAndMakeVisible (list);

    proc.getDrumMapListeners().add (this);
    refreshPresets();
    refreshNotes();
}

RemapEditor::~RemapEditor()
{
    proc.getDrumMapListeners().remove (this);
    list.setModel (nullptr);
}

void RemapEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawRect (getLocalBounds());
}

void RemapEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (70));
    title.setBounds (top);
    r.removeFromTop (4);
    hint.setBounds (r.removeFromTop (34));
    r.removeFromTop (6);

    auto bar = r.removeFromTop (28);
    presetBox.setBounds (bar.removeFromLeft (juce::jmin (300, bar.getWidth() / 3)));
    bar.removeFromLeft (8);
    for (auto* b : { &saveButton, &importButton, &revealButton, &identityButton })
    {
        b->setBounds (bar.removeFromLeft (92));
        bar.removeFromLeft (6);
    }
    changedOnly.setBounds (bar);
    r.removeFromTop (8);
    list.setBounds (r);
}

void RemapEditor::refreshPresets()
{
    presets = proc.app->getAllRemapPresets();
    presetBox.clear (juce::dontSendNotification);

    int selected = 0;
    for (int i = 0; i < presets.size(); ++i)
    {
        presetBox.addItem (presets[i].name, i + 1);
        if (presets[i].map == proc.getDrumMap().map)
            selected = i + 1;
    }

    if (selected > 0)
        presetBox.setSelectedId (selected, juce::dontSendNotification);
    else
        presetBox.setText (proc.getDrumMap().name, juce::dontSendNotification);
}

void RemapEditor::refreshNotes()
{
    notes.clear();
    auto& m = proc.getDrumMap();
    for (int n = 0; n < 128; ++n)
        if (! changedOnly.getToggleState() || m.map[(size_t) n] != n)
            notes.push_back (n);

    list.updateContent();
    list.repaint();
}

juce::Component* RemapEditor::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (! juce::isPositiveAndBelow (row, (int) notes.size()))
    {
        delete existing;
        return nullptr;
    }

    auto* r = dynamic_cast<Row*> (existing);
    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }

    r->setNote (notes[(size_t) row]);
    return r;
}

void RemapEditor::setOutput (int inNote, int outNote)
{
    auto m = proc.getDrumMap();
    m.map[(size_t) inNote] = outNote;

    bool matchesPreset = false;
    for (auto& p : presets)
        if (p.map == m.map) { m.name = p.name; matchesPreset = true; break; }

    if (! matchesPreset && ! m.name.endsWith (" (edited)"))
        m.name << " (edited)";

    proc.setDrumMap (m);
}

void RemapEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshPresets();

    if (changedOnly.getToggleState())
        refreshNotes();
    else
    {
        for (int i = 0; i < getNumRows(); ++i)
            if (auto* r = dynamic_cast<Row*> (list.getComponentForRowNumber (i)))
                r->setNote (notes[(size_t) i]);
        list.repaint();
    }
}

void RemapEditor::saveAs()
{
    auto* w = new juce::AlertWindow ("Save remap preset", "Name of the new preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getDrumMap().name.upToFirstOccurrenceOf (" (edited)", false, false));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<RemapEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty())
            return;

        auto m = safe->proc.getDrumMap();
        m.name = name;
        if (m.saveToFile (AppContext::getRemapFolder().getChildFile (tt::makeSafeFileName (name) + ".xml")))
        {
            safe->proc.setDrumMap (m);
            safe->refreshPresets();
        }
    }), true);
}

void RemapEditor::import()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a GroovePlayer remap", AppContext::getRemapFolder(), "*.xml");
    juce::Component::SafePointer<RemapEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        tt::DrumMap m;
        if (! tt::DrumMap::loadFromFile (fc.getResult(), m))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed",
                                                    "That file is not a GroovePlayer remap.");
            return;
        }

        auto dest = AppContext::getRemapFolder().getChildFile (fc.getResult().getFileName());
        if (dest != fc.getResult())
            m.saveToFile (dest);

        safe->proc.setDrumMap (m);
        safe->refreshPresets();
    });
}
