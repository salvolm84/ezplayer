#pragma once

#include "Theme.h"
#include "Core/RhythmIndex.h"
#include "Core/Groove.h"

// A tiny step sequencer (kick, snare, any cymbal) used as a query for the beat search.
class BeatSearchPanel : public juce::Component
{
public:
    BeatSearchPanel();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    const std::vector<tt::BarPrint>& getPattern() const { return pattern; }
    bool hasHits() const;
    int getNum() const { return num; }
    int getDen() const { return den; }
    int getSteps() const { return tt::RhythmPrint::stepsPerBar (num, den); }

    // A playable groove of the pattern (kick 36, snare 38, closed hat 42).
    tt::Groove::Ptr makeGroove() const;

    void setPlaying (bool);

    void setStatus (const juce::String& s) { if (s != status.getText()) status.setText (s, juce::dontSendNotification); }

    std::function<void()> onPatternChanged;
    std::function<void()> onPlay;

    // Persistence of the pattern in the plugin state.
    juce::String toString() const;
    void fromString (const juce::String&);

private:
    std::vector<tt::BarPrint> pattern { 1 };
    int num = 4, den = 4;
    bool paintValue = true;
    int lastCellRow = -1, lastCellStep = -1;

    juce::ComboBox meterBox, barsBox;
    juce::TextButton playButton { "Play" }, clearButton { "Clear" };
    juce::Label status;

    static constexpr int labelWidth = 62;

    juce::Rectangle<int> gridArea() const;
    bool cellAt (juce::Point<int>, int& row, int& step) const;
    bool getCell (int row, int step) const;
    void setCell (int row, int step, bool on);
    void setLayout (int newNum, int newDen, int bars);
    void changed();
};
