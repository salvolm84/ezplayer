#include "Song.h"

#include <map>

namespace tt
{
    static constexpr double eps = 1.0e-6;
    static constexpr size_t maxUndo = 100;

    std::vector<SongClip> Song::getClipsCopy() const
    {
        const juce::ScopedLock sl (lock);
        return clips;
    }

    const SongClip* Song::find (int id) const
    {
        for (auto& c : clips)
            if (c.id == id)
                return &c;
        return nullptr;
    }

    void Song::place (std::vector<SongClip>& list, SongClip c, int& nextIdRef)
    {
        const auto s = c.start, e = c.end();
        std::vector<SongClip> out;
        out.reserve (list.size() + 2);

        for (auto& o : list)
        {
            if (o.end() <= s + eps || o.start >= e - eps)
            {
                out.push_back (o);
                continue;
            }

            bool keptLeft = false;
            if (o.start < s - eps)
            {
                auto left = o;
                left.length = s - o.start;
                out.push_back (left);
                keptLeft = true;
            }

            if (o.end() > e + eps)
            {
                auto right = o;
                right.start = e;
                right.length = o.end() - e;
                right.offset = o.offset + (e - o.start);
                if (keptLeft)
                    right.id = nextIdRef++;
                out.push_back (right);
            }
        }

        out.push_back (std::move (c));
        std::sort (out.begin(), out.end(), [] (const SongClip& a, const SongClip& b) { return a.start < b.start; });
        list = std::move (out);
    }

    void Song::pushUndo()
    {
        undoStack.push_back (clips);
        if (undoStack.size() > maxUndo)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
    }

    void Song::commit (std::vector<SongClip> newClips)
    {
        {
            const juce::ScopedLock sl (lock);
            clips = std::move (newClips);
        }
        sendChangeMessage();
    }

    int Song::addClip (Groove::Ptr g, double start, double length)
    {
        if (g == nullptr)
            return 0;

        pushUndo();
        SongClip c;
        c.id = nextId++;
        c.start = juce::jmax (0.0, start);
        c.length = length > eps ? length : g->lengthBeats;
        c.groove = g;

        auto list = clips;
        place (list, c, nextId);
        commit (std::move (list));
        return c.id;
    }

    void Song::moveClips (const std::vector<int>& ids, double delta, bool copy, std::vector<int>* newIds)
    {
        std::vector<SongClip> moving, list;
        for (auto& c : clips)
        {
            bool sel = std::find (ids.begin(), ids.end(), c.id) != ids.end();
            if (sel)
                moving.push_back (c);
            if (! sel || copy)
                list.push_back (c);
        }

        if (moving.empty() || (std::abs (delta) < eps && ! copy))
            return;

        double minStart = moving.front().start;
        for (auto& m : moving)
            minStart = juce::jmin (minStart, m.start);

        delta = juce::jmax (delta, -minStart);

        pushUndo();
        for (auto& m : moving)
        {
            m.start += delta;
            if (copy)
                m.id = nextId++;
            if (newIds != nullptr)
                newIds->push_back (m.id);
            place (list, m, nextId);
        }

        commit (std::move (list));
    }

    void Song::resizeClip (int id, double newLength)
    {
        auto* c = find (id);
        if (c == nullptr || newLength < 0.25 || std::abs (newLength - c->length) < eps)
            return;

        pushUndo();
        auto updated = *c;
        updated.length = newLength;

        std::vector<SongClip> list;
        for (auto& o : clips)
            if (o.id != id)
                list.push_back (o);

        place (list, updated, nextId);
        commit (std::move (list));
    }

    void Song::removeClips (const std::vector<int>& ids)
    {
        std::vector<SongClip> list;
        for (auto& c : clips)
            if (std::find (ids.begin(), ids.end(), c.id) == ids.end())
                list.push_back (c);

        if (list.size() == clips.size())
            return;

        pushUndo();
        commit (std::move (list));
    }

    std::vector<int> Song::duplicateClips (const std::vector<int>& ids)
    {
        double s = 1e300, e = -1e300;
        for (auto& c : clips)
        {
            if (std::find (ids.begin(), ids.end(), c.id) != ids.end())
            {
                s = juce::jmin (s, c.start);
                e = juce::jmax (e, c.end());
            }
        }

        std::vector<int> newIds;
        if (e > s)
            moveClips (ids, e - s, true, &newIds);
        return newIds;
    }

    void Song::clear()
    {
        if (clips.empty())
            return;
        pushUndo();
        commit ({});
    }

    double Song::getEndBeat() const
    {
        double e = 0.0;
        for (auto& c : clips)
            e = juce::jmax (e, c.end());
        return e;
    }

    bool Song::undo()
    {
        if (undoStack.empty())
            return false;
        redoStack.push_back (clips);
        auto prev = std::move (undoStack.back());
        undoStack.pop_back();
        commit (std::move (prev));
        return true;
    }

    bool Song::redo()
    {
        if (redoStack.empty())
            return false;
        undoStack.push_back (clips);
        auto next = std::move (redoStack.back());
        redoStack.pop_back();
        commit (std::move (next));
        return true;
    }

    //==============================================================================
    std::vector<TimelineNote> Song::flatten (const std::vector<SongClip>& list, double rebase)
    {
        std::vector<TimelineNote> notes;

        for (auto& c : list)
        {
            if (c.groove == nullptr || c.groove->lengthBeats <= 0.0)
                continue;

            const auto L = c.groove->lengthBeats;
            const auto from = c.offset, to = c.offset + c.length;
            const auto firstRep = (int) std::floor (from / L);
            const auto lastRep = (int) std::ceil (to / L);

            for (int r = firstRep; r <= lastRep; ++r)
            {
                const auto repBase = r * L;
                const auto repEndAbs = c.start + (repBase + L - from);

                for (auto& n : c.groove->notes)
                {
                    auto local = repBase + n.start;
                    if (local < from - eps || local >= to - eps)
                        continue;

                    TimelineNote t;
                    t.start = c.start + (local - from) - rebase;
                    t.end = juce::jmin (t.start + n.length, c.end() - rebase, repEndAbs - rebase);
                    t.end = juce::jmax (t.end, t.start + 1.0e-4);
                    t.note = n.note;
                    t.velocity = n.velocity;
                    notes.push_back (t);
                }
            }
        }

        std::stable_sort (notes.begin(), notes.end(),
                          [] (const TimelineNote& a, const TimelineNote& b) { return a.start < b.start; });
        return notes;
    }

    Timeline::Ptr Song::buildTimeline() const
    {
        Timeline::Ptr t = new Timeline();
        t->notes = flatten (clips, 0.0);
        t->length = getEndBeat();
        return t;
    }

    //==============================================================================
    juce::ValueTree Song::toValueTree() const
    {
        const juce::ScopedLock sl (lock);

        juce::ValueTree song ("Song");
        juce::ValueTree grooves ("Grooves");
        std::vector<const Groove*> written;

        for (auto& c : clips)
        {
            if (c.groove == nullptr)
                continue;

            auto it = std::find (written.begin(), written.end(), c.groove.get());
            auto key = (int) std::distance (written.begin(), it);

            if (it == written.end())
            {
                written.push_back (c.groove.get());
                auto& g = *c.groove;
                juce::ValueTree gv ("Groove");
                gv.setProperty ("key", key, nullptr);
                gv.setProperty ("name", g.name, nullptr);
                gv.setProperty ("source", g.sourcePath, nullptr);
                gv.setProperty ("length", g.lengthBeats, nullptr);
                gv.setProperty ("midi", g.toMidiFileData().toBase64Encoding(), nullptr);
                grooves.appendChild (gv, nullptr);
            }

            juce::ValueTree cv ("Clip");
            cv.setProperty ("start", c.start, nullptr);
            cv.setProperty ("length", c.length, nullptr);
            cv.setProperty ("offset", c.offset, nullptr);
            cv.setProperty ("groove", key, nullptr);
            song.appendChild (cv, nullptr);
        }

        song.appendChild (grooves, nullptr);
        return song;
    }

    void Song::fromValueTree (const juce::ValueTree& v)
    {
        std::map<int, Groove::Ptr> grooves;
        for (auto gv : v.getChildWithName ("Grooves"))
        {
            juce::MemoryBlock mb;
            if (! mb.fromBase64Encoding (gv.getProperty ("midi").toString()))
                continue;

            if (auto g = Groove::fromMidiFileData (mb, gv.getProperty ("name"), (double) gv.getProperty ("length", 0.0)))
            {
                g->sourcePath = gv.getProperty ("source").toString();
                grooves[(int) gv.getProperty ("key")] = g;
            }
        }

        std::vector<SongClip> list;
        for (auto cv : v)
        {
            if (! cv.hasType ("Clip"))
                continue;

            auto it = grooves.find ((int) cv.getProperty ("groove", -1));
            if (it == grooves.end())
                continue;

            SongClip c;
            c.id = nextId++;
            c.start = cv.getProperty ("start", 0.0);
            c.length = cv.getProperty ("length", it->second->lengthBeats);
            c.offset = cv.getProperty ("offset", 0.0);
            c.groove = it->second;
            if (c.length > eps)
                list.push_back (c);
        }

        std::sort (list.begin(), list.end(), [] (const SongClip& a, const SongClip& b) { return a.start < b.start; });
        undoStack.clear();
        redoStack.clear();
        commit (std::move (list));
    }
}
