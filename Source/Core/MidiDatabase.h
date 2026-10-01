#pragma once

#include <juce_core/juce_core.h>
#include <unordered_map>

// Reader for the per-library "midiDB" SQLite index that ships with Toontrack MIDI packs.
// It provides display names, tempo, meter, length and tags for every groove.
namespace tt
{
    struct DbEntry
    {
        juce::String name;          // e.g. "Fills 09"
        double tempo = 0.0;
        int num = 0, den = 0;
        double lengthBeats = 0.0;   // in quarter notes
        int intensity = 0;
        juce::String genre;
        juce::StringArray tags;     // tag names ("Fill", "Swing", "Double Kick", ...)
    };

    struct MidiDatabase
    {
        juce::String libraryName;   // LIBRARY.FormattedName, e.g. "The Beats of New Orleans"
        juce::StringArray playStyleTags; // tag names in the "Play Style" group

        // key: lower-cased path relative to the folder holding the midiDB, using '/'
        std::unordered_map<juce::String, DbEntry> entries;

        bool load (const juce::File& dbFile, juce::String& error);

        const DbEntry* find (const juce::String& relativePath) const;
    };
}
