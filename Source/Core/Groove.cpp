#include "Groove.h"

namespace tt
{
    static constexpr double minNoteLength = 1.0 / 64.0;

    double Groove::resolveLength (double eotBeats, double lastNoteStart, double known)
    {
        if (known > 0.0 && known >= lastNoteStart)
            return known;

        auto len = juce::jmax (eotBeats, lastNoteStart);
        if (len <= 0.0)
            return 4.0;

        // Toontrack writes end-of-track at "length - 1 tick"; other vendors are less
        // precise (e.g. 951 ticks @240ppq for a one-bar fill). Snap to whole beats
        // when close, otherwise round up.
        auto rounded = std::round (len);
        auto beats = std::abs (len - rounded) < 0.1 ? rounded : std::ceil (len);

        if (beats <= lastNoteStart)
            beats = std::floor (lastNoteStart) + 1.0;

        return juce::jmax (1.0, beats);
    }

    Groove::Ptr Groove::fromMidiFile (const juce::MidiFile& mf, const juce::String& name, double known, juce::String& error)
    {
        auto tpq = (int) mf.getTimeFormat();
        if (tpq <= 0)
        {
            error = "SMPTE-timed MIDI files are not supported";
            return nullptr;
        }

        Ptr g = new Groove();
        g->name = name;

        double eot = 0.0, lastStart = 0.0;
        bool gotTempo = false;

        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            auto* seq = mf.getTrack (t);

            for (int i = 0; i < seq->getNumEvents(); ++i)
            {
                auto* ev = seq->getEventPointer (i);
                auto& m = ev->message;
                auto beat = m.getTimeStamp() / tpq;

                if (m.isEndOfTrackMetaEvent())
                {
                    eot = juce::jmax (eot, beat);
                }
                else if (m.isTempoMetaEvent() && ! gotTempo)
                {
                    auto spq = m.getTempoSecondsPerQuarterNote();
                    if (spq > 0.0)
                    {
                        g->tempo = std::round (600.0 / spq) / 10.0;
                        gotTempo = true;
                    }
                }
                else if (m.isTimeSignatureMetaEvent() && ! g->hasTimeSig)
                {
                    int n = 4, d = 4;
                    m.getTimeSignatureInfo (n, d);
                    if (n > 0 && d > 0)
                    {
                        g->num = n;
                        g->den = d;
                        g->hasTimeSig = true;
                    }
                }
                else if (m.isNoteOn())
                {
                    GrooveNote n;
                    n.start = beat;
                    n.note = (juce::uint8) m.getNoteNumber();
                    n.velocity = (juce::uint8) juce::jlimit (1, 127, (int) m.getVelocity());

                    double end = beat + 0.25;
                    if (ev->noteOffObject != nullptr)
                        end = ev->noteOffObject->message.getTimeStamp() / tpq;

                    n.length = juce::jmax (minNoteLength, end - beat);
                    g->notes.push_back (n);
                    lastStart = juce::jmax (lastStart, beat);
                }
            }
        }

        if (g->notes.empty())
        {
            error = "The MIDI file contains no notes";
            return nullptr;
        }

        std::stable_sort (g->notes.begin(), g->notes.end(),
                          [] (const GrooveNote& a, const GrooveNote& b) { return a.start < b.start; });

        g->lengthBeats = resolveLength (eot, lastStart, known);

        // Notes past the loop end would never play; notes may not ring past it.
        g->notes.erase (std::remove_if (g->notes.begin(), g->notes.end(),
                                        [len = g->lengthBeats] (const GrooveNote& n) { return n.start >= len; }),
                        g->notes.end());

        for (auto& n : g->notes)
            n.length = juce::jmax (minNoteLength, juce::jmin (n.length, g->lengthBeats - n.start));

        return g;
    }

    Groove::Ptr Groove::loadFromFile (const juce::File& file, const juce::String& name, double known, juce::String& error)
    {
        juce::FileInputStream in (file);
        if (! in.openedOk())
        {
            error = "Cannot open " + file.getFullPathName();
            return nullptr;
        }

        juce::MidiFile mf;
        if (! mf.readFrom (in, true))
        {
            error = "Not a valid MIDI file: " + file.getFileName();
            return nullptr;
        }

        auto g = fromMidiFile (mf, name.isNotEmpty() ? name : file.getFileNameWithoutExtension(), known, error);
        if (g != nullptr)
            g->sourcePath = file.getFullPathName();

        return g;
    }

    juce::MemoryBlock Groove::toMidiFileData() const
    {
        constexpr int ppq = 960;
        juce::MidiMessageSequence seq;

        if (tempo > 0.0)
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60'000'000.0 / tempo)), 0);
        if (hasTimeSig)
            seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (num, den), 0);

        for (auto& n : notes)
        {
            seq.addEvent (juce::MidiMessage::noteOn (10, n.note, n.velocity), std::round (n.start * ppq));
            seq.addEvent (juce::MidiMessage::noteOff (10, n.note), std::round ((n.start + n.length) * ppq));
        }

        seq.addEvent (juce::MidiMessage::endOfTrack(), lengthBeats * ppq);
        seq.updateMatchedPairs();

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);

        juce::MemoryOutputStream out;
        mf.writeTo (out, 0);
        return out.getMemoryBlock();
    }

    Groove::Ptr Groove::fromMidiFileData (const juce::MemoryBlock& data, const juce::String& name, double lengthBeats)
    {
        juce::MemoryInputStream in (data, false);
        juce::MidiFile mf;
        if (! mf.readFrom (in, true))
            return nullptr;

        juce::String err;
        return fromMidiFile (mf, name, lengthBeats, err);
    }

    //==============================================================================
    Groove::Ptr GrooveCache::get (const juce::String& path)
    {
        std::lock_guard<std::mutex> sl (lock);
        for (auto it = items.begin(); it != items.end(); ++it)
        {
            if ((*it)->sourcePath == path)
            {
                auto g = *it;
                items.erase (it);
                items.push_front (g);
                return g;
            }
        }
        return nullptr;
    }

    void GrooveCache::put (const Groove::Ptr& g)
    {
        if (g == nullptr || g->sourcePath.isEmpty())
            return;

        std::lock_guard<std::mutex> sl (lock);
        items.remove_if ([&] (const Groove::Ptr& p) { return p->sourcePath == g->sourcePath; });
        items.push_front (g);

        while (items.size() > maxItems)
            items.pop_back();
    }
}
