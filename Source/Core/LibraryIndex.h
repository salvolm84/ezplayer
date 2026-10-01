#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <vector>

namespace tt
{
    struct FolderInfo
    {
        juce::String name;        // display name
        juce::String rawName;     // on-disk name
        juce::String path;        // absolute path
        juce::String section;     // preceding sibling heading ("SMOKEY IKO 90 BPM"), may be empty
        int parent = -1;
        bool isHeading = false;   // Toontrack "header" folder: rendered as a section title
        bool isLibrary = false;   // folder that owns a midiDB
        std::vector<int> children;

        int firstFile = 0, numFiles = 0;  // direct entries (grooves + list headings)
        int descBegin = 0, descEnd = 0;   // all descendant entries, contiguous

        juce::String searchText;  // runtime only: lower-cased ancestor + own names
        int numGrooves = 0;       // runtime only: playable descendants
    };

    struct GrooveInfo
    {
        juce::String name;        // display name
        juce::String fileName;    // on-disk file name
        int folder = -1;
        float tempo = 0.0f;       // 0 = unknown
        float lengthBeats = 0.0f; // quarter notes, 0 = unknown (resolved when loaded)
        juce::uint8 num = 0, den = 0;
        juce::uint8 intensity = 0;
        bool isHeading = false;   // ".head" list heading, not playable
        bool fromDb = false;
        juce::uint64 tags = 0;    // bit i == LibraryIndex::tagNames[i]

        juce::String nameLower;   // runtime only: name + list heading, lower case
    };

    struct SearchFilter
    {
        enum class Type { any, beats, fills };

        juce::String text;
        int scopeFolder = 0;      // 0 = whole library
        Type type = Type::any;
        juce::uint64 requiredTags = 0;
        int num = 0, den = 0;     // 0 = any meter
        double tempoMin = 0.0, tempoMax = 0.0; // 0 = unbounded
        bool favouritesOnly = false;

        bool isActive() const
        {
            return text.trim().isNotEmpty() || type != Type::any || requiredTags != 0
                || num != 0 || tempoMin > 0.0 || tempoMax > 0.0 || favouritesOnly;
        }
    };

    class LibraryIndex
    {
    public:
        static constexpr int maxTags = 64;

        std::vector<FolderInfo> folders;  // folders[0] is a virtual root
        std::vector<GrooveInfo> grooves;
        juce::StringArray tagNames;
        juce::StringArray playStyles;     // subset of tagNames that are play styles
        juce::StringArray timeSignatures; // runtime only, e.g. "4/4", "6/8"
        juce::StringArray roots;

        int getTagBit (const juce::String& name) const { return tagNames.indexOf (name, true); }
        juce::uint64 tagMask (const juce::String& name) const;
        int addTag (const juce::String& name);

        juce::File getFile (int grooveIndex) const;
        juce::String getLocation (int grooveIndex) const;
        juce::String getTypeText (const GrooveInfo&) const;
        juce::String getFolderPathText (int folderIndex) const;
        int findFolderByPath (const juce::String& absolutePath) const;
        int findGrooveByFile (const juce::File&) const;

        // Builds runtime-only fields. Call after scanning or loading.
        void finalise();

        std::vector<int> search (const SearchFilter&, const std::function<bool (int)>& isFavourite) const;

        bool save (const juce::File&) const;
        static std::unique_ptr<LibraryIndex> load (const juce::File&);

        int getNumPlayableGrooves() const;
    };

    struct ScanProgress
    {
        std::atomic<int> filesFound { 0 };
        std::atomic<int> foldersFound { 0 };
    };

    // Walks the library roots. Runs on a background thread; `shouldExit` lets the
    // caller abort.
    std::unique_ptr<LibraryIndex> scanLibrary (const juce::StringArray& roots,
                                               ScanProgress& progress,
                                               const std::function<bool()>& shouldExit);
}
