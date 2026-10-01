#include "AppContext.h"

namespace
{
    const juce::String defaultLibraryRoot =
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile ("Library/Mobile Documents/com~apple~CloudDocs/Cubase/drums midi/Toontrack")
            .getFullPathName();
}

//==============================================================================
class AppContext::ScanThread : public juce::Thread
{
public:
    ScanThread (AppContext& o, juce::StringArray r)
        : juce::Thread ("GroovePlayer library scan"), owner (&o), roots (std::move (r)) {}

    ~ScanThread() override { stopThread (10000); }

    void run() override
    {
        auto idx = tt::scanLibrary (roots, progress, [this] { return threadShouldExit(); });
        if (idx == nullptr || threadShouldExit())
            return;

        idx->save (AppContext::getIndexCacheFile());

        auto shared = std::make_shared<std::unique_ptr<tt::LibraryIndex>> (std::move (idx));
        juce::MessageManager::callAsync ([ref = owner, shared]
        {
            if (auto* o = ref.get())
                o->indexReady (std::move (*shared));
        });
    }

    tt::ScanProgress progress;

private:
    juce::WeakReference<AppContext> owner;
    juce::StringArray roots;
};

//==============================================================================
AppContext::AppContext()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "GroovePlayer";
    o.filenameSuffix = ".settings";
    o.folderName = "SLM/GroovePlayer";
    o.osxLibrarySubFolder = "Application Support";
    o.processLock = nullptr;
    settings = std::make_unique<juce::PropertiesFile> (o);

    juce::StringArray favs;
    favs.addLines (settings->getValue ("favourites"));
    favs.removeEmptyStrings();
    for (auto& f : favs)
        favourites.insert (f);

    if (! settings->containsKey ("libraryRoots") && juce::File (defaultLibraryRoot).isDirectory())
        settings->setValue ("libraryRoots", defaultLibraryRoot);

    // Load the cached index instantly, scan only when it is missing or stale.
    auto cached = tt::LibraryIndex::load (getIndexCacheFile());
    if (cached != nullptr && cached->roots == getLibraryRoots())
        index = std::shared_ptr<const tt::LibraryIndex> (std::move (cached));
    else
        rescan();
}

AppContext::~AppContext()
{
    scanThread.reset();
    loaderPool.removeAllJobs (true, 5000);
    settings->saveIfNeeded();
}

juce::File AppContext::getDataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support/SLM/GroovePlayer");
}

juce::StringArray AppContext::getLibraryRoots() const
{
    juce::StringArray roots;
    roots.addLines (settings->getValue ("libraryRoots"));
    roots.removeEmptyStrings();
    return roots;
}

void AppContext::setLibraryRoots (const juce::StringArray& roots)
{
    settings->setValue ("libraryRoots", roots.joinIntoString ("\n"));
    settings->saveIfNeeded();
    rescan();
}

void AppContext::rescan()
{
    scanThread.reset();
    lastScanError.clear();

    auto roots = getLibraryRoots();
    if (roots.isEmpty())
    {
        auto empty = std::make_unique<tt::LibraryIndex>();
        empty->folders.push_back ({});
        empty->folders[0].name = "All Libraries";
        empty->finalise();
        indexReady (std::move (empty));
        return;
    }

    scanThread = std::make_unique<ScanThread> (*this, roots);
    scanThread->startThread (juce::Thread::Priority::background);
    sendChangeMessage();
}

bool AppContext::isScanning() const
{
    return scanThread != nullptr && scanThread->isThreadRunning();
}

juce::String AppContext::getStatusText() const
{
    if (isScanning())
        return "Scanning library... " + juce::String (scanThread->progress.filesFound.load()) + " grooves";

    if (index == nullptr || index->folders.size() <= 1)
        return getLibraryRoots().isEmpty() ? "No library folder set - open Settings" : "Library is empty";

    return juce::String (index->getNumPlayableGrooves()) + " grooves";
}

void AppContext::indexReady (std::unique_ptr<tt::LibraryIndex> idx)
{
    index = std::shared_ptr<const tt::LibraryIndex> (std::move (idx));
    sendChangeMessage();
}

//==============================================================================
bool AppContext::isFavourite (int grooveIndex) const
{
    return index != nullptr && isFavourite (index->getFile (grooveIndex).getFullPathName());
}

void AppContext::toggleFavourite (int grooveIndex)
{
    if (index == nullptr)
        return;

    auto path = index->getFile (grooveIndex).getFullPathName();
    if (path.isEmpty())
        return;

    if (! favourites.erase (path))
        favourites.insert (path);

    saveFavourites();
}

void AppContext::saveFavourites()
{
    juce::StringArray a;
    for (auto& f : favourites)
        a.add (f);
    a.sort (true);
    settings->setValue ("favourites", a.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

//==============================================================================
tt::Groove::Ptr AppContext::loadGrooveFile (const juce::File& file, juce::String& error)
{
    if (auto g = cache.get (file.getFullPathName()))
        return g;

    auto g = tt::Groove::loadFromFile (file, {}, 0.0, error);
    cache.put (g);
    return g;
}

tt::Groove::Ptr AppContext::loadGroove (int i, juce::String& error)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (i, (int) index->grooves.size()))
    {
        error = "Groove not found";
        return nullptr;
    }

    auto file = index->getFile (i);
    if (auto g = cache.get (file.getFullPathName()))
        return g;

    auto& info = index->grooves[(size_t) i];
    auto g = tt::Groove::loadFromFile (file, info.name, info.lengthBeats, error);
    if (g != nullptr && g->tempo <= 0.0)
        g->tempo = info.tempo;

    cache.put (g);
    return g;
}

void AppContext::loadGrooveAsync (int i, std::function<void (tt::Groove::Ptr, juce::String)> onLoaded)
{
    if (index == nullptr || ! juce::isPositiveAndBelow (i, (int) index->grooves.size()))
        return;

    auto file = index->getFile (i);
    if (auto g = cache.get (file.getFullPathName()))
    {
        onLoaded (g, {});
        return;
    }

    auto info = index->grooves[(size_t) i];
    juce::WeakReference<AppContext> ref (this);

    loaderPool.addJob ([ref, file, info, callback = std::move (onLoaded)]
    {
        juce::String err;
        auto g = tt::Groove::loadFromFile (file, info.name, info.lengthBeats, err);
        if (g != nullptr && g->tempo <= 0.0)
            g->tempo = info.tempo;

        juce::MessageManager::callAsync ([ref, g, err, callback]
        {
            if (auto* o = ref.get())
            {
                o->cache.put (g);
                callback (g, err);
            }
        });
    });
}

//==============================================================================
juce::Array<tt::DrumMap> AppContext::getAllRemapPresets() const
{
    auto presets = tt::DrumMap::builtInPresets();

    auto files = getRemapFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (auto& f : files)
    {
        tt::DrumMap m;
        if (tt::DrumMap::loadFromFile (f, m))
            presets.add (m);
    }

    return presets;
}

bool AppContext::getAutoPreview() const { return settings->getBoolValue ("autoPreview", true); }

void AppContext::setAutoPreview (bool b)
{
    settings->setValue ("autoPreview", b);
    settings->saveIfNeeded();
}
