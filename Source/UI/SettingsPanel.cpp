#include "SettingsPanel.h"

SettingsPanel::SettingsPanel (GroovePlayerProcessor& p) : proc (p)
{
    title.setText ("Settings", juce::dontSendNotification);
    title.setFont (theme::font (18.0f, true));
    addAndMakeVisible (title);

    libLabel.setText ("MIDI library folders (Toontrack format: \"NNN@NAME\" folders, header files and midiDB are understood; "
                      "any folder of .mid files works too)",
                      juce::dontSendNotification);
    libLabel.setFont (theme::font (12.5f));
    libLabel.setColour (juce::Label::textColourId, theme::dim);
    libLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (libLabel);

    rootList = proc.app->getLibraryRoots();
    roots.setModel (this);
    roots.setRowHeight (24);
    roots.setColour (juce::ListBox::backgroundColourId, theme::row);
    roots.setOutlineThickness (1);
    addAndMakeVisible (roots);

    addButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a MIDI library folder",
                                                        juce::File::getSpecialLocation (juce::File::userHomeDirectory));
        juce::Component::SafePointer<SettingsPanel> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (safe == nullptr || ! dir.isDirectory())
                return;

            safe->rootList.addIfNotAlreadyThere (dir.getFullPathName());
            safe->proc.app->setLibraryRoots (safe->rootList);
            safe->roots.updateContent();
        });
    };

    removeButton.onClick = [this]
    {
        auto row = roots.getSelectedRow();
        if (juce::isPositiveAndBelow (row, rootList.size()))
        {
            rootList.remove (row);
            proc.app->setLibraryRoots (rootList);
            roots.updateContent();
        }
    };

    rescanButton.onClick = [this] { proc.app->rescan(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    for (auto* b : { &addButton, &removeButton, &rescanButton, &closeButton })
        addAndMakeVisible (*b);

    status.setFont (theme::font (12.5f));
    status.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (status);

    tempoLabel.setText ("Tempo when the host provides none (e.g. Standalone):", juce::dontSendNotification);
    tempoLabel.setFont (theme::font (13.0f));
    addAndMakeVisible (tempoLabel);

    fallbackTempo.setSliderStyle (juce::Slider::LinearHorizontal);
    fallbackTempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 22);
    fallbackTempo.setRange (40.0, 300.0, 0.5);
    fallbackTempo.setValue (proc.engine.fallbackTempo.load(), juce::dontSendNotification);
    fallbackTempo.onValueChange = [this] { proc.engine.fallbackTempo.store (fallbackTempo.getValue()); };
    addAndMakeVisible (fallbackTempo);

    help.setText ("Using GroovePlayer in Cubase:\n"
                  "  1. Add GroovePlayer as an Instrument track.\n"
                  "  2. On your drum instrument's MIDI track set the input to \"GroovePlayer\" (MIDI out), "
                  "enable monitoring - or simply drag parts / the whole song from GroovePlayer onto the drum track.\n"
                  "  3. Playback follows the Cubase transport, tempo and cycle. Notes come out on the selected MIDI channel "
                  "with the remap applied.\n\n"
                  "Mouse: drag grooves onto the song track, Alt-drag to copy, drag a part's right edge to loop / shorten it, "
                  "Cmd+wheel to zoom, Backspace to delete, Cmd+D to duplicate, Cmd+Z to undo.",
                  juce::dontSendNotification);
    help.setFont (theme::font (12.5f));
    help.setColour (juce::Label::textColourId, theme::dim);
    help.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (help);

    timerCallback();
    startTimerHz (5);
}

SettingsPanel::~SettingsPanel()
{
    roots.setModel (nullptr);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawRect (getLocalBounds());
}

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (70));
    title.setBounds (top);
    r.removeFromTop (6);
    libLabel.setBounds (r.removeFromTop (34));
    roots.setBounds (r.removeFromTop (110));
    r.removeFromTop (6);

    auto bar = r.removeFromTop (26);
    addButton.setBounds (bar.removeFromLeft (110));
    bar.removeFromLeft (6);
    removeButton.setBounds (bar.removeFromLeft (80));
    bar.removeFromLeft (6);
    rescanButton.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (10);
    status.setBounds (bar);

    r.removeFromTop (16);
    auto t = r.removeFromTop (26);
    tempoLabel.setBounds (t.removeFromLeft (330));
    fallbackTempo.setBounds (t.removeFromLeft (300));
    r.removeFromTop (16);
    help.setBounds (r);
}

void SettingsPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    g.fillAll (selected ? theme::selection : (row % 2 ? theme::rowAlt : theme::row));
    auto exists = juce::File (rootList[row]).isDirectory();
    g.setColour (exists ? theme::text : juce::Colours::indianred);
    g.setFont (theme::font (13.0f));
    g.drawText (rootList[row] + (exists ? "" : "   (not found)"), 8, 0, w - 16, h, juce::Justification::centredLeft, true);
}

void SettingsPanel::timerCallback()
{
    status.setText (proc.app->getStatusText(), juce::dontSendNotification);
    rescanButton.setEnabled (! proc.app->isScanning());
}
