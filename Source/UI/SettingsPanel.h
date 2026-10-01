#pragma once

#include "Theme.h"
#include "PluginProcessor.h"

class SettingsPanel : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    explicit SettingsPanel (GroovePlayerProcessor&);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onClose;

private:
    GroovePlayerProcessor& proc;
    juce::Label title, libLabel, status, tempoLabel, help;
    juce::ListBox roots;
    juce::StringArray rootList;
    juce::TextButton addButton { "Add folder..." }, removeButton { "Remove" }, rescanButton { "Rescan library" },
                     closeButton { "Done" };
    juce::Slider fallbackTempo;
    std::unique_ptr<juce::FileChooser> chooser;

    int getNumRows() override { return rootList.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void timerCallback() override;
};
