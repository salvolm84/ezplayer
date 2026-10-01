#include "TransportBar.h"
#include "DragTypes.h"

DragHandle::DragHandle()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag the whole song (with remap applied) onto a MIDI / instrument track in your DAW");
}

void DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (enabledForDrag ? theme::accentDim : theme::panelHi);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (enabledForDrag ? theme::accent : theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    auto grip = r.removeFromLeft (18.0f).reduced (5.0f, 7.0f);
    g.setColour (theme::text.withAlpha (enabledForDrag ? 0.9f : 0.4f));
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (grip.getX() + col * 5.0f, grip.getY() + row * (grip.getHeight() / 2.0f) - 1.0f, 2.5f, 2.5f);

    g.setFont (theme::font (12.5f, true));
    g.drawText ("Drag song to DAW", r.reduced (2.0f, 0.0f), juce::Justification::centredLeft, true);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (started || ! enabledForDrag || e.getDistanceFromDragStart() < 4)
        return;

    started = true;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        c->startDragging (drag::make (drag::song), this, juce::ScaledImage (createComponentSnapshot (getLocalBounds()), 1.0));
}

//==============================================================================
TransportBar::TransportBar (GroovePlayerProcessor& p) : proc (p)
{
    rewindButton.setTooltip ("Return to the song start");
    playButton.setTooltip ("Play / stop the song inside the plugin (when the DAW is stopped). "
                           "While the DAW plays, the song follows the DAW timeline.");
    loopButton.setTooltip ("Loop the song when playing inside the plugin");
    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (proc.engine.loopSong.load(), juce::dontSendNotification);
    loopButton.onClick = [this] { proc.setLoopSong (loopButton.getToggleState()); };
    rewindButton.onClick = [this] { if (onRewind) onRewind(); };
    playButton.onClick = [this] { if (onPlay) onPlay(); };

    for (auto* l : { &positionLabel, &tempoLabel })
    {
        l->setFont (theme::font (13.0f, true));
        l->setColour (juce::Label::textColourId, theme::text);
        l->setColour (juce::Label::backgroundColourId, theme::row);
        l->setColour (juce::Label::outlineColourId, theme::outline);
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*l);
    }

    snapBox.addItemList ({ "Snap: Bar", "Snap: Beat", "Snap: 1/8", "Snap: Off" }, 1);
    snapBox.setSelectedId (1, juce::dontSendNotification);
    snapBox.onChange = [this] { if (onSnapChanged) onSnapChanged (snapBox.getSelectedId()); };
    snapBox.setTooltip ("Grid used when dropping, moving and resizing parts");

    zoomOut.onClick = [this] { if (onZoom) onZoom (0.8); };
    zoomIn.onClick = [this] { if (onZoom) onZoom (1.25); };
    undoButton.onClick = [this] { if (onUndo) onUndo(); };
    redoButton.onClick = [this] { if (onRedo) onRedo(); };
    duplicateButton.onClick = [this] { if (onDuplicate) onDuplicate(); };
    deleteButton.onClick = [this] { if (onDelete) onDelete(); };
    clearButton.onClick = [this] { if (onClear) onClear(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };

    duplicateButton.setTooltip ("Duplicate the selected parts (Cmd+D)");
    deleteButton.setTooltip ("Delete the selected parts (Backspace)");
    clearButton.setTooltip ("Remove every part from the song");
    exportButton.setTooltip ("Save the song as a MIDI file (remap applied)");

    for (juce::Component* c : { (juce::Component*) &rewindButton, (juce::Component*) &playButton, (juce::Component*) &loopButton,
                                (juce::Component*) &snapBox, (juce::Component*) &zoomOut, (juce::Component*) &zoomIn,
                                (juce::Component*) &undoButton, (juce::Component*) &redoButton, (juce::Component*) &duplicateButton,
                                (juce::Component*) &deleteButton, (juce::Component*) &clearButton, (juce::Component*) &exportButton,
                                (juce::Component*) &songHandle })
        addAndMakeVisible (c);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.fillRect (0, 0, getWidth(), 1);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (6, 5);
    auto place = [&r] (juce::Component& c, int w, int gap = 4)
    {
        c.setBounds (r.removeFromLeft (w));
        r.removeFromLeft (gap);
    };

    place (rewindButton, 34);
    place (playButton, 62);
    place (loopButton, 50, 10);
    place (positionLabel, 92);
    place (tempoLabel, 168, 10);
    place (snapBox, 106);
    place (zoomOut, 26, 2);
    place (zoomIn, 26, 10);

    songHandle.setBounds (r.removeFromRight (150));
    r.removeFromRight (6);
    exportButton.setBounds (r.removeFromRight (106));
    r.removeFromRight (10);

    auto compact = r.getWidth() < 330;
    place (undoButton, compact ? 44 : 50);
    place (redoButton, compact ? 44 : 50, 8);
    place (duplicateButton, compact ? 66 : 76);
    place (deleteButton, compact ? 50 : 58);
    place (clearButton, compact ? 44 : 50);
}

void TransportBar::update()
{
    auto& e = proc.engine;
    auto hostPlaying = e.hostPlaying.load();
    auto internal = e.internalPlaying.load();

    auto pos = e.songPosition.load();
    auto num = juce::jmax (1, e.hostSigNum.load());
    auto den = juce::jmax (1, e.hostSigDen.load());
    auto beatLen = 4.0 / den;
    auto barLen = num * beatLen;
    auto bar = (int) std::floor (pos / barLen + 1.0e-9);
    auto beat = (int) std::floor ((pos - bar * barLen) / beatLen + 1.0e-9);

    positionLabel.setText (juce::String (juce::jmax (1, bar + 1)) + " . " + juce::String (juce::jmax (1, beat + 1)),
                           juce::dontSendNotification);

    tempoLabel.setText (juce::String (e.currentTempo.load(), 1) + " BPM  " + juce::String (num) + "/" + juce::String (den)
                            + (hostPlaying ? "  DAW" : internal ? "  INT" : ""),
                        juce::dontSendNotification);
    tempoLabel.setColour (juce::Label::textColourId, hostPlaying ? theme::accent : theme::text);

    playButton.setButtonText (internal ? "Stop" : "Play");
    playButton.setToggleState (internal, juce::dontSendNotification);
    playButton.setEnabled (! hostPlaying);

    undoButton.setEnabled (proc.song.canUndo());
    redoButton.setEnabled (proc.song.canRedo());

    auto hasSong = ! proc.song.getClips().empty();
    clearButton.setEnabled (hasSong);
    exportButton.setEnabled (hasSong);
    if (songHandle.enabledForDrag != hasSong)
    {
        songHandle.enabledForDrag = hasSong;
        songHandle.repaint();
    }
}
