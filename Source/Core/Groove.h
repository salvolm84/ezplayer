#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <list>
#include <mutex>

namespace tt
{
    struct GrooveNote
    {
        double start = 0.0;   // quarter notes from groove start
        double length = 0.0;  // quarter notes
        juce::uint8 note = 0, velocity = 0;
    };

    // An immutable, fully parsed MIDI groove. Shared between threads by reference count.
    class Groove : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<Groove>;

        juce::String name;
        juce::String sourcePath;  // absolute file path it was loaded from (may be empty)
        std::vector<GrooveNote> notes; // sorted by start
        double lengthBeats = 4.0;
        double tempo = 0.0;       // 0 = unknown
        int num = 4, den = 4;
        bool hasTimeSig = false;

        double getBarLengthBeats() const { return num * 4.0 / den; }

        // Parses a standard MIDI file. `knownLengthBeats` (e.g. from midiDB) overrides
        // the end-of-track derived length when > 0.
        static Ptr fromMidiFile (const juce::MidiFile&, const juce::String& name, double knownLengthBeats, juce::String& error);
        static Ptr loadFromFile (const juce::File&, const juce::String& name, double knownLengthBeats, juce::String& error);

        // Raw .mid round trip used to embed grooves in the plugin state.
        juce::MemoryBlock toMidiFileData() const;
        static Ptr fromMidiFileData (const juce::MemoryBlock&, const juce::String& name, double lengthBeats);

        // Picks a sensible loop length in beats from the end-of-track time.
        static double resolveLength (double eotBeats, double lastNoteStartBeats, double knownLengthBeats);
    };

    // Small thread-safe LRU cache of loaded grooves keyed by absolute path.
    class GrooveCache
    {
    public:
        explicit GrooveCache (size_t capacity = 128) : maxItems (capacity) {}

        Groove::Ptr get (const juce::String& path);
        void put (const Groove::Ptr&);

    private:
        std::mutex lock;
        size_t maxItems;
        std::list<Groove::Ptr> items; // most recent first
    };
}
