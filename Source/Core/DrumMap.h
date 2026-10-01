#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>

namespace tt
{
    // Kit-piece categories, used for colouring the pattern views.
    enum class KitPiece : int { kick, snare, sidestick, hihat, hihatPedal, toms, ride, crash, other, count };

    KitPiece kitPieceForNote (int toontrackNote);
    juce::String kitPieceName (KitPiece);

    // Articulation name of a note in the Toontrack (EZdrummer / Superior Drummer) map.
    juce::String toontrackArticulationName (int note);

    // Note name using the Cubase / Yamaha convention (60 = C3).
    juce::String noteName (int note);

    // Input note -> output note table. -1 mutes the note.
    struct DrumMap
    {
        std::array<int, 128> map {};
        juce::String name { "Toontrack (no remap)" };

        DrumMap() { setIdentity(); }

        void setIdentity();
        bool isIdentity() const;
        int apply (int note) const { return juce::isPositiveAndBelow (note, 128) ? map[(size_t) note] : -1; }

        juce::ValueTree toValueTree() const;
        static DrumMap fromValueTree (const juce::ValueTree&);

        bool saveToFile (const juce::File&) const;
        static bool loadFromFile (const juce::File&, DrumMap& result);

        static DrumMap toontrackToGM();
        static juce::Array<DrumMap> builtInPresets();
    };
}
