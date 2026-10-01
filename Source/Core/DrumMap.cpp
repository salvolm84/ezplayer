#include "DrumMap.h"

namespace tt
{
    namespace
    {
        // Groups from Toontrack's "EZDrummer 2 Core" instrumentgroups file.
        struct Group { const char* name; KitPiece piece; int gmKey; std::initializer_list<int> notes; };

        const std::vector<Group>& groups()
        {
            static const std::vector<Group> g {
                { "Kick",             KitPiece::kick,       36, { 34, 35, 36 } },
                { "Snare",            KitPiece::snare,      38, { 6, 33, 38, 39, 40, 66, 68, 69, 70, 71, 125, 126 } },
                { "Sidestick",        KitPiece::sidestick,  37, { 1, 2, 3, 4, 5, 37, 67, 127 } },
                { "Hi-Hat Pedal",     KitPiece::hihatPedal, 44, { 10, 21, 23, 44 } },
                { "Hi-Hat Closed",    KitPiece::hihat,      42, { 22, 42, 61, 62, 63, 64, 65, 122 } },
                { "Hi-Hat Open",      KitPiece::hihat,      46, { 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 24, 25, 26, 46, 60, 123, 124 } },
                { "Toms",             KitPiece::toms,       47, { 41, 43, 45, 47, 48, 72, 73, 74, 75, 77, 78, 79, 80, 81, 82 } },
                { "Ride Crash",       KitPiece::ride,       59, { 59, 76, 86, 91, 98, 103, 110, 115, 118 } },
                { "Ride",             KitPiece::ride,       51, { 51, 53, 84, 85, 87, 88, 89, 90, 92, 93, 96, 97, 99, 100, 101, 102, 104, 105, 108, 109, 111, 112, 113, 114, 116, 117 } },
                { "Crash",            KitPiece::crash,      49, { 27, 28, 29, 30, 31, 32, 49, 50, 52, 54, 55, 56, 57, 58, 83, 94, 95, 106, 107 } },
            };
            return g;
        }

        const Group* groupFor (int note)
        {
            for (auto& g : groups())
                for (auto n : g.notes)
                    if (n == note)
                        return &g;
            return nullptr;
        }
    }

    KitPiece kitPieceForNote (int note)
    {
        static const auto table = []
        {
            std::array<KitPiece, 128> t;
            t.fill (KitPiece::other);
            for (int i = 0; i < 128; ++i)
                if (auto* g = groupFor (i))
                    t[(size_t) i] = g->piece;
            return t;
        }();

        return juce::isPositiveAndBelow (note, 128) ? table[(size_t) note] : KitPiece::other;
    }

    juce::String kitPieceName (KitPiece p)
    {
        switch (p)
        {
            case KitPiece::kick:       return "Kick";
            case KitPiece::snare:      return "Snare";
            case KitPiece::sidestick:  return "Sidestick";
            case KitPiece::hihat:      return "Hi-Hat";
            case KitPiece::hihatPedal: return "HH Pedal";
            case KitPiece::toms:       return "Toms";
            case KitPiece::ride:       return "Ride";
            case KitPiece::crash:      return "Cymbals";
            case KitPiece::other:
            case KitPiece::count:      break;
        }
        return "Other";
    }

    juce::String toontrackArticulationName (int note)
    {
        // Well-known pieces of the Toontrack default map get specific names.
        switch (note)
        {
            case 36: return "Kick";
            case 38: return "Snare Center";
            case 40: return "Snare Rimshot";
            case 37: return "Snare Sidestick";
            case 39: return "Snare Rim / Clap";
            case 42: return "Hi-Hat Closed Tip";
            case 22: return "Hi-Hat Closed Edge";
            case 44: return "Hi-Hat Pedal";
            case 21: return "Hi-Hat Pedal Open";
            case 46: return "Hi-Hat Open";
            case 48: return "Tom 1";
            case 47: return "Tom 2";
            case 45: return "Tom 3";
            case 43: return "Floor Tom 1";
            case 41: return "Floor Tom 2";
            case 49: return "Crash 1";
            case 57: return "Crash 2";
            case 52: return "China";
            case 55: return "Splash";
            case 51: return "Ride Tip";
            case 53: return "Ride Bell";
            case 59: return "Ride Edge";
            case 54: return "Tambourine";
            case 56: return "Cowbell";
            default: break;
        }

        if (auto* g = groupFor (note))
            return juce::String (g->name) + " (var.)";

        return {};
    }

    juce::String noteName (int note)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (! juce::isPositiveAndBelow (note, 128))
            return "-";
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    //==============================================================================
    void DrumMap::setIdentity()
    {
        for (int i = 0; i < 128; ++i)
            map[(size_t) i] = i;
    }

    bool DrumMap::isIdentity() const
    {
        for (int i = 0; i < 128; ++i)
            if (map[(size_t) i] != i)
                return false;
        return true;
    }

    juce::ValueTree DrumMap::toValueTree() const
    {
        juce::ValueTree v ("DrumMap");
        v.setProperty ("name", name, nullptr);

        juce::StringArray values;
        for (auto m : map)
            values.add (juce::String (m));

        v.setProperty ("map", values.joinIntoString (","), nullptr);
        return v;
    }

    DrumMap DrumMap::fromValueTree (const juce::ValueTree& v)
    {
        DrumMap d;
        d.name = v.getProperty ("name", d.name).toString();

        juce::StringArray values;
        values.addTokens (v.getProperty ("map").toString(), ",", {});

        if (values.size() == 128)
            for (int i = 0; i < 128; ++i)
                d.map[(size_t) i] = juce::jlimit (-1, 127, values[i].getIntValue());

        return d;
    }

    bool DrumMap::saveToFile (const juce::File& f) const
    {
        // Human readable: one <Note in="" out=""/> per remapped note.
        juce::XmlElement xml ("GroovePlayerDrumMap");
        xml.setAttribute ("name", name);
        xml.setAttribute ("version", 1);

        for (int i = 0; i < 128; ++i)
        {
            if (map[(size_t) i] == i)
                continue;

            auto* n = xml.createNewChildElement ("Note");
            n->setAttribute ("in", i);
            n->setAttribute ("out", map[(size_t) i]);
            n->setAttribute ("name", toontrackArticulationName (i));
        }

        f.getParentDirectory().createDirectory();
        return xml.writeTo (f);
    }

    bool DrumMap::loadFromFile (const juce::File& f, DrumMap& result)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("GroovePlayerDrumMap"))
            return false;

        DrumMap d;
        d.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());

        for (auto* n : xml->getChildWithTagNameIterator ("Note"))
        {
            auto in = n->getIntAttribute ("in", -1);
            if (juce::isPositiveAndBelow (in, 128))
                d.map[(size_t) in] = juce::jlimit (-1, 127, n->getIntAttribute ("out", in));
        }

        result = d;
        return true;
    }

    DrumMap DrumMap::toontrackToGM()
    {
        DrumMap d;
        d.name = "Toontrack -> General MIDI";

        // Collapse every articulation onto its group's GM key, but keep notes that are
        // already meaningful GM pieces.
        static const std::initializer_list<int> keepAsIs {
            35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
            49, 51, 52, 53, 54, 55, 56, 57, 58, 59 };

        for (int i = 0; i < 128; ++i)
        {
            if (std::find (keepAsIs.begin(), keepAsIs.end(), i) != keepAsIs.end())
                continue;

            if (auto* g = groupFor (i))
                d.map[(size_t) i] = g->gmKey;
        }

        // Tom rim/rimshot variations (pairs per tom, high to low).
        for (auto n : { 72, 73 }) d.map[(size_t) n] = 48;
        for (auto n : { 74, 75 }) d.map[(size_t) n] = 47;
        for (auto n : { 77, 78 }) d.map[(size_t) n] = 45;
        for (auto n : { 79, 80 }) d.map[(size_t) n] = 43;
        for (auto n : { 81, 82 }) d.map[(size_t) n] = 41;

        // Secondary crashes
        for (auto n : { 30, 31, 32, 106, 107 }) d.map[(size_t) n] = 57;
        d.map[34] = 36; // foot stomp / extra kick
        return d;
    }

    juce::Array<DrumMap> DrumMap::builtInPresets()
    {
        return { DrumMap(), toontrackToGM() };
    }
}
