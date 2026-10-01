#pragma once

#include "Theme.h"
#include "Core/Groove.h"

// Read-only drum grid of a groove: one row per kit piece, bar lines and playhead.
class PatternView : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    PatternView();

    void setGroove (tt::Groove::Ptr, int grooveIndex);
    tt::Groove::Ptr getGroove() const { return groove; }
    int getGrooveIndex() const { return grooveIndex; }

    void setPlayhead (double beatOrMinusOne);
    void setMessage (const juce::String& m) { message = m; repaint(); }

    // Draws `length` beats of `g` starting at `offset` (loops when longer than the groove).
    static void drawPattern (juce::Graphics&, juce::Rectangle<float> area, const tt::Groove& g,
                             double offset, double length, bool withLabels, bool showBars);

    std::function<void (const juce::MouseEvent&)> onDragOut;
    std::function<void()> onDoubleClick;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    tt::Groove::Ptr groove;
    int grooveIndex = -1;
    double playhead = -1.0;
    juce::String message;
    bool dragStarted = false;

    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }
};
