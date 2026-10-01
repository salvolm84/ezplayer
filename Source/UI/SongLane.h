#pragma once

#include "Theme.h"
#include "PluginProcessor.h"

#include <set>

// Single-lane song track. Grooves are dropped from the browser (or Finder), moved,
// copied (Alt), resized from the right edge (loops the groove) and deleted. Clips can
// be dragged out of the plugin straight into the DAW.
class SongLane : public juce::Component,
                 public juce::DragAndDropTarget,
                 public juce::FileDragAndDropTarget,
                 private juce::ScrollBar::Listener,
                 private juce::ChangeListener
{
public:
    explicit SongLane (GroovePlayerProcessor&);
    ~SongLane() override;

    enum SnapMode { snapBar = 1, snapBeat, snapEighth, snapOff };
    void setSnapMode (int m);
    int getSnapMode() const { return snapMode; }

    void deleteSelected();
    void duplicateSelected();
    void selectAll();
    void addGrooveAtEnd (int grooveIndex);
    void zoomBy (double factor);
    void updatePlayhead();

    std::vector<int> getSelection() const { return { selection.begin(), selection.end() }; }

    std::function<void (tt::Groove::Ptr)> onPreviewGroove;
    std::function<void (const juce::File&)> onExportClipsRequest;
    std::function<void()> onTogglePlay;

    // Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;
    bool keyPressed (const juce::KeyPress&) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    GroovePlayerProcessor& proc;
    juce::ScrollBar hScroll { false };

    double pixelsPerBeat = 9.0;
    double viewStart = 0.0;
    int snapMode = snapBar;
    std::set<int> selection;

    enum class Mode { none, seek, resize, pendingMove, dragging };
    Mode mode = Mode::none;
    juce::Point<int> downPos;
    int resizeId = 0;
    double resizeLength = 0.0;
    double grabOffset = 0.0;    // mouse beat - selection start when a move begins

    bool ghostVisible = false;
    double ghostStart = 0.0, ghostLength = 0.0;
    int ghostGrooveIndex = -1;

    double playhead = 0.0;
    bool lastPlaying = false;

    static constexpr int rulerHeight = 22;

    juce::Rectangle<int> laneArea() const;
    double xToBeat (float x) const { return viewStart + (x - (float) laneArea().getX()) / pixelsPerBeat; }
    float beatToX (double b) const { return (float) laneArea().getX() + (float) ((b - viewStart) * pixelsPerBeat); }
    double gridSize() const;
    double snapNearest (double beat) const;
    double snapFloor (double beat) const;
    double visibleBeats() const { return laneArea().getWidth() / pixelsPerBeat; }

    const tt::SongClip* clipAt (juce::Point<int>) const;
    juce::Rectangle<float> clipBounds (const tt::SongClip&) const;
    double selectionStart() const;
    double selectionEnd() const;
    double lengthForDragSource (const SourceDetails&);

    void setViewStart (double beats);
    void updateScrollBar();
    void ensureVisible (double beat);
    void showClipMenu (const tt::SongClip*);
    void startClipDrag();

    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongLane)
};
