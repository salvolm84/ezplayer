#include "DrumMap.h"

namespace tt
{
    namespace
    {
        // Groups from Toontrack's "EZDrummer 2 Core" instrumentgroups file.
        struct Group { const char* name; KitPiece piece; int gmKey; std::vector<int> notes; };

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

    //==============================================================================
    // Target kits. Each field is the library's note for that articulation, or -1 when
    // the kit doesn't have it (fallbacks are applied in buildFromKit).
    namespace
    {
        struct Kit
        {
            const char* name;
            int kick, snare, sidestick;
            std::array<int, 6> toms;      // high to low, -1 terminated
            int hhPedal, hhPedalOpen;     // "chick", "ching"/splash
            int hhTightTip, hhTightEdge, hhClosedTip, hhClosedEdge;
            int hhOpen1, hhOpen2, hhOpen3, hhOpenFull;
            int rideBow, rideBell, rideCrash;
            int crashL, crashR, china, splash;
        };

        int firstValid (std::initializer_list<int> notes)
        {
            for (auto n : notes)
                if (n >= 0)
                    return n;
            return -1;
        }

        // Spreads Toontrack's 5 toms (Tom 1-3, Floor 1-2) over however many the kit has.
        int tomFor (const Kit& k, int toontrackTom /* 0..4 */)
        {
            int count = 0;
            while (count < 6 && k.toms[(size_t) count] >= 0)
                ++count;

            if (count == 0)
                return -1;

            static const int spread[7][5] = {
                {}, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 1, 1 }, { 0, 1, 1, 2, 2 },
                { 0, 1, 2, 3, 3 }, { 0, 1, 2, 3, 4 }, { 0, 1, 2, 3, 4 } };

            return k.toms[(size_t) spread[count][toontrackTom]];
        }

        DrumMap buildFromKit (const Kit& k)
        {
            DrumMap d;
            d.name = k.name;
            d.map.fill (-1); // anything the kit can't play is muted

            auto set = [&d] (std::initializer_list<int> ins, int out)
            {
                for (auto i : ins)
                    d.map[(size_t) i] = out;
            };

            const auto closedTip  = firstValid ({ k.hhClosedTip, k.hhClosedEdge });
            const auto closedEdge = firstValid ({ k.hhClosedEdge, k.hhClosedTip });
            const auto tightTip   = firstValid ({ k.hhTightTip, k.hhTightEdge, closedTip });
            const auto tightEdge  = firstValid ({ k.hhTightEdge, k.hhTightTip, closedEdge });
            const auto open1      = firstValid ({ k.hhOpen1, k.hhOpen2, k.hhOpen3 });
            const auto open2      = firstValid ({ k.hhOpen2, k.hhOpen1, k.hhOpen3 });
            const auto open3      = firstValid ({ k.hhOpen3, k.hhOpen2, k.hhOpen1 });
            const auto openFull   = firstValid ({ k.hhOpenFull, k.hhOpen3, k.hhOpen2 });
            const auto pedal      = firstValid ({ k.hhPedal, closedTip });
            const auto pedalOpen  = firstValid ({ k.hhPedalOpen, k.hhPedal, open1 });
            const auto crashL     = firstValid ({ k.crashL, k.crashR });
            const auto crashR     = firstValid ({ k.crashR, k.crashL });
            const auto rideCrash  = firstValid ({ k.rideCrash, k.rideBow });

            // Kick / snare (rimshots and snare variations fold onto the snare hit)
            set ({ 34, 35, 36 }, k.kick);
            set ({ 6, 33, 38, 39, 40, 66, 68, 69, 70, 71, 125, 126 }, k.snare);
            set ({ 1, 2, 3, 4, 5, 37, 67, 127 }, firstValid ({ k.sidestick, k.snare }));

            // Toms with their rim / rimshot variations
            set ({ 48, 72, 73 }, tomFor (k, 0));
            set ({ 47, 74, 75 }, tomFor (k, 1));
            set ({ 45, 77, 78 }, tomFor (k, 2));
            set ({ 43, 79, 80 }, tomFor (k, 3));
            set ({ 41, 81, 82 }, tomFor (k, 4));

            // Hi-hat
            set ({ 44, 10 }, pedal);
            set ({ 21, 23 }, pedalOpen);
            set ({ 42, 122 }, closedTip);
            set ({ 22, 61, 64, 65 }, closedEdge);
            set ({ 62 }, tightTip);
            set ({ 63 }, tightEdge);
            set ({ 24, 7, 8, 9, 123 }, open1);
            set ({ 25, 11, 12, 13, 14, 15, 124 }, open2);
            set ({ 26, 16, 17, 18, 19, 20 }, open3);
            set ({ 46, 60 }, openFull);

            // Ride
            set ({ 51, 84, 85, 87, 88, 89, 90, 92, 93, 96, 97, 99, 100, 101, 102, 104, 105, 108, 109, 111, 112, 113, 114, 116, 117 }, k.rideBow);
            set ({ 53 }, firstValid ({ k.rideBell, k.rideBow }));
            set ({ 59, 76, 86, 91, 98, 103, 110, 115, 118 }, rideCrash);

            // Crashes and effects cymbals
            set ({ 49, 27, 28, 29, 50, 83, 94, 95 }, crashL);
            set ({ 57, 30, 31, 32, 106, 107 }, crashR);
            set ({ 52 }, firstValid ({ k.china, crashR }));
            set ({ 55 }, firstValid ({ k.splash, crashR }));

            return d;
        }

        // Default maps of the Kontakt-based GetGood Drums libraries, taken from the
        // libraries' own default .nka mapping presets (github.com/JPplayground/MidiNoteNameGen).
        const Kit ggdHalpern { "GGD Matt Halpern Signature",
            24, 26, 30, { 33, 35, 37, 39, -1, -1 },
            43, 44, 45, 46, 47, 48, 52, 54, 56, 58,
            72, 74, 73, 62, 67, 76, 83 };

        // Modern & Massive was built on the Halpern packs and uses the same default layout (notes 24-83).
        const Kit ggdModernMassive { "GGD Modern & Massive",
            24, 26, 30, { 33, 35, 37, 39, -1, -1 },
            43, 44, 45, 46, 47, 48, 52, 54, 56, 58,
            72, 74, 73, 62, 67, 76, 83 };

        const Kit ggdP4Halpern { "GGD P IV Matt Halpern Signature",
            24, 26, 30, { 33, 34, 35, 36, -1, -1 },
            48, 48, 41, 42, 43, 44, 45, 46, 47, 47,
            62, 61, -1, 52, 54, 65, 73 };

        const Kit ggdInvasion { "GGD Invasion",
            24, 26, 30, { 33, 34, 35, 36, 37, 38 },
            48, 48, 41, 42, 43, 44, 45, 46, 47, 47,
            62, 61, 63, 52, 54, 65, 73 };

        const Kit ggdOkwAggressiveRock { "GGD One Kit Wonder: Aggressive Rock",
            24, 26, 30, { 33, 34, -1, -1, -1, -1 },
            48, 48, 41, 42, 43, 44, 45, 46, 47, 47,
            62, 61, -1, 52, 54, 65, 73 };

        const Kit ggdOkwArchitects { "GGD One Kit Wonder: Architects",
            24, 28, 30, { 35, 36, 37, -1, -1, -1 },
            48, 48, 41, 42, 43, 44, 45, 46, 47, 47,
            62, 61, 63, 52, 54, 67, 73 };

        const Kit ggdOkwMetal { "GGD One Kit Wonder: Metal",
            24, 26, 30, { 33, 34, 35, 36, -1, -1 },
            48, 48, 41, 42, 43, 44, 45, 46, 47, 47,
            62, 61, -1, 52, 58, 65, 73 };

        // Modern & Massive 2 (GGD player) default map.
        const Kit ggdModernMassive2 { "GGD Modern & Massive 2",
            36, 38, 37, { 42, 43, 45, 47, -1, -1 },
            56, 57, 49, 50, 51, 52, 53, 54, 55, 55,
            68, 69, 70, 60, 64, 74, 76 };
    }

    DrumMap DrumMap::toontrackToIOF()
    {
        // IOF Drummer 6 default mapping (from the plugin's Mapping page). IOF has no
        // hi-hat pedal notes, one floor tom and a "Hit 2" alternate for most pieces.
        DrumMap d;
        d.name = "IOF Drummer 6";
        d.map.fill (-1);

        auto set = [&d] (std::initializer_list<int> ins, int out)
        {
            for (auto i : ins)
                d.map[(size_t) i] = out;
        };

        set ({ 34, 36 }, 36);                    // Kick
        set ({ 35 }, 37);                        // Kick (Hit 2)
        set ({ 6, 33, 38, 39, 40, 66, 68, 69, 70, 71, 125, 126 }, 38); // Snare
        set ({ 37 }, 43);                        // Sidestick Hard
        set ({ 1, 2, 3, 4, 5, 67, 127 }, 42);    // Sidestick Soft

        set ({ 48 }, 45); set ({ 72, 73 }, 46);  // Tom 1 / Hit 2
        set ({ 47 }, 47); set ({ 74, 75 }, 48);  // Tom 2 / Hit 2
        set ({ 45 }, 49); set ({ 77, 78 }, 50);  // Tom 3 / Hit 2
        set ({ 43, 41 }, 51);                    // Floor Tom
        set ({ 79, 80, 81, 82 }, 52);            // Floor Tom (Hit 2)

        set ({ 42, 62, 122 }, 57);               // HiHat Closed Tip
        set ({ 22, 61, 63, 64, 65 }, 58);        // HiHat Closed Edge
        set ({ 7, 8, 9, 123 }, 59);              // HiHat Semi Closed Tip
        set ({ 24 }, 60);                        // HiHat Semi Closed Edge
        set ({ 11, 12, 13, 14, 15, 124 }, 65);   // HiHat Semi Open Tip
        set ({ 25 }, 66);                        // HiHat Semi Open Edge
        set ({ 16, 17, 18, 19, 20 }, 68);        // HiHat Open Tip
        set ({ 26, 46, 60 }, 70);                // HiHat Open Edge

        set ({ 51, 84, 85, 87, 88, 89, 90, 92, 93, 96, 97, 99, 100, 101, 102, 104, 105, 108, 109, 111, 112, 113, 114, 116, 117 }, 81); // Ride Zone 1
        set ({ 53 }, 86);                        // Ride Bell
        set ({ 59, 76, 86, 91, 98, 103, 110, 115, 118 }, 85); // Ride Crashed

        set ({ 49, 27, 28, 29, 50, 83, 94, 95 }, 74); // Crash L
        set ({ 57, 30, 31, 32, 106, 107 }, 79);  // Crash R
        set ({ 52 }, 88);                        // China
        set ({ 55 }, 95);                        // Splash
        set ({ 54 }, 92);                        // Tambourine Hand Hit
        return d;
    }

    juce::Array<DrumMap> DrumMap::builtInPresets()
    {
        return { DrumMap(),
                 toontrackToGM(),
                 buildFromKit (ggdHalpern),
                 buildFromKit (ggdModernMassive),
                 buildFromKit (ggdModernMassive2),
                 buildFromKit (ggdP4Halpern),
                 buildFromKit (ggdInvasion),
                 buildFromKit (ggdOkwAggressiveRock),
                 buildFromKit (ggdOkwArchitects),
                 buildFromKit (ggdOkwMetal),
                 toontrackToIOF() };
    }
}
