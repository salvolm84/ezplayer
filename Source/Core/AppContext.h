#pragma once

#include "LibraryIndex.h"
#include "Groove.h"
#include "DrumMap.h"

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <unordered_set>

// Process-wide state shared by every plugin instance (via SharedResourcePointer):
// global settings, the library index + background scanner, favourites and the
// groove loader. Everything here is used from the message thread unless noted.
class AppContext : public juce::ChangeBroadcaster
{
public:
    AppContext();
    ~AppContext() override;

    juce::PropertiesFile& getSettings() { return *settings; }
    static juce::File getDataFolder();
    static juce::File getRemapFolder() { return getDataFolder().getChildFile ("Remaps"); }

    //==============================================================================
    std::shared_ptr<const tt::LibraryIndex> getIndex() const { return index; }
    juce::StringArray getLibraryRoots() const;
    void setLibraryRoots (const juce::StringArray&);
    void rescan();
    bool isScanning() const;
    juce::String getStatusText() const;

    //==============================================================================
    bool isFavourite (const juce::String& absolutePath) const { return favourites.count (absolutePath) > 0; }
    bool isFavourite (int grooveIndex) const;
    void toggleFavourite (int grooveIndex);

    //==============================================================================
    // Loads the groove of the current index. The sync variant is used for drag and
    // drop where the result is needed immediately (files are small and cached).
    tt::Groove::Ptr loadGroove (int grooveIndex, juce::String& error);
    tt::Groove::Ptr loadGrooveFile (const juce::File&, juce::String& error);
    void loadGrooveAsync (int grooveIndex, std::function<void (tt::Groove::Ptr, juce::String)> callback);

    //==============================================================================
    juce::Array<tt::DrumMap> getAllRemapPresets() const;

    bool getAutoPreview() const;
    void setAutoPreview (bool);

private:
    class ScanThread;

    std::unique_ptr<juce::PropertiesFile> settings;
    std::shared_ptr<const tt::LibraryIndex> index;
    std::unique_ptr<ScanThread> scanThread;
    std::unordered_set<juce::String> favourites;
    tt::GrooveCache cache;
    juce::ThreadPool loaderPool { juce::ThreadPoolOptions{}.withThreadName ("GrooveLoader").withNumberOfThreads (2) };
    juce::String lastScanError;

    static juce::File getIndexCacheFile() { return getDataFolder().getChildFile ("library-index.bin"); }
    void indexReady (std::unique_ptr<tt::LibraryIndex>);
    void saveFavourites();

    JUCE_DECLARE_WEAK_REFERENCEABLE (AppContext)
    JUCE_DECLARE_NON_COPYABLE (AppContext)
};
