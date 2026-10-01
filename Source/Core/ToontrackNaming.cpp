#include "ToontrackNaming.h"

namespace tt
{
    juce::String prettify (const juce::String& s)
    {
        auto out = s.replaceCharacter ('_', ' ').replaceCharacter ('#', '/');

        while (out.contains ("  "))
            out = out.replace ("  ", " ");

        return out.trim();
    }

    DecodedName decodeName (const juce::String& raw)
    {
        DecodedName d;
        auto name = raw;
        auto at = raw.indexOfChar ('@');

        if (at >= 0)
        {
            d.sortKey = raw.substring (0, at);
            name = raw.substring (at + 1);
        }

        if (name.startsWithChar ('_'))
        {
            d.headingMarker = true;
            name = name.trimCharactersAtStart ("_");
        }

        d.display = prettify (name);

        if (d.display.isEmpty())
            d.display = prettify (raw);

        return d;
    }

    static bool isPlausibleTempo (int t) { return t >= 30 && t <= 320; }

    double tempoFromName (const juce::String& name, const juce::String& sortKey)
    {
        // "... 92 BPM", "_92_BPM", "92BPM"
        auto upper = name.toUpperCase();
        for (int pos = upper.indexOf ("BPM"); pos >= 0; pos = upper.indexOf (pos + 3, "BPM"))
        {
            int end = pos;
            while (end > 0 && (upper[end - 1] == ' ' || upper[end - 1] == '_' || upper[end - 1] == '-'))
                --end;

            int start = end;
            while (start > 0 && juce::CharacterFunctions::isDigit (upper[start - 1]))
                --start;

            if (end - start >= 2 && end - start <= 3)
            {
                auto t = upper.substring (start, end).getIntValue();
                if (isPlausibleTempo (t))
                    return t;
            }
        }

        // Toontrack sort keys like "090-S012" or "180-S049"
        if (sortKey.length() >= 4)
        {
            auto dash = sortKey.indexOfChar ('-');
            if (dash >= 2 && dash <= 3 && sortKey.substring (0, dash).containsOnly ("0123456789"))
            {
                auto t = sortKey.substring (0, dash).getIntValue();
                if (isPlausibleTempo (t))
                    return t;
            }
        }

        // Groove Monkee & similar: "150 6-4 Fill 13", "089 West Coast 06"
        auto trimmed = name.trimStart();
        int digits = 0;
        while (digits < trimmed.length() && juce::CharacterFunctions::isDigit (trimmed[digits]))
            ++digits;

        if ((digits == 2 || digits == 3) && digits < trimmed.length()
            && (trimmed[digits] == ' ' || trimmed[digits] == '_'))
        {
            auto t = trimmed.substring (0, digits).getIntValue();
            if (isPlausibleTempo (t))
                return t;
        }

        return 0.0;
    }

    static bool validDen (int d) { return d == 2 || d == 4 || d == 8 || d == 16; }

    bool timeSigFromName (const juce::String& name, int& num, int& den)
    {
        auto tryParse = [&] (juce::juce_wchar sep, bool strict) -> bool
        {
            for (int i = name.indexOfChar (sep); i > 0; i = name.indexOfChar (i + 1, sep))
            {
                int s = i;
                while (s > 0 && juce::CharacterFunctions::isDigit (name[s - 1]) && i - s < 2)
                    --s;

                int e = i + 1;
                while (e < name.length() && juce::CharacterFunctions::isDigit (name[e]) && e - (i + 1) < 2)
                    ++e;

                if (s == i || e == i + 1)
                    continue;

                // Reject when embedded in a longer number (e.g. "120-S030")
                if (s > 0 && juce::CharacterFunctions::isLetterOrDigit (name[s - 1]))
                    continue;
                if (e < name.length() && juce::CharacterFunctions::isDigit (name[e]))
                    continue;

                auto n = name.substring (s, i).getIntValue();
                auto d = name.substring (i + 1, e).getIntValue();

                if (n >= 2 && n <= 32 && validDen (d) && (! strict || d >= 4))
                {
                    num = n;
                    den = d;
                    return true;
                }
            }
            return false;
        };

        return tryParse ('#', false) || tryParse ('/', false) || tryParse ('-', true);
    }

    int compareRawNames (const juce::String& a, const juce::String& b)
    {
        return a.compareNatural (b, false);
    }

    bool isMidiFileName (const juce::String& f)
    {
        return f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi");
    }

    bool isHeadFileName (const juce::String& f)
    {
        return f.endsWithIgnoreCase (".head");
    }
}
