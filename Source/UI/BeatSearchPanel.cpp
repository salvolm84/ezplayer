#include "BeatSearchPanel.h"

namespace
{
    struct Meter { const char* name; int num, den; };
    const Meter meters[] = { { "4/4", 4, 4 }, { "3/4", 3, 4 }, { "6/8", 6, 8 }, { "12/8", 12, 8 },
                             { "5/4", 5, 4 }, { "7/8", 7, 8 }, { "2/4", 2, 4 } };

    const char* rowNames[] = { "Cymbal", "Snare", "Kick" };
    const tt::KitPiece rowPieces[] = { tt::KitPiece::ride, tt::KitPiece::snare, tt::KitPiece::kick };

    juce::uint32 tt::BarPrint::* voiceFor (int row)
    {
        switch (row)
        {
            case 0:  return &tt::BarPrint::cymbal;
            case 1:  return &tt::BarPrint::snare;
            default: return &tt::BarPrint::kick;
        }
    }
}

BeatSearchPanel::BeatSearchPanel()
{
    for (int i = 0; i < (int) std::size (meters); ++i)
        meterBox.addItem (meters[i].name, i + 1);
    meterBox.setSelectedId (1, juce::dontSendNotification);
    meterBox.setTooltip ("Meter of the pattern; only grooves in the same meter are searched");
    meterBox.onChange = [this]
    {
        auto& m = meters[meterBox.getSelectedId() - 1];
        setLayout (m.num, m.den, (int) pattern.size());
    };

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.setSelectedId (1, juce::dontSendNotification);
    barsBox.onChange = [this] { setLayout (num, den, barsBox.getSelectedId()); };

    playButton.setTooltip ("Listen to the pattern (edits are heard while it plays)");
    playButton.onClick = [this] { if (onPlay) onPlay(); };
    clearButton.onClick = [this]
    {
        for (auto& b : pattern) b = {};
        changed();
    };

    status.setFont (theme::font (12.0f));
    status.setColour (juce::Label::textColourId, theme::dim);
    status.setJustificationType (juce::Justification::centredRight);

    for (auto* c : { (juce::Component*) &meterBox, (juce::Component*) &barsBox, (juce::Component*) &playButton,
                     (juce::Component*) &clearButton, (juce::Component*) &status })
        addAndMakeVisible (c);
}

bool BeatSearchPanel::hasHits() const
{
    for (auto& b : pattern)
        if (! b.isEmpty())
            return true;
    return false;
}

juce::Rectangle<int> BeatSearchPanel::gridArea() const
{
    return getLocalBounds().reduced (6).withTrimmedTop (30).withTrimmedLeft (labelWidth);
}

bool BeatSearchPanel::cellAt (juce::Point<int> p, int& row, int& step) const
{
    auto g = gridArea();
    if (! g.contains (p))
        return false;

    auto totalSteps = getSteps() * (int) pattern.size();
    row = juce::jlimit (0, 2, (p.y - g.getY()) * 3 / juce::jmax (1, g.getHeight()));
    step = juce::jlimit (0, totalSteps - 1, (p.x - g.getX()) * totalSteps / juce::jmax (1, g.getWidth()));
    return true;
}

bool BeatSearchPanel::getCell (int row, int step) const
{
    auto steps = getSteps();
    auto& bar = pattern[(size_t) (step / steps)];
    return ((bar.*voiceFor (row)) >> (step % steps)) & 1u;
}

void BeatSearchPanel::setCell (int row, int step, bool on)
{
    auto steps = getSteps();
    auto& v = pattern[(size_t) (step / steps)].*voiceFor (row);
    auto bit = 1u << (step % steps);
    v = on ? (v | bit) : (v & ~bit);
}

void BeatSearchPanel::setLayout (int newNum, int newDen, int bars)
{
    if (newNum != num || newDen != den)
        pattern.assign ((size_t) bars, {});   // grid changed: start over
    else
        pattern.resize ((size_t) bars);

    num = newNum;
    den = newDen;
    changed();
}

void BeatSearchPanel::changed()
{
    repaint();
    if (onPatternChanged)
        onPatternChanged();
}

void BeatSearchPanel::mouseDown (const juce::MouseEvent& e)
{
    int row, step;
    if (! cellAt (e.getPosition(), row, step))
        return;

    paintValue = ! getCell (row, step);
    setCell (row, step, paintValue);
    lastCellRow = row;
    lastCellStep = step;
    changed();
}

void BeatSearchPanel::mouseDrag (const juce::MouseEvent& e)
{
    int row, step;
    if (! cellAt (e.getPosition(), row, step) || (row == lastCellRow && step == lastCellStep))
        return;

    lastCellRow = row;
    lastCellStep = step;
    if (getCell (row, step) != paintValue)
    {
        setCell (row, step, paintValue);
        changed();
    }
}

void BeatSearchPanel::resized()
{
    auto top = getLocalBounds().reduced (6).removeFromTop (24);
    meterBox.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    barsBox.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (6);
    playButton.setBounds (top.removeFromLeft (56));
    top.removeFromLeft (6);
    clearButton.setBounds (top.removeFromLeft (56));
    status.setBounds (top);
}

void BeatSearchPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawRect (getLocalBounds());

    auto grid = gridArea();
    const auto steps = getSteps();
    const auto totalSteps = steps * (int) pattern.size();
    const auto cellW = (float) grid.getWidth() / (float) totalSteps;
    const auto rowH = (float) grid.getHeight() / 3.0f;
    const auto stepsPerBeat = den == 8 ? 6 : 4; // group 8th-based meters in dotted quarters

    g.setFont (theme::font (12.0f, true));
    for (int r = 0; r < 3; ++r)
    {
        auto y = (float) grid.getY() + r * rowH;
        g.setColour (theme::pieceColour (rowPieces[r]));
        g.drawText (rowNames[r], juce::Rectangle<float> (6.0f, y, (float) labelWidth - 10.0f, rowH),
                    juce::Justification::centredLeft, false);

        for (int s = 0; s < totalSteps; ++s)
        {
            auto cell = juce::Rectangle<float> ((float) grid.getX() + s * cellW, y, cellW, rowH).reduced (1.5f);
            auto beatShade = ((s % steps) / stepsPerBeat) % 2 == 0;
            g.setColour (beatShade ? theme::row : theme::rowAlt.brighter (0.06f));
            g.fillRoundedRectangle (cell, 2.0f);

            if (getCell (r, s))
            {
                g.setColour (theme::pieceColour (rowPieces[r]));
                g.fillRoundedRectangle (cell.reduced (2.0f), 2.0f);
            }
        }
    }

    // Bar separators
    g.setColour (theme::accent.withAlpha (0.7f));
    for (size_t b = 1; b < pattern.size(); ++b)
        g.fillRect ((float) grid.getX() + (float) (b * (size_t) steps) * cellW - 1.0f, (float) grid.getY(), 2.0f, (float) grid.getHeight());
}

void BeatSearchPanel::setPlaying (bool playing)
{
    playButton.setButtonText (playing ? "Stop" : "Play");
    playButton.setToggleState (playing, juce::dontSendNotification);
}

tt::Groove::Ptr BeatSearchPanel::makeGroove() const
{
    tt::Groove::Ptr gr = new tt::Groove();
    gr->name = "Beat search pattern";
    gr->num = num;
    gr->den = den;
    gr->hasTimeSig = true;

    const auto steps = getSteps();
    const auto stepLen = (num * 4.0 / den) / steps;
    gr->lengthBeats = stepLen * steps * (double) pattern.size();

    static const int notes[] = { 42, 38, 36 };
    for (int s = 0; s < steps * (int) pattern.size(); ++s)
        for (int r = 0; r < 3; ++r)
            if (getCell (r, s))
                gr->notes.push_back ({ s * stepLen, stepLen * 0.5, (juce::uint8) notes[r], (juce::uint8) (r == 0 ? 90 : 110) });

    return gr;
}

juce::String BeatSearchPanel::toString() const
{
    juce::StringArray parts { juce::String (num), juce::String (den) };
    for (auto& b : pattern)
        parts.add (juce::String::toHexString ((juce::int64) b.kick) + ":" + juce::String::toHexString ((juce::int64) b.snare)
                   + ":" + juce::String::toHexString ((juce::int64) b.cymbal));
    return parts.joinIntoString (",");
}

void BeatSearchPanel::fromString (const juce::String& s)
{
    juce::StringArray parts;
    parts.addTokens (s, ",", {});
    if (parts.size() < 3)
        return;

    num = juce::jlimit (1, 32, parts[0].getIntValue());
    den = juce::jlimit (1, 32, parts[1].getIntValue());
    pattern.clear();

    for (int i = 2; i < juce::jmin (parts.size(), 4); ++i)
    {
        juce::StringArray v;
        v.addTokens (parts[i], ":", {});
        tt::BarPrint b;
        b.kick = (juce::uint32) v[0].getHexValue64();
        b.snare = (juce::uint32) v[1].getHexValue64();
        b.cymbal = (juce::uint32) v[2].getHexValue64();
        pattern.push_back (b);
    }

    for (int i = 0; i < (int) std::size (meters); ++i)
        if (meters[i].num == num && meters[i].den == den)
            meterBox.setSelectedId (i + 1, juce::dontSendNotification);
    barsBox.setSelectedId ((int) pattern.size(), juce::dontSendNotification);
    repaint();
}
