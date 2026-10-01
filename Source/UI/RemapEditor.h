#pragma once

#include "Theme.h"
#include "PluginProcessor.h"

// Note remap editor: choose a preset or edit any input note's output note.
class RemapEditor : public juce::Component,
                    private juce::ListBoxModel,
                    private juce::ChangeListener
{
public:
    explicit RemapEditor (GroovePlayerProcessor&);
    ~RemapEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onClose;

private:
    class Row;

    GroovePlayerProcessor& proc;
    juce::Label title, hint;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save as..." }, importButton { "Import..." }, revealButton { "Show folder" },
                     identityButton { "Reset" }, closeButton { "Done" };
    juce::ToggleButton changedOnly { "Only remapped notes" };
    juce::ListBox list;
    std::vector<int> notes;
    juce::Array<tt::DrumMap> presets;
    std::unique_ptr<juce::FileChooser> chooser;

    void refreshPresets();
    void refreshNotes();
    void setOutput (int inNote, int outNote);
    void saveAs();
    void import();

    int getNumRows() override { return (int) notes.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
};
