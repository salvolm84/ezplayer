#pragma once

#include "Groove.h"
#include "PlaybackEngine.h"

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

namespace tt
{
    struct SongClip
    {
        int id = 0;
        double start = 0.0;   // beats from song start (= DAW bar 1)
        double length = 0.0;  // beats; longer than the groove loops it
        double offset = 0.0;  // beats into the groove where the clip starts
        Groove::Ptr groove;

        double end() const { return start + length; }
    };

    // One-lane song. Modified on the message thread only; other threads may take a
    // copy via getClipsCopy().
    class Song : public juce::ChangeBroadcaster
    {
    public:
        const std::vector<SongClip>& getClips() const { return clips; }
        std::vector<SongClip> getClipsCopy() const;
        const SongClip* find (int id) const;

        int addClip (Groove::Ptr, double start, double length = 0.0);
        void moveClips (const std::vector<int>& ids, double deltaBeats, bool copy, std::vector<int>* newIds = nullptr);
        void resizeClip (int id, double newLength);
        void removeClips (const std::vector<int>& ids);
        std::vector<int> duplicateClips (const std::vector<int>& ids);
        void clear();

        double getEndBeat() const;

        bool canUndo() const { return ! undoStack.empty(); }
        bool canRedo() const { return ! redoStack.empty(); }
        bool undo();
        bool redo();

        juce::ValueTree toValueTree() const;
        void fromValueTree (const juce::ValueTree&);

        Timeline::Ptr buildTimeline() const;

        // Notes of a set of clips, re-based so the earliest clip starts at 0.
        static std::vector<TimelineNote> flatten (const std::vector<SongClip>&, double rebase);

        // Places `c` on `list`, trimming / splitting / removing anything it overlaps.
        static void place (std::vector<SongClip>& list, SongClip c, int& nextId);

    private:
        mutable juce::CriticalSection lock;
        std::vector<SongClip> clips;
        int nextId = 1;
        std::vector<std::vector<SongClip>> undoStack, redoStack;

        void pushUndo();
        void commit (std::vector<SongClip> newClips);
    };
}
