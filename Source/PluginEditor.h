#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/BrowserComponent.h"
#include "UI/SongLane.h"
#include "UI/TransportBar.h"

class GroovePlayerEditor : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer,
                           private juce::Timer,
                           private juce::ChangeListener
{
public:
    explicit GroovePlayerEditor (GroovePlayerProcessor&);
    ~GroovePlayerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    // Turns internal drags that leave the plugin window into MIDI file drags.
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

private:
    class Overlay;

    GroovePlayerProcessor& proc;
    GPLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Label titleLabel, velocityLabel;
    juce::Slider velocity;
    juce::ComboBox channelBox;
    juce::ToggleButton thruToggle { "MIDI thru" };
    juce::TextButton remapButton, settingsButton { "Settings" };

    BrowserComponent browser;
    TransportBar transport;
    SongLane lane;
    std::unique_ptr<Overlay> overlay;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::AudioProcessorValueTreeState::SliderAttachment velocityAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment thruAttachment;

    void showOverlay (std::unique_ptr<juce::Component>, int maxW, int maxH);
    void closeOverlay();
    void togglePlay();
    void exportMidi (bool selectionOnly);
    void updateRemapButton();

    std::vector<tt::TimelineNote> notesForDrag (const juce::var& description, double& length, juce::String& name);

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovePlayerEditor)
};
