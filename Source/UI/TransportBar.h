#pragma once

#include "Theme.h"
#include "PluginProcessor.h"

// A grip that starts a drag of the whole song (drop it on a DAW track).
class DragHandle : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    DragHandle();
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    bool enabledForDrag = true;

private:
    bool started = false;
};

class TransportBar : public juce::Component
{
public:
    explicit TransportBar (GroovePlayerProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;
    void update();

    std::function<void()> onPlay, onRewind, onUndo, onRedo, onDelete, onDuplicate, onClear, onExport;
    std::function<void (double)> onZoom;
    std::function<void (int)> onSnapChanged;

    void setSnap (int id) { snapBox.setSelectedId (id, juce::dontSendNotification); }

private:
    GroovePlayerProcessor& proc;

    juce::TextButton rewindButton { "|<" }, playButton { "Play" }, loopButton { "Loop" };
    juce::Label positionLabel, tempoLabel;
    juce::ComboBox snapBox;
    juce::TextButton zoomOut { "-" }, zoomIn { "+" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, duplicateButton { "Duplicate" },
                     deleteButton { "Delete" }, clearButton { "Clear" }, exportButton { "Export MIDI..." };
    DragHandle songHandle;
};
