#include "MidiExport.h"

namespace tt
{
    juce::MidiFile buildMidiFile (const std::vector<TimelineNote>& notes, double lengthBeats, const ExportOptions& o)
    {
        const auto ppq = o.ppq;
        const auto ch = juce::jlimit (1, 16, o.channel);
        juce::MidiMessageSequence seq;

        if (o.trackName.isNotEmpty())
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, o.trackName), 0);
        if (o.tempo > 0.0)
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60'000'000.0 / o.tempo)), 0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (o.sigNum, o.sigDen), 0);

        for (auto& n : notes)
        {
            auto out = o.map.apply (n.note);
            if (out < 0)
                continue;

            auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * o.velocityScale));
            auto t0 = std::round (n.start * ppq);
            auto t1 = juce::jmax (t0 + 1.0, std::round (n.end * ppq));
            seq.addEvent (juce::MidiMessage::noteOn (ch, out, vel), t0);
            seq.addEvent (juce::MidiMessage::noteOff (ch, out), t1);
        }

        seq.sort();
        seq.addEvent (juce::MidiMessage::endOfTrack(), juce::jmax (seq.getEndTime(), std::round (lengthBeats * ppq)));
        seq.updateMatchedPairs();

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);
        return mf;
    }

    bool writeMidiFile (const juce::File& file, const std::vector<TimelineNote>& notes, double lengthBeats, const ExportOptions& o)
    {
        auto mf = buildMidiFile (notes, lengthBeats, o);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (! out.openedOk())
            return false;
        return mf.writeTo (out, 0);
    }

    static juce::File dragFolder()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GroovePlayer Drag");
    }

    juce::String makeSafeFileName (const juce::String& s)
    {
        auto r = juce::File::createLegalFileName (s.replaceCharacters ("/\\:", "-- ")).trim();
        return r.isEmpty() ? juce::String ("Groove") : r.substring (0, 100);
    }

    juce::File writeTempMidiForDrag (const juce::String& baseName, const std::vector<TimelineNote>& notes,
                                     double lengthBeats, const ExportOptions& o)
    {
        // A unique sub-folder per drag keeps the file name clean (the DAW names the
        // part after it) while never overwriting a file the DAW may still be reading.
        auto dir = dragFolder().getChildFile (juce::String (juce::Time::currentTimeMillis()) + "-"
                                              + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
        dir.createDirectory();

        auto file = dir.getChildFile (makeSafeFileName (baseName) + ".mid");
        return writeMidiFile (file, notes, lengthBeats, o) ? file : juce::File();
    }

    void cleanOldDragFiles()
    {
        auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days (2);
        for (const auto& e : juce::RangedDirectoryIterator (dragFolder(), false, "*", juce::File::findDirectories))
            if (e.getModificationTime() < cutoff)
                e.getFile().deleteRecursively();
    }
}
