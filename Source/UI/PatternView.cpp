#include "PatternView.h"

PatternView::PatternView()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto the song track, or out of the plugin into your DAW");
}

void PatternView::setGroove (tt::Groove::Ptr g, int index)
{
    groove = std::move (g);
    grooveIndex = index;
    message.clear();
    repaint();
}

void PatternView::setPlayhead (double beat)
{
    if (std::abs (beat - playhead) > 1.0e-4)
    {
        playhead = beat;
        repaint();
    }
}

void PatternView::drawPattern (juce::Graphics& g, juce::Rectangle<float> area, const tt::Groove& groove,
                               double offset, double length, bool withLabels, bool showBars)
{
    using tt::KitPiece;
    static const KitPiece order[] = { KitPiece::crash, KitPiece::ride, KitPiece::hihat, KitPiece::hihatPedal,
                                      KitPiece::toms, KitPiece::snare, KitPiece::sidestick, KitPiece::kick, KitPiece::other };

    if (length <= 0.0 || area.isEmpty())
        return;

    bool used[(int) KitPiece::count] = {};
    for (auto& n : groove.notes)
        used[(int) tt::kitPieceForNote (n.note)] = true;

    int rowOf[(int) KitPiece::count];
    int numRows = 0;
    for (auto p : order)
        rowOf[(int) p] = used[(int) p] ? numRows++ : -1;

    if (numRows == 0)
        return;

    auto labels = withLabels ? area.removeFromLeft (64.0f) : juce::Rectangle<float>();
    const auto rowH = area.getHeight() / (float) numRows;
    const auto pxPerBeat = area.getWidth() / (float) length;

    if (withLabels)
    {
        g.setFont (theme::font (juce::jlimit (9.0f, 12.0f, rowH * 0.8f)));
        for (auto p : order)
        {
            if (rowOf[(int) p] < 0)
                continue;
            auto r = labels.withHeight (rowH).withY (area.getY() + rowOf[(int) p] * rowH);
            g.setColour (theme::pieceColour (p).withAlpha (0.85f));
            g.drawText (tt::kitPieceName (p), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
        }

        g.setColour (theme::outline.withAlpha (0.35f));
        for (int r = 1; r < numRows; ++r)
            g.fillRect (area.getX(), area.getY() + r * rowH, area.getWidth(), 1.0f);
    }

    if (showBars)
    {
        auto bar = groove.getBarLengthBeats();
        for (double b = std::ceil (offset / bar) * bar; b < offset + length; b += bar)
        {
            auto x = area.getX() + (float) ((b - offset) * pxPerBeat);
            g.setColour (theme::outline.withAlpha (b == 0.0 ? 0.0f : 0.8f));
            g.fillRect (x, area.getY(), 1.0f, area.getHeight());
        }

        if (pxPerBeat > 8.0f)
        {
            g.setColour (theme::outline.withAlpha (0.3f));
            for (double b = std::ceil (offset); b < offset + length; b += 1.0)
                g.fillRect (area.getX() + (float) ((b - offset) * pxPerBeat), area.getY(), 1.0f, area.getHeight());
        }
    }

    const auto L = groove.lengthBeats;
    const auto noteW = juce::jlimit (1.5f, 5.0f, pxPerBeat * 0.12f);
    const auto noteH = juce::jmax (1.5f, rowH * 0.72f);

    for (int rep = (int) std::floor (offset / L); rep * L < offset + length; ++rep)
    {
        for (auto& n : groove.notes)
        {
            auto pos = rep * L + n.start - offset;
            if (pos < 0.0 || pos >= length)
                continue;

            auto piece = tt::kitPieceForNote (n.note);
            auto row = rowOf[(int) piece];
            auto x = area.getX() + (float) (pos * pxPerBeat);
            auto y = area.getY() + row * rowH + (rowH - noteH) * 0.5f;
            g.setColour (theme::pieceColour (piece).withAlpha (0.35f + 0.65f * (n.velocity / 127.0f)));
            g.fillRect (x, y, noteW, noteH);
        }
    }
}

void PatternView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::row);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    auto inner = r.reduced (6.0f, 6.0f);

    if (groove == nullptr)
    {
        g.setColour (theme::dim);
        g.setFont (theme::font (13.0f));
        g.drawText (message.isNotEmpty() ? message : juce::String ("Select a groove to preview it"),
                    inner, juce::Justification::centred, true);
        return;
    }

    drawPattern (g, inner, *groove, 0.0, groove->lengthBeats, true, true);

    if (playhead >= 0.0)
    {
        auto grid = inner.withTrimmedLeft (64.0f);
        auto x = grid.getX() + (float) (playhead / groove->lengthBeats) * grid.getWidth();
        g.setColour (theme::playhead);
        g.fillRect (x, inner.getY(), 1.5f, inner.getHeight());
    }
}

void PatternView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && groove != nullptr && e.getDistanceFromDragStart() > 4 && onDragOut)
    {
        dragStarted = true;
        onDragOut (e);
    }
}

void PatternView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}
