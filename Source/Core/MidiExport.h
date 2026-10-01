#pragma once

#include "PlaybackEngine.h"
#include "DrumMap.h"

namespace tt
{
    struct ExportOptions
    {
        DrumMap map;              // applied to every note
        int channel = 10;         // 1..16
        float velocityScale = 1.0f;
        double tempo = 120.0;     // written as a tempo meta event
        int sigNum = 4, sigDen = 4;
        juce::String trackName;
        int ppq = 960;
    };

    juce::MidiFile buildMidiFile (const std::vector<TimelineNote>& notes, double lengthBeats, const ExportOptions&);
    bool writeMidiFile (const juce::File&, const std::vector<TimelineNote>& notes, double lengthBeats, const ExportOptions&);

    // Writes into a temp folder that the DAW can read from after a drag and drop.
    juce::File writeTempMidiForDrag (const juce::String& baseName, const std::vector<TimelineNote>& notes,
                                     double lengthBeats, const ExportOptions&);
    void cleanOldDragFiles();

    juce::String makeSafeFileName (const juce::String&);
}
