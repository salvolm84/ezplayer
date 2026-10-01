#pragma once

#include <juce_core/juce_core.h>

// Helpers for the Toontrack MIDI library naming convention.
//
// Every folder / file in a Toontrack MIDI library is named "<sortkey>@<NAME>":
//   - the sort key orders siblings ("000352", "406", "090-S012", "M", ...)
//   - '_' encodes a space and '#' encodes '/' (e.g. "STRAIGHT_4#4" -> "STRAIGHT 4/4")
//   - a leading '_' in NAME marks a heading ("300@_SLOW_MIDTEMPO")
//   - a folder containing an empty file called "header" is a heading row
//   - an empty "<sortkey>@_<TEXT>.head" file is a heading inside a file list
namespace tt
{
    struct DecodedName
    {
        juce::String sortKey;      // empty when the raw name has no '@'
        juce::String display;      // human readable
        bool headingMarker = false; // NAME started with '_'
    };

    // `raw` should already have its file extension removed.
    DecodedName decodeName (const juce::String& raw);

    // '_' -> ' ', '#' -> '/', collapses repeated whitespace.
    juce::String prettify (const juce::String& s);

    // Returns 0 when no tempo can be inferred. Understands "92 BPM", "_92_BPM",
    // a "090-S012" style sort key and Groove Monkee style "150 6-4 Fill 13".
    double tempoFromName (const juce::String& rawOrDisplay, const juce::String& sortKey = {});

    // Detects "4#4", "4/4", "6-8 FILLS" style time signatures.
    bool timeSigFromName (const juce::String& name, int& num, int& den);

    // Toontrack-compatible sibling ordering on raw names.
    int compareRawNames (const juce::String& a, const juce::String& b);

    bool isMidiFileName (const juce::String& fileName);
    bool isHeadFileName (const juce::String& fileName);
}
