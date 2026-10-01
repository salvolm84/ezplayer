#include "SongLane.h"
#include "DragTypes.h"
#include "PatternView.h"

SongLane::SongLane (GroovePlayerProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    hScroll.setAutoHide (false);
    hScroll.addListener (this);
    addAndMakeVisible (hScroll);

    pixelsPerBeat = (double) proc.uiState.getProperty ("zoom", 9.0);
    snapMode = (int) proc.uiState.getProperty ("snap", (int) snapBar);
    proc.song.addChangeListener (this);
}

SongLane::~SongLane()
{
    proc.song.removeChangeListener (this);
}

void SongLane::setSnapMode (int m)
{
    snapMode = juce::jlimit ((int) snapBar, (int) snapOff, m);
    proc.uiState.setProperty ("snap", snapMode, nullptr);
}

juce::Rectangle<int> SongLane::laneArea() const
{
    return getLocalBounds().withTrimmedTop (rulerHeight).withTrimmedBottom (12);
}

double SongLane::gridSize() const
{
    switch (snapMode)
    {
        case snapBar:    return proc.getBarLengthBeats();
        case snapBeat:   return 1.0;
        case snapEighth: return 0.5;
        default:         return 0.0;
    }
}

double SongLane::snapNearest (double beat) const
{
    auto g = gridSize();
    return juce::jmax (0.0, g > 0.0 ? std::round (beat / g) * g : beat);
}

double SongLane::snapFloor (double beat) const
{
    auto g = gridSize();
    return juce::jmax (0.0, g > 0.0 ? std::floor (beat / g + 1.0e-9) * g : beat);
}

juce::Rectangle<float> SongLane::clipBounds (const tt::SongClip& c) const
{
    auto lane = laneArea().toFloat().reduced (0.0f, 5.0f);
    auto x0 = beatToX (c.start), x1 = beatToX (c.end());
    return { x0, lane.getY(), juce::jmax (2.0f, x1 - x0), lane.getHeight() };
}

const tt::SongClip* SongLane::clipAt (juce::Point<int> p) const
{
    for (auto& c : proc.song.getClips())
        if (clipBounds (c).contains (p.toFloat()))
            return &c;
    return nullptr;
}

double SongLane::selectionStart() const
{
    double s = 1.0e300;
    for (auto& c : proc.song.getClips())
        if (selection.count (c.id))
            s = juce::jmin (s, c.start);
    return s;
}

double SongLane::selectionEnd() const
{
    double e = 0.0;
    for (auto& c : proc.song.getClips())
        if (selection.count (c.id))
            e = juce::jmax (e, c.end());
    return e;
}

//==============================================================================
void SongLane::paint (juce::Graphics& g)
{
    auto lane = laneArea();
    auto ruler = getLocalBounds().removeFromTop (rulerHeight);
    const auto bar = proc.getBarLengthBeats();

    g.fillAll (theme::panel);
    g.setColour (theme::panelHi);
    g.fillRect (ruler);

    // Bars: shading, grid lines and numbers.
    auto firstBar = (int) std::floor (viewStart / bar);
    auto lastBar = (int) std::ceil ((viewStart + visibleBeats()) / bar);
    auto barPx = bar * pixelsPerBeat;
    auto labelEvery = barPx >= 28 ? 1 : barPx >= 14 ? 2 : barPx >= 7 ? 4 : 8;

    g.setFont (theme::font (11.0f));
    for (int b = juce::jmax (0, firstBar); b <= lastBar; ++b)
    {
        auto x = beatToX (b * bar);
        if (b % 2 == 1)
        {
            g.setColour (theme::row.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (x, (float) lane.getY(), (float) barPx, (float) lane.getHeight()));
        }

        g.setColour (theme::outline.withAlpha (0.7f));
        g.fillRect (x, (float) lane.getY(), 1.0f, (float) lane.getHeight());
        g.fillRect (x, (float) ruler.getBottom() - (b % labelEvery == 0 ? 8.0f : 4.0f), 1.0f, 8.0f);

        if (b % labelEvery == 0)
        {
            g.setColour (theme::dim);
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 3.0f, 0.0f, 40.0f, (float) rulerHeight - 6.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    if (pixelsPerBeat >= 12.0)
    {
        g.setColour (theme::outline.withAlpha (0.25f));
        for (auto beat = std::ceil (viewStart); beat < viewStart + visibleBeats(); beat += 1.0)
            g.fillRect (beatToX (beat), (float) lane.getY(), 1.0f, (float) lane.getHeight());
    }

    // Clips
    for (auto& c : proc.song.getClips())
    {
        auto r = clipBounds (c);
        if (c.id == resizeId && mode == Mode::resize)
            r.setRight (beatToX (c.start + resizeLength));

        if (r.getRight() < (float) lane.getX() || r.getX() > (float) lane.getRight())
            continue;

        auto sel = selection.count (c.id) > 0;
        g.setColour (sel ? theme::clipSel : theme::clip);
        g.fillRoundedRectangle (r, 4.0f);

        auto shownLength = (c.id == resizeId && mode == Mode::resize) ? resizeLength : c.length;
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());

        if (c.groove != nullptr)
        {
            // Loop seams when the clip is longer than its groove
            auto L = c.groove->lengthBeats;
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (auto seam = std::floor (c.offset / L + 1.0) * L; seam < c.offset + shownLength - 1.0e-6; seam += L)
                g.fillRect (beatToX (c.start + seam - c.offset), r.getY(), 1.5f, r.getHeight());

            PatternView::drawPattern (g, r.reduced (3.0f, 3.0f).withTrimmedTop (16.0f), *c.groove, c.offset, shownLength, false, false);

            g.setColour (sel ? juce::Colours::white : theme::text);
            g.setFont (theme::font (12.0f, true));
            g.drawText (c.groove->name, r.reduced (5.0f, 2.0f).removeFromTop (14.0f), juce::Justification::centredLeft, true);
        }

        g.restoreState();
        g.setColour (sel ? theme::accent : theme::outline.brighter (0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, sel ? 1.5f : 1.0f);
    }

    if (ghostVisible)
    {
        auto lr = lane.toFloat().reduced (0.0f, 5.0f);
        auto x0 = beatToX (ghostStart), x1 = beatToX (ghostStart + ghostLength);
        juce::Rectangle<float> r (x0, lr.getY(), juce::jmax (4.0f, x1 - x0), lr.getHeight());
        g.setColour (theme::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::accent);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
    }

    if (proc.song.getClips().empty() && ! ghostVisible)
    {
        g.setColour (theme::dim);
        g.setFont (theme::font (13.0f));
        g.drawText ("Drag grooves here from the browser (or double-click a groove) to build a song. "
                    "Drag clips or the song out of the plugin to drop MIDI into your DAW.",
                    lane.reduced (12, 0), juce::Justification::centred, true);
    }

    // Playhead
    auto px = beatToX (playhead);
    if (px >= (float) lane.getX() && px <= (float) lane.getRight())
    {
        g.setColour (theme::playhead);
        g.fillRect (px - 0.5f, (float) ruler.getY(), 1.5f, (float) lane.getBottom() - (float) ruler.getY());
        juce::Path tri;
        tri.addTriangle (px - 5.0f, (float) ruler.getBottom() - 8.0f, px + 5.0f, (float) ruler.getBottom() - 8.0f, px, (float) ruler.getBottom());
        g.fillPath (tri);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (theme::accent.withAlpha (0.4f));
        g.drawRect (getLocalBounds(), 1);
    }
}

void SongLane::resized()
{
    hScroll.setBounds (getLocalBounds().removeFromBottom (12));
    updateScrollBar();
}

void SongLane::updateScrollBar()
{
    auto total = juce::jmax (proc.song.getEndBeat() + proc.getBarLengthBeats() * 16.0, viewStart + visibleBeats());
    hScroll.setRangeLimits (0.0, total, juce::dontSendNotification);
    hScroll.setCurrentRange (viewStart, visibleBeats(), juce::dontSendNotification);
}

void SongLane::setViewStart (double b)
{
    viewStart = juce::jmax (0.0, b);
    updateScrollBar();
    repaint();
}

void SongLane::scrollBarMoved (juce::ScrollBar*, double newStart)
{
    viewStart = juce::jmax (0.0, newStart);
    repaint();
}

void SongLane::ensureVisible (double beat)
{
    if (beat < viewStart || beat > viewStart + visibleBeats() * 0.95)
        setViewStart (beat - visibleBeats() * 0.1);
}

void SongLane::zoomBy (double factor)
{
    auto centre = viewStart + visibleBeats() * 0.5;
    pixelsPerBeat = juce::jlimit (1.5, 80.0, pixelsPerBeat * factor);
    proc.uiState.setProperty ("zoom", pixelsPerBeat, nullptr);
    setViewStart (centre - visibleBeats() * 0.5);
}

void SongLane::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto it = selection.begin(); it != selection.end();)
        it = proc.song.find (*it) == nullptr ? selection.erase (it) : std::next (it);

    updateScrollBar();
    repaint();
}

void SongLane::updatePlayhead()
{
    auto pos = proc.engine.songPosition.load();
    auto playing = proc.engine.hostPlaying.load() || proc.engine.internalPlaying.load();

    if (std::abs (pos - playhead) > 1.0e-3)
    {
        playhead = pos;
        if (playing && mode == Mode::none && ! ghostVisible)
            if (playhead > viewStart + visibleBeats() * 0.92 || playhead < viewStart)
                setViewStart (playhead - visibleBeats() * 0.05);
        repaint();
    }

    lastPlaying = playing;
}

//==============================================================================
void SongLane::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    downPos = e.getPosition();
    mode = Mode::none;

    if (e.y < rulerHeight)
    {
        mode = Mode::seek;
        auto b = snapNearest (xToBeat ((float) e.x));
        proc.engine.seek (b);
        playhead = b;
        repaint();
        return;
    }

    auto* c = clipAt (e.getPosition());

    if (e.mods.isPopupMenu())
    {
        if (c != nullptr && ! selection.count (c->id))
        {
            selection.clear();
            selection.insert (c->id);
        }
        repaint();
        showClipMenu (c);
        return;
    }

    if (c == nullptr)
    {
        if (! e.mods.isShiftDown())
            selection.clear();
        repaint();
        return;
    }

    if (e.mods.isShiftDown() || e.mods.isCommandDown())
    {
        if (! selection.erase (c->id))
            selection.insert (c->id);
    }
    else if (! selection.count (c->id))
    {
        selection.clear();
        selection.insert (c->id);
    }

    if (clipBounds (*c).getRight() - (float) e.x < 7.0f && selection.size() == 1)
    {
        mode = Mode::resize;
        resizeId = c->id;
        resizeLength = c->length;
    }
    else
    {
        mode = Mode::pendingMove;
        grabOffset = xToBeat ((float) e.x) - selectionStart();
    }

    repaint();
}

void SongLane::mouseDrag (const juce::MouseEvent& e)
{
    switch (mode)
    {
        case Mode::seek:
        {
            auto b = snapNearest (xToBeat ((float) e.x));
            proc.engine.seek (b);
            playhead = b;
            repaint();
            break;
        }
        case Mode::resize:
        {
            if (auto* c = proc.song.find (resizeId))
            {
                auto minLen = gridSize() > 0.0 ? gridSize() : 0.25;
                auto end = snapNearest (xToBeat ((float) e.x));
                resizeLength = juce::jmax (minLen, end - c->start);
                repaint();
            }
            break;
        }
        case Mode::pendingMove:
            if (e.getDistanceFromDragStart() > 4 && ! selection.empty())
                startClipDrag();
            break;
        case Mode::dragging:
        case Mode::none:
            break;
    }
}

void SongLane::mouseUp (const juce::MouseEvent&)
{
    if (mode == Mode::resize)
    {
        auto id = resizeId;
        mode = Mode::none;
        resizeId = 0;
        proc.song.resizeClip (id, resizeLength);
    }

    mode = Mode::none;
    repaint();
}

void SongLane::mouseMove (const juce::MouseEvent& e)
{
    auto* c = e.y >= rulerHeight ? clipAt (e.getPosition()) : nullptr;
    if (c != nullptr && clipBounds (*c).getRight() - (float) e.x < 7.0f)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (c != nullptr)
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SongLane::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* c = clipAt (e.getPosition()))
        if (onPreviewGroove && c->groove != nullptr)
            onPreviewGroove (c->groove);
}

void SongLane::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomBy (w.deltaY > 0 ? 1.15 : 1.0 / 1.15);
        return;
    }

    auto delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    setViewStart (viewStart - delta * visibleBeats() * 0.25);
}

void SongLane::mouseMagnify (const juce::MouseEvent&, float scale)
{
    zoomBy (scale);
}

bool SongLane::keyPressed (const juce::KeyPress& k)
{
    auto cmd = k.getModifiers().isCommandDown();

    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (cmd && k.getKeyCode() == 'D')  { duplicateSelected(); return true; }
    if (cmd && k.getKeyCode() == 'A')  { selectAll(); return true; }
    if (cmd && k.getKeyCode() == 'Z')
    {
        if (k.getModifiers().isShiftDown()) proc.song.redo();
        else                                proc.song.undo();
        return true;
    }
    if (k == juce::KeyPress::escapeKey) { selection.clear(); repaint(); return true; }
    if (k == juce::KeyPress::spaceKey && onTogglePlay) { onTogglePlay(); return true; }
    if (k.getKeyCode() == '=' || k.getKeyCode() == '+') { zoomBy (1.25); return true; }
    if (k.getKeyCode() == '-') { zoomBy (0.8); return true; }
    return false;
}

void SongLane::showClipMenu (const tt::SongClip* c)
{
    juce::PopupMenu m;
    if (c != nullptr)
    {
        m.addItem (1, "Preview groove");
        m.addItem (2, "Duplicate", true);
        m.addItem (3, "Reset length to groove", selection.size() == 1);
        m.addItem (4, "Delete");
        m.addSeparator();
        m.addItem (5, "Export selection as MIDI file...");
        m.addSeparator();
    }
    m.addItem (6, "Select all", ! proc.song.getClips().empty());
    m.addItem (7, "Undo", proc.song.canUndo());
    m.addItem (8, "Redo", proc.song.canRedo());

    auto groove = c != nullptr ? c->groove : nullptr;
    auto clipId = c != nullptr ? c->id : 0;
    juce::Component::SafePointer<SongLane> safe (this);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, groove, clipId] (int r)
    {
        if (safe == nullptr)
            return;

        auto& self = *safe;
        switch (r)
        {
            case 1: if (self.onPreviewGroove && groove != nullptr) self.onPreviewGroove (groove); break;
            case 2: self.duplicateSelected(); break;
            case 3: if (auto* cc = self.proc.song.find (clipId); cc != nullptr && cc->groove != nullptr)
                        self.proc.song.resizeClip (clipId, cc->groove->lengthBeats);
                    break;
            case 4: self.deleteSelected(); break;
            case 5: if (self.onExportClipsRequest) self.onExportClipsRequest ({}); break;
            case 6: self.selectAll(); break;
            case 7: self.proc.song.undo(); break;
            case 8: self.proc.song.redo(); break;
            default: break;
        }
    });
}

//==============================================================================
void SongLane::deleteSelected()
{
    proc.song.removeClips (getSelection());
    selection.clear();
}

void SongLane::duplicateSelected()
{
    if (selection.empty())
        return;

    auto ids = proc.song.duplicateClips (getSelection());
    selection.clear();
    selection.insert (ids.begin(), ids.end());
    ensureVisible (selectionEnd());
    repaint();
}

void SongLane::selectAll()
{
    selection.clear();
    for (auto& c : proc.song.getClips())
        selection.insert (c.id);
    repaint();
}

void SongLane::addGrooveAtEnd (int grooveIndex)
{
    juce::String err;
    auto g = proc.app->loadGroove (grooveIndex, err);
    if (g == nullptr)
        return;

    auto bar = proc.getBarLengthBeats();
    auto start = std::ceil (proc.song.getEndBeat() / bar - 1.0e-9) * bar;
    auto id = proc.song.addClip (g, start);
    selection.clear();
    selection.insert (id);
    ensureVisible (start + g->lengthBeats);
    repaint();
}

void SongLane::startClipDrag()
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr)
        return;

    mode = Mode::dragging;
    auto s = selectionStart(), e = selectionEnd();
    auto area = juce::Rectangle<float> (beatToX (s), (float) laneArea().getY(), (float) ((e - s) * pixelsPerBeat), (float) laneArea().getHeight())
                    .getIntersection (getLocalBounds().toFloat()).toNearestInt();

    juce::Image img;
    if (! area.isEmpty())
        img = createComponentSnapshot (area, true, 1.0f);

    juce::Point<int> offset (area.getX() - downPos.x, area.getY() - downPos.y);
    container->startDragging (drag::make (drag::clips), this, juce::ScaledImage (img, 1.0), false, &offset);
}

//==============================================================================
double SongLane::lengthForDragSource (const SourceDetails& d)
{
    auto type = drag::typeOf (d.description);
    if (type == drag::clips)
        return selectionEnd() - selectionStart();

    if (type == drag::groove)
    {
        auto idx = drag::indexOf (d.description);
        if (idx != ghostGrooveIndex)
        {
            ghostGrooveIndex = idx;
            ghostLength = 0.0;
            if (auto index = proc.app->getIndex(); index != nullptr && juce::isPositiveAndBelow (idx, (int) index->grooves.size()))
                ghostLength = index->grooves[(size_t) idx].lengthBeats;

            if (ghostLength <= 0.0)
            {
                juce::String err;
                if (auto g = proc.app->loadGroove (idx, err))
                    ghostLength = g->lengthBeats;
            }
        }
        return ghostLength > 0.0 ? ghostLength : proc.getBarLengthBeats();
    }

    return 0.0;
}

bool SongLane::isInterestedInDragSource (const SourceDetails& d)
{
    auto type = drag::typeOf (d.description);
    return type == drag::groove || type == drag::clips;
}

void SongLane::itemDragEnter (const SourceDetails& d)
{
    ghostGrooveIndex = -1;
    itemDragMove (d);
}

void SongLane::itemDragMove (const SourceDetails& d)
{
    auto len = lengthForDragSource (d);
    auto beat = xToBeat ((float) d.localPosition.x);

    if (drag::typeOf (d.description) == drag::clips)
        ghostStart = snapNearest (beat - grabOffset);
    else
        ghostStart = snapFloor (beat);

    ghostLength = len;
    ghostVisible = true;

    // Auto-scroll near the edges
    if (d.localPosition.x > getWidth() - 30)  setViewStart (viewStart + visibleBeats() * 0.02);
    else if (d.localPosition.x < 30)          setViewStart (viewStart - visibleBeats() * 0.02);

    repaint();
}

void SongLane::itemDragExit (const SourceDetails&)
{
    ghostVisible = false;
    ghostGrooveIndex = -1;
    repaint();
}

void SongLane::itemDropped (const SourceDetails& d)
{
    itemDragMove (d);
    ghostVisible = false;
    auto type = drag::typeOf (d.description);

    if (type == drag::clips)
    {
        std::vector<int> newIds;
        auto copy = juce::ModifierKeys::currentModifiers.isAltDown();
        proc.song.moveClips (getSelection(), ghostStart - selectionStart(), copy, &newIds);
        if (! newIds.empty())
        {
            selection.clear();
            selection.insert (newIds.begin(), newIds.end());
        }
    }
    else if (type == drag::groove)
    {
        juce::String err;
        if (auto g = proc.app->loadGroove (drag::indexOf (d.description), err))
        {
            auto id = proc.song.addClip (g, ghostStart);
            selection.clear();
            selection.insert (id);
        }
    }

    mode = Mode::none;
    ghostGrooveIndex = -1;
    grabKeyboardFocus();
    repaint();
}

//==============================================================================
bool SongLane::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
            return true;
    return false;
}

void SongLane::fileDragMove (const juce::StringArray&, int x, int)
{
    ghostStart = snapFloor (xToBeat ((float) x));
    ghostLength = proc.getBarLengthBeats();
    ghostVisible = true;
    repaint();
}

void SongLane::fileDragExit (const juce::StringArray&)
{
    ghostVisible = false;
    repaint();
}

void SongLane::filesDropped (const juce::StringArray& files, int x, int)
{
    ghostVisible = false;
    auto pos = snapFloor (xToBeat ((float) x));
    selection.clear();

    for (auto& path : files)
    {
        if (! (path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi")))
            continue;

        juce::String err;
        if (auto g = proc.app->loadGrooveFile (juce::File (path), err))
        {
            selection.insert (proc.song.addClip (g, pos));
            pos += g->lengthBeats;
        }
    }

    repaint();
}
