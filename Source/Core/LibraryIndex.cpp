#include "LibraryIndex.h"
#include "MidiDatabase.h"
#include "ToontrackNaming.h"

namespace tt
{
    //==============================================================================
    juce::uint64 LibraryIndex::tagMask (const juce::String& name) const
    {
        auto bit = getTagBit (name);
        return bit >= 0 ? (juce::uint64 (1) << bit) : 0;
    }

    int LibraryIndex::addTag (const juce::String& name)
    {
        auto bit = getTagBit (name);
        if (bit >= 0)
            return bit;

        if (tagNames.size() >= maxTags)
            return -1;

        tagNames.add (name);
        return tagNames.size() - 1;
    }

    juce::File LibraryIndex::getFile (int i) const
    {
        if (! juce::isPositiveAndBelow (i, (int) grooves.size()))
            return {};

        auto& g = grooves[(size_t) i];
        return juce::File (folders[(size_t) g.folder].path).getChildFile (g.fileName);
    }

    juce::String LibraryIndex::getFolderPathText (int f) const
    {
        juce::StringArray parts;
        for (; f > 0; f = folders[(size_t) f].parent)
        {
            parts.insert (0, folders[(size_t) f].name);
            if (folders[(size_t) f].section.isNotEmpty())
                parts.insert (0, folders[(size_t) f].section);
        }

        return parts.joinIntoString (" / ");
    }

    juce::String LibraryIndex::getLocation (int i) const
    {
        if (! juce::isPositiveAndBelow (i, (int) grooves.size()))
            return {};

        // Skip the library root folder(s) themselves to keep it short.
        juce::StringArray parts;
        for (int f = grooves[(size_t) i].folder; f > 0 && folders[(size_t) f].parent > 0; f = folders[(size_t) f].parent)
        {
            parts.insert (0, folders[(size_t) f].name);
            if (folders[(size_t) f].section.isNotEmpty())
                parts.insert (0, folders[(size_t) f].section);
        }

        return parts.joinIntoString (" / ");
    }

    juce::String LibraryIndex::getTypeText (const GrooveInfo& g) const
    {
        juce::StringArray t;
        for (int b = 0; b < tagNames.size(); ++b)
            if ((g.tags >> b) & 1)
                t.add (tagNames[b]);

        // Most useful tags first
        juce::StringArray order { "Fill", "Beat", "Ending", "Straight", "Swing", "Half Time", "Double Time" };
        juce::StringArray sorted;
        for (auto& o : order)
            if (t.contains (o)) { sorted.add (o); t.removeString (o); }

        t.removeString ("Normal Time");
        t.removeString ("Standard");
        sorted.addArray (t);
        return sorted.joinIntoString (", ");
    }

    int LibraryIndex::findFolderByPath (const juce::String& p) const
    {
        for (size_t i = 1; i < folders.size(); ++i)
            if (folders[i].path == p)
                return (int) i;
        return -1;
    }

    int LibraryIndex::findGrooveByFile (const juce::File& file) const
    {
        auto parentPath = file.getParentDirectory().getFullPathName();
        auto f = findFolderByPath (parentPath);
        if (f < 0)
            return -1;

        auto& fo = folders[(size_t) f];
        for (int i = fo.firstFile; i < fo.firstFile + fo.numFiles; ++i)
            if (grooves[(size_t) i].fileName == file.getFileName())
                return i;

        return -1;
    }

    int LibraryIndex::getNumPlayableGrooves() const
    {
        int n = 0;
        for (auto& g : grooves)
            n += g.isHeading ? 0 : 1;
        return n;
    }

    void LibraryIndex::finalise()
    {
        for (size_t i = 0; i < folders.size(); ++i)
        {
            auto& f = folders[i];
            juce::String s;
            if (f.parent > 0)
                s = folders[(size_t) f.parent].searchText + " ";
            if (f.section.isNotEmpty())
                s << f.section.toLowerCase() << " ";
            if (i > 0)
                s << f.name.toLowerCase();
            f.searchText = s;
        }

        juce::StringArray sigs;
        for (auto& fo : folders)
        {
            juce::String heading;
            for (int i = fo.firstFile; i < fo.firstFile + fo.numFiles; ++i)
            {
                auto& g = grooves[(size_t) i];
                if (g.isHeading)
                    heading = g.name.toLowerCase();

                g.nameLower = heading.isEmpty() || g.isHeading ? g.name.toLowerCase()
                                                               : g.name.toLowerCase() + " " + heading;

                if (! g.isHeading && g.num > 0 && g.den > 0)
                    sigs.addIfNotAlreadyThere (juce::String (g.num) + "/" + juce::String (g.den));
            }
        }

        std::sort (sigs.begin(), sigs.end(), [] (const juce::String& a, const juce::String& b)
        {
            auto da = a.fromFirstOccurrenceOf ("/", false, false).getIntValue();
            auto db = b.fromFirstOccurrenceOf ("/", false, false).getIntValue();
            if (da != db) return da < db;
            return a.getIntValue() < b.getIntValue();
        });

        timeSignatures = sigs;

        std::vector<int> prefix (grooves.size() + 1, 0);
        for (size_t i = 0; i < grooves.size(); ++i)
            prefix[i + 1] = prefix[i] + (grooves[i].isHeading ? 0 : 1);

        for (auto& f : folders)
            f.numGrooves = prefix[(size_t) f.descEnd] - prefix[(size_t) f.descBegin];
    }

    std::vector<int> LibraryIndex::search (const SearchFilter& filter, const std::function<bool (int)>& isFavourite) const
    {
        std::vector<int> result;

        auto scope = juce::isPositiveAndBelow (filter.scopeFolder, (int) folders.size()) ? filter.scopeFolder : 0;
        auto& sf = folders[(size_t) scope];

        juce::StringArray tokens;
        tokens.addTokens (filter.text.toLowerCase(), " ", "\"");
        tokens.removeEmptyStrings();

        auto beatMask = tagMask ("Beat");
        auto fillMask = tagMask ("Fill");

        for (int i = sf.descBegin; i < sf.descEnd; ++i)
        {
            auto& g = grooves[(size_t) i];
            if (g.isHeading)
                continue;

            if (filter.type == SearchFilter::Type::fills && (g.tags & fillMask) == 0) continue;
            if (filter.type == SearchFilter::Type::beats && ((g.tags & fillMask) != 0 || (beatMask != 0 && (g.tags & beatMask) == 0))) continue;
            if ((g.tags & filter.requiredTags) != filter.requiredTags) continue;
            if (filter.num != 0 && (g.num != filter.num || g.den != filter.den)) continue;
            if (filter.tempoMin > 0.0 && (g.tempo <= 0.0f || g.tempo < filter.tempoMin)) continue;
            if (filter.tempoMax > 0.0 && (g.tempo <= 0.0f || g.tempo > filter.tempoMax)) continue;

            bool textOk = true;
            for (auto& t : tokens)
            {
                if (! g.nameLower.contains (t) && ! folders[(size_t) g.folder].searchText.contains (t))
                {
                    textOk = false;
                    break;
                }
            }

            if (! textOk)
                continue;

            if (filter.favouritesOnly && ! (isFavourite && isFavourite (i)))
                continue;

            result.push_back (i);
        }

        return result;
    }

    //==============================================================================
    static constexpr int indexMagic = 0x47504958; // "GPIX"
    static constexpr int indexVersion = 3;

    bool LibraryIndex::save (const juce::File& file) const
    {
        juce::MemoryOutputStream out;
        out.writeInt (indexMagic);
        out.writeInt (indexVersion);

        auto writeStrings = [&out] (const juce::StringArray& a)
        {
            out.writeInt (a.size());
            for (auto& s : a)
                out.writeString (s);
        };

        writeStrings (roots);
        writeStrings (tagNames);
        writeStrings (playStyles);

        out.writeInt ((int) folders.size());
        for (auto& f : folders)
        {
            out.writeString (f.name);
            out.writeString (f.rawName);
            out.writeString (f.path);
            out.writeString (f.section);
            out.writeInt (f.parent);
            out.writeByte ((char) ((f.isHeading ? 1 : 0) | (f.isLibrary ? 2 : 0)));
            out.writeInt ((int) f.children.size());
            for (auto c : f.children)
                out.writeInt (c);
            out.writeInt (f.firstFile);
            out.writeInt (f.numFiles);
            out.writeInt (f.descBegin);
            out.writeInt (f.descEnd);
        }

        out.writeInt ((int) grooves.size());
        for (auto& g : grooves)
        {
            out.writeString (g.name);
            out.writeString (g.fileName);
            out.writeInt (g.folder);
            out.writeFloat (g.tempo);
            out.writeFloat (g.lengthBeats);
            out.writeByte ((char) g.num);
            out.writeByte ((char) g.den);
            out.writeByte ((char) g.intensity);
            out.writeByte ((char) ((g.isHeading ? 1 : 0) | (g.fromDb ? 2 : 0)));
            out.writeInt64 ((juce::int64) g.tags);
        }

        file.getParentDirectory().createDirectory();
        auto tmp = file.getSiblingFile (file.getFileName() + ".tmp");
        if (! tmp.replaceWithData (out.getData(), out.getDataSize()))
            return false;

        return tmp.moveFileTo (file);
    }

    std::unique_ptr<LibraryIndex> LibraryIndex::load (const juce::File& file)
    {
        juce::MemoryBlock data;
        if (! file.loadFileAsData (data))
            return nullptr;

        juce::MemoryInputStream in (data, false);
        if (in.readInt() != indexMagic || in.readInt() != indexVersion)
            return nullptr;

        auto idx = std::make_unique<LibraryIndex>();

        auto readStrings = [&in] (juce::StringArray& a)
        {
            auto n = in.readInt();
            for (int i = 0; i < n && ! in.isExhausted(); ++i)
                a.add (in.readString());
        };

        readStrings (idx->roots);
        readStrings (idx->tagNames);
        readStrings (idx->playStyles);

        auto nf = in.readInt();
        if (nf <= 0 || nf > 10'000'000)
            return nullptr;

        idx->folders.resize ((size_t) nf);
        for (auto& f : idx->folders)
        {
            f.name = in.readString();
            f.rawName = in.readString();
            f.path = in.readString();
            f.section = in.readString();
            f.parent = in.readInt();
            auto flags = (int) in.readByte();
            f.isHeading = (flags & 1) != 0;
            f.isLibrary = (flags & 2) != 0;
            auto nc = in.readInt();
            if (nc < 0 || nc > nf)
                return nullptr;
            f.children.resize ((size_t) nc);
            for (auto& c : f.children)
                c = in.readInt();
            f.firstFile = in.readInt();
            f.numFiles = in.readInt();
            f.descBegin = in.readInt();
            f.descEnd = in.readInt();
        }

        auto ng = in.readInt();
        if (ng < 0 || ng > 50'000'000)
            return nullptr;

        idx->grooves.resize ((size_t) ng);
        for (auto& g : idx->grooves)
        {
            g.name = in.readString();
            g.fileName = in.readString();
            g.folder = in.readInt();
            g.tempo = in.readFloat();
            g.lengthBeats = in.readFloat();
            g.num = (juce::uint8) in.readByte();
            g.den = (juce::uint8) in.readByte();
            g.intensity = (juce::uint8) in.readByte();
            auto flags = (int) in.readByte();
            g.isHeading = (flags & 1) != 0;
            g.fromDb = (flags & 2) != 0;
            g.tags = (juce::uint64) in.readInt64();
        }

        if (in.getNumBytesRemaining() != 0)
            return nullptr;

        for (auto& g : idx->grooves)
            if (! juce::isPositiveAndBelow (g.folder, nf))
                return nullptr;

        idx->finalise();
        return idx;
    }

    //==============================================================================
    namespace
    {
        struct DbContext
        {
            MidiDatabase db;
            juce::File base;
        };

        struct Inherited
        {
            double tempo = 0.0;
            int num = 0, den = 0;
            juce::String chainLower; // ancestor names, lower case
        };

        class Scanner
        {
        public:
            Scanner (LibraryIndex& i, ScanProgress& p, const std::function<bool()>& e)
                : idx (i), progress (p), shouldExit (e) {}

            void scanRoot (const juce::File& dir)
            {
                Inherited inh;
                scanDir (dir, 0, nullptr, inh, 0, true);
            }

        private:
            LibraryIndex& idx;
            ScanProgress& progress;
            const std::function<bool()>& shouldExit;

            void setTag (GrooveInfo& g, const juce::String& name)
            {
                auto bit = idx.addTag (name);
                if (bit >= 0)
                    g.tags |= (juce::uint64 (1) << bit);
            }

            void inferTags (GrooveInfo& g, const juce::String& chainLower)
            {
                auto all = chainLower + " " + g.name.toLowerCase();

                if (all.contains ("fill"))      setTag (g, "Fill");
                else                            setTag (g, "Beat");

                if (all.contains ("swing") || all.contains ("shuffle")) setTag (g, "Swing");
                else if (all.contains ("straight"))                     setTag (g, "Straight");

                if (all.contains ("half time") || all.contains ("halftime") || all.contains ("half-time"))
                    setTag (g, "Half Time");
                if (all.contains ("double time") || all.contains ("doubletime"))
                    setTag (g, "Double Time");
                if (all.contains ("ending") || all.contains ("outro"))
                    setTag (g, "Ending");
            }

            static juce::String relativePath (const juce::File& file, const juce::File& base)
            {
                return file.getRelativePathFrom (base).replaceCharacter ('\\', '/');
            }

            void scanDir (const juce::File& dir, int parent, const DbContext* db, Inherited inh, int depth, bool isRoot)
            {
                if (depth > 32 || (shouldExit && shouldExit()))
                    return;

                juce::Array<juce::File> subdirs;
                juce::StringArray files;
                bool hasHeader = false, hasDb = false;

                for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*",
                                                                         juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
                {
                    auto f = entry.getFile();
                    auto name = f.getFileName();

                    if (entry.isDirectory())
                    {
                        if (! name.startsWithChar ('.'))
                            subdirs.add (f);
                        continue;
                    }

                    if (name == "header")                          hasHeader = true;
                    else if (name == "midiDB")                     hasDb = true;
                    else if (isMidiFileName (name) || isHeadFileName (name)) files.add (name);
                }

                FolderInfo fo;
                fo.rawName = dir.getFileName();
                auto decoded = decodeName (fo.rawName);
                fo.name = decoded.display;
                fo.path = dir.getFullPathName();
                fo.parent = parent;

                std::unique_ptr<DbContext> ownDb;
                if (hasDb)
                {
                    ownDb = std::make_unique<DbContext>();
                    ownDb->base = dir;
                    juce::String err;
                    if (ownDb->db.load (dir.getChildFile ("midiDB"), err))
                    {
                        db = ownDb.get();
                        fo.isLibrary = true;
                        if (ownDb->db.libraryName.isNotEmpty())
                            fo.name = ownDb->db.libraryName;

                        for (auto& s : ownDb->db.playStyleTags)
                        {
                            idx.addTag (s);
                            idx.playStyles.addIfNotAlreadyThere (s);
                        }
                    }
                    else
                    {
                        ownDb.reset();
                    }
                }

                fo.isHeading = hasHeader && files.isEmpty() && subdirs.isEmpty();

                // Inherit meter/tempo hints from folder names
                if (! isRoot)
                {
                    int n = 0, d = 0;
                    if (timeSigFromName (fo.rawName.fromFirstOccurrenceOf ("@", false, false), n, d)
                        || (! fo.rawName.containsChar ('@') && timeSigFromName (fo.rawName, n, d)))
                    {
                        inh.num = n;
                        inh.den = d;
                    }

                    if (auto t = tempoFromName (decoded.display, decoded.sortKey); t > 0.0)
                        inh.tempo = t;

                    inh.chainLower << " " << fo.name.toLowerCase();
                }

                const int me = (int) idx.folders.size();
                idx.folders.push_back (std::move (fo));
                if (parent >= 0)
                    idx.folders[(size_t) parent].children.push_back (me);

                progress.foldersFound++;

                std::sort (files.begin(), files.end(), [] (const juce::String& a, const juce::String& b)
                           { return compareRawNames (a, b) < 0; });

                idx.folders[(size_t) me].firstFile = (int) idx.grooves.size();
                idx.folders[(size_t) me].descBegin = (int) idx.grooves.size();

                double headingTempo = 0.0;
                juce::String relDir = db != nullptr && dir != db->base ? relativePath (dir, db->base) + "/" : juce::String();

                for (auto& name : files)
                {
                    GrooveInfo g;
                    g.fileName = name;
                    g.folder = me;

                    auto base = name.upToLastOccurrenceOf (".", false, false);
                    auto dn = decodeName (base);

                    if (isHeadFileName (name))
                    {
                        g.isHeading = true;
                        g.name = dn.display;
                        headingTempo = tempoFromName (dn.display, dn.sortKey);
                        idx.grooves.push_back (std::move (g));
                        continue;
                    }

                    const DbEntry* e = db != nullptr ? db->db.find (relDir + name) : nullptr;

                    if (e != nullptr)
                    {
                        g.fromDb = true;
                        g.name = e->name.isNotEmpty() ? e->name : dn.display;
                        g.tempo = (float) e->tempo;
                        g.lengthBeats = (float) e->lengthBeats;
                        g.num = (juce::uint8) e->num;
                        g.den = (juce::uint8) e->den;
                        g.intensity = (juce::uint8) juce::jlimit (0, 255, e->intensity);

                        for (auto& t : e->tags)
                            setTag (g, t);

                        // Some DBs have no Beat/Fill tag on every entry
                        if (g.tags == 0)
                            inferTags (g, inh.chainLower);
                    }
                    else
                    {
                        g.name = dn.display;
                        auto t = tempoFromName (dn.display, dn.sortKey);
                        if (t <= 0.0) t = headingTempo;
                        if (t <= 0.0) t = inh.tempo;
                        g.tempo = (float) t;
                        g.num = (juce::uint8) inh.num;
                        g.den = (juce::uint8) inh.den;
                        inferTags (g, inh.chainLower);
                    }

                    if (g.tempo <= 0.0f && headingTempo > 0.0)
                        g.tempo = (float) headingTempo;

                    idx.grooves.push_back (std::move (g));
                    progress.filesFound++;
                }

                idx.folders[(size_t) me].numFiles = (int) idx.grooves.size() - idx.folders[(size_t) me].firstFile;

                std::sort (subdirs.begin(), subdirs.end(), [] (const juce::File& a, const juce::File& b)
                           { return compareRawNames (a.getFileName(), b.getFileName()) < 0; });

                // Heading folders title the siblings that follow them.
                juce::String section;
                for (auto& sd : subdirs)
                {
                    auto childIndex = idx.folders.size();
                    scanDir (sd, me, db, inh, depth + 1, false);

                    if (childIndex < idx.folders.size())
                    {
                        auto& child = idx.folders[childIndex];
                        if (child.isHeading)
                            section = child.name;
                        else
                            child.section = section;
                    }
                }

                idx.folders[(size_t) me].descEnd = (int) idx.grooves.size();
            }
        };
    }

    std::unique_ptr<LibraryIndex> scanLibrary (const juce::StringArray& roots,
                                               ScanProgress& progress,
                                               const std::function<bool()>& shouldExit)
    {
        auto idx = std::make_unique<LibraryIndex>();
        idx->roots = roots;

        // Make the common tags occupy stable low bits.
        for (auto* t : { "Beat", "Fill", "Straight", "Swing", "Half Time", "Double Time", "Ending" })
            idx->addTag (t);

        FolderInfo root;
        root.name = "All Libraries";
        idx->folders.push_back (root);

        Scanner scanner (*idx, progress, shouldExit);

        for (auto& r : roots)
        {
            juce::File dir (r);
            if (dir.isDirectory())
                scanner.scanRoot (dir);

            if (shouldExit && shouldExit())
                return nullptr;
        }

        idx->folders[0].descBegin = 0;
        idx->folders[0].descEnd = (int) idx->grooves.size();
        idx->finalise();
        return idx;
    }
}
