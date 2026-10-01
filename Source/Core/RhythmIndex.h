#pragma once

#include "LibraryIndex.h"
#include "Groove.h"

#include <atomic>
#include <unordered_map>

// Rhythm fingerprints used by the "beat search": every groove is reduced to one
// 16th-note step mask per bar for three voices (kick, snare, any cymbal), which can
// then be compared quickly against a user-programmed pattern.
namespace tt
{
    struct BarPrint
    {
        juce::uint32 kick = 0, snare = 0, cymbal = 0; // bit i = step i (max 32 steps)

        bool isEmpty() const { return (kick | snare | cymbal) == 0; }
        bool operator== (const BarPrint& o) const { return kick == o.kick && snare == o.snare && cymbal == o.cymbal; }
    };

    struct RhythmPrint
    {
        juce::uint8 num = 4, den = 4;
        std::vector<BarPrint> bars;

        int getStepsPerBar() const { return stepsPerBar (num, den); }
        static int stepsPerBar (int num, int den) { return juce::jlimit (1, 32, num * 16 / juce::jmax (1, den)); }
    };

    // Quantises a groove onto the 16th grid of its meter. Ghost notes and the hi-hat
    // pedal are ignored.
    RhythmPrint makeRhythmPrint (const Groove&, int num, int den, int maxBars = 16);

    // 0..1 similarity of a pattern against a groove (best bar window, with a little
    // weight on how consistently the whole groove matches).
    float scoreRhythm (const std::vector<BarPrint>& pattern, int steps, const RhythmPrint& groove);

    class RhythmIndex
    {
    public:
        std::shared_ptr<const LibraryIndex> library;  // the index this one is aligned to
        std::vector<int> printForGroove;              // groove index -> prints[] or -1
        std::vector<RhythmPrint> prints;
        int numMissing = 0;

        const RhythmPrint* get (int grooveIndex) const
        {
            if (! juce::isPositiveAndBelow (grooveIndex, (int) printForGroove.size()))
                return nullptr;
            auto p = printForGroove[(size_t) grooveIndex];
            return p >= 0 ? &prints[(size_t) p] : nullptr;
        }

        struct Match { int groove; float score; };

        // Scores `candidates` (groove indices) against the pattern, best first.
        std::vector<Match> search (const std::vector<BarPrint>& pattern, int num, int den,
                                   const std::vector<int>& candidates, float minScore, size_t maxResults) const;
    };

    // Path-keyed persistent cache of fingerprints, so only new files are parsed.
    struct RhythmCache
    {
        std::unordered_map<juce::String, RhythmPrint> byPath;

        bool save (const juce::File&) const;
        bool load (const juce::File&);
    };

    struct RhythmProgress
    {
        std::atomic<int> done { 0 }, total { 0 };
    };

    // Builds the index for `library`, parsing files not present in `cache` (which is
    // updated). Runs on a background thread.
    std::unique_ptr<RhythmIndex> buildRhythmIndex (std::shared_ptr<const LibraryIndex> library, RhythmCache& cache,
                                                   RhythmProgress&, const std::function<bool()>& shouldExit,
                                                   bool& cacheChanged);
}
