#include "RhythmIndex.h"
#include "DrumMap.h"

namespace tt
{
    static constexpr int ghostVelocity = 30;

    static int popcount (juce::uint32 v) { return juce::countNumberOfBits (v); }

    RhythmPrint makeRhythmPrint (const Groove& g, int num, int den, int maxBars)
    {
        RhythmPrint p;
        p.num = (juce::uint8) juce::jlimit (1, 32, num);
        p.den = (juce::uint8) juce::jlimit (1, 32, den);

        const auto steps = p.getStepsPerBar();
        const auto barLen = p.num * 4.0 / p.den;
        const auto stepLen = barLen / steps;
        const auto numBars = juce::jlimit (1, maxBars, (int) std::ceil (g.lengthBeats / barLen - 1.0e-6));
        p.bars.resize ((size_t) numBars);

        for (auto& n : g.notes)
        {
            if (n.velocity < ghostVelocity)
                continue;

            juce::uint32 BarPrint::* voice = nullptr;
            switch (kitPieceForNote (n.note))
            {
                case KitPiece::kick:      voice = &BarPrint::kick; break;
                case KitPiece::snare:
                case KitPiece::sidestick: voice = &BarPrint::snare; break;
                case KitPiece::hihat:
                case KitPiece::ride:
                case KitPiece::crash:     voice = &BarPrint::cymbal; break;
                default: break;
            }

            if (voice == nullptr)
                continue;

            auto stepIndex = (int) std::lround (n.start / stepLen);
            auto bar = stepIndex / steps;
            auto step = stepIndex % steps;

            if (bar >= numBars) // a hit quantised past the loop end belongs to the downbeat
                bar = 0;

            p.bars[(size_t) bar].*voice |= (1u << step);
        }

        return p;
    }

    // Dice similarity of two step masks; hits one step apart count half.
    static float voiceScore (juce::uint32 pat, juce::uint32 grv, juce::uint32 stepMask)
    {
        auto np = popcount (pat), ng = popcount (grv);
        if (np + ng == 0)
            return 1.0f;

        auto exact = pat & grv;
        auto pRest = pat & ~exact, gRest = grv & ~exact;
        auto neighbours = ((gRest << 1) | (gRest >> 1)) & stepMask;
        auto near = juce::jmin (popcount (pRest & neighbours), popcount (gRest));

        auto matched = popcount (exact) + 0.5f * (float) near;
        return 2.0f * matched / (float) (np + ng);
    }

    static float barScore (const BarPrint& p, const BarPrint& g, juce::uint32 stepMask)
    {
        static constexpr float wKick = 1.0f, wSnare = 1.0f, wCymbal = 0.6f;
        float sum = 0.0f, weight = 0.0f;

        // Only the voices the user programmed take part.
        if (p.kick != 0)   { sum += wKick   * voiceScore (p.kick, g.kick, stepMask);       weight += wKick; }
        if (p.snare != 0)  { sum += wSnare  * voiceScore (p.snare, g.snare, stepMask);     weight += wSnare; }
        if (p.cymbal != 0) { sum += wCymbal * voiceScore (p.cymbal, g.cymbal, stepMask);   weight += wCymbal; }

        return weight > 0.0f ? sum / weight : 0.0f;
    }

    float scoreRhythm (const std::vector<BarPrint>& pattern, int steps, const RhythmPrint& groove)
    {
        if (pattern.empty() || groove.bars.empty() || groove.getStepsPerBar() != steps)
            return 0.0f;

        const auto stepMask = steps >= 32 ? 0xffffffffu : ((1u << steps) - 1u);
        const auto nb = groove.bars.size();
        float best = 0.0f, total = 0.0f;
        int windows = 0;

        for (size_t start = 0; start < nb; ++start)
        {
            float s = 0.0f;
            for (size_t i = 0; i < pattern.size(); ++i)
                s += barScore (pattern[i], groove.bars[(start + i) % nb], stepMask);

            s /= (float) pattern.size();
            best = juce::jmax (best, s);
            total += s;
            ++windows;
        }

        return 0.85f * best + 0.15f * (total / (float) windows);
    }

    std::vector<RhythmIndex::Match> RhythmIndex::search (const std::vector<BarPrint>& pattern, int num, int den,
                                                         const std::vector<int>& candidates, float minScore,
                                                         size_t maxResults) const
    {
        std::vector<Match> out;
        const auto steps = RhythmPrint::stepsPerBar (num, den);

        bool any = false;
        for (auto& b : pattern)
            any = any || ! b.isEmpty();
        if (! any)
            return out;

        for (auto g : candidates)
        {
            auto* p = get (g);
            if (p == nullptr || p->num != num || p->den != den)
                continue;

            auto s = scoreRhythm (pattern, steps, *p);
            if (s >= minScore)
                out.push_back ({ g, s });
        }

        std::stable_sort (out.begin(), out.end(), [] (const Match& a, const Match& b) { return a.score > b.score; });
        if (out.size() > maxResults)
            out.resize (maxResults);
        return out;
    }

    //==============================================================================
    static constexpr int cacheMagic = 0x47505248; // "GPRH"
    static constexpr int cacheVersion = 1;

    bool RhythmCache::save (const juce::File& f) const
    {
        juce::MemoryOutputStream out;
        out.writeInt (cacheMagic);
        out.writeInt (cacheVersion);
        out.writeInt ((int) byPath.size());

        for (auto& [path, p] : byPath)
        {
            out.writeString (path);
            out.writeByte ((char) p.num);
            out.writeByte ((char) p.den);
            out.writeByte ((char) p.bars.size());
            for (auto& b : p.bars)
            {
                out.writeInt ((int) b.kick);
                out.writeInt ((int) b.snare);
                out.writeInt ((int) b.cymbal);
            }
        }

        f.getParentDirectory().createDirectory();
        auto tmp = f.getSiblingFile (f.getFileName() + ".tmp");
        return tmp.replaceWithData (out.getData(), out.getDataSize()) && tmp.moveFileTo (f);
    }

    bool RhythmCache::load (const juce::File& f)
    {
        juce::MemoryBlock data;
        if (! f.loadFileAsData (data))
            return false;

        juce::MemoryInputStream in (data, false);
        if (in.readInt() != cacheMagic || in.readInt() != cacheVersion)
            return false;

        auto n = in.readInt();
        if (n < 0 || n > 50'000'000)
            return false;

        byPath.reserve ((size_t) n);
        for (int i = 0; i < n && ! in.isExhausted(); ++i)
        {
            auto path = in.readString();
            RhythmPrint p;
            p.num = (juce::uint8) in.readByte();
            p.den = (juce::uint8) in.readByte();
            p.bars.resize ((size_t) (juce::uint8) in.readByte());
            for (auto& b : p.bars)
            {
                b.kick = (juce::uint32) in.readInt();
                b.snare = (juce::uint32) in.readInt();
                b.cymbal = (juce::uint32) in.readInt();
            }
            byPath[path] = std::move (p);
        }

        return true;
    }

    //==============================================================================
    std::unique_ptr<RhythmIndex> buildRhythmIndex (std::shared_ptr<const LibraryIndex> library, RhythmCache& cache,
                                                   RhythmProgress& progress, const std::function<bool()>& shouldExit,
                                                   bool& cacheChanged)
    {
        auto idx = std::make_unique<RhythmIndex>();
        idx->library = library;
        cacheChanged = false;

        if (library == nullptr)
            return idx;

        const auto& grooves = library->grooves;
        idx->printForGroove.assign (grooves.size(), -1);
        idx->prints.reserve (grooves.size());
        progress.total = (int) grooves.size();
        progress.done = 0;

        std::unordered_map<juce::String, RhythmPrint> live;
        live.reserve (grooves.size());

        for (size_t i = 0; i < grooves.size(); ++i)
        {
            progress.done = (int) i + 1;

            if (shouldExit && shouldExit())
                return nullptr;

            auto& g = grooves[i];
            if (g.isHeading)
                continue;

            auto path = library->getFile ((int) i).getFullPathName();
            RhythmPrint print;

            if (auto it = cache.byPath.find (path); it != cache.byPath.end())
            {
                print = it->second;
            }
            else
            {
                juce::String err;
                auto groove = Groove::loadFromFile (juce::File (path), g.name, g.lengthBeats, err);
                if (groove == nullptr)
                {
                    idx->numMissing++;
                    continue;
                }

                auto num = g.num > 0 ? (int) g.num : (groove->hasTimeSig ? groove->num : 4);
                auto den = g.den > 0 ? (int) g.den : (groove->hasTimeSig ? groove->den : 4);
                print = makeRhythmPrint (*groove, num, den);
                cacheChanged = true;
            }

            idx->printForGroove[i] = (int) idx->prints.size();
            idx->prints.push_back (print);
            live[path] = std::move (print);
        }

        // Keep only files that still exist in the library.
        if (live.size() != cache.byPath.size())
            cacheChanged = true;
        cache.byPath = std::move (live);
        return idx;
    }
}
