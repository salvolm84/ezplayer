#include "MidiDatabase.h"

#include <sqlite3.h>

namespace tt
{
    namespace
    {
        struct Db
        {
            sqlite3* handle = nullptr;
            ~Db() { if (handle != nullptr) sqlite3_close (handle); }
        };

        struct Stmt
        {
            sqlite3_stmt* s = nullptr;
            Stmt (sqlite3* db, const char* sql) { sqlite3_prepare_v2 (db, sql, -1, &s, nullptr); }
            ~Stmt() { if (s != nullptr) sqlite3_finalize (s); }
            bool ok() const { return s != nullptr; }
            bool step() { return sqlite3_step (s) == SQLITE_ROW; }

            juce::String text (int col) const
            {
                auto* t = reinterpret_cast<const char*> (sqlite3_column_text (s, col));
                return t != nullptr ? juce::String::fromUTF8 (t) : juce::String();
            }

            int integer (int col) const { return sqlite3_column_int (s, col); }
            int64_t int64 (int col) const { return sqlite3_column_int64 (s, col); }
        };

        juce::String percentEncodePath (const juce::String& path)
        {
            juce::String out;
            auto utf8 = path.toRawUTF8();

            for (auto* p = utf8; *p != 0; ++p)
            {
                auto c = (unsigned char) *p;
                if (juce::CharacterFunctions::isLetterOrDigit ((char) c) || c == '/' || c == '-' || c == '.' || c == '_')
                    out << (char) c;
                else
                    out << '%' << juce::String::toHexString ((int) c).paddedLeft ('0', 2).toUpperCase();
            }

            return out;
        }

        bool openReadOnly (Db& db, const juce::File& file)
        {
            // "immutable=1" avoids any locking/journal activity inside the (possibly
            // iCloud-synced) library folder.
            auto uri = "file:" + percentEncodePath (file.getFullPathName()) + "?immutable=1";

            if (sqlite3_open_v2 (uri.toRawUTF8(), &db.handle, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr) == SQLITE_OK)
                return true;

            if (db.handle != nullptr) { sqlite3_close (db.handle); db.handle = nullptr; }

            return sqlite3_open_v2 (file.getFullPathName().toRawUTF8(), &db.handle, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK;
        }

        // DB paths look like "_EZD2MIDI_<library folder>/406@SWING_4#4/090-S012@VERSE/".
        // The first component names the library folder (which does not always match the
        // folder name on disk), so the remainder is relative to the midiDB's folder.
        juce::String relativeFromDbPath (const juce::String& dbPath)
        {
            auto p = dbPath.replaceCharacter ('\\', '/');
            auto slash = p.indexOfChar ('/');
            return slash >= 0 ? p.substring (slash + 1) : juce::String();
        }
    }

    bool MidiDatabase::load (const juce::File& dbFile, juce::String& error)
    {
        Db db;
        if (! openReadOnly (db, dbFile))
        {
            error = "Cannot open " + dbFile.getFullPathName();
            return false;
        }

        {
            Stmt q (db.handle, "SELECT FormattedName FROM LIBRARY LIMIT 1");
            if (q.ok() && q.step())
                libraryName = q.text (0);
        }

        // Tags per entity
        std::unordered_map<int64_t, juce::StringArray> tagsByEntity;
        {
            Stmt q (db.handle,
                    "SELECT r.EntID, t.Name, g.Name FROM REL_ENT_TAGS r "
                    "JOIN TAGS t ON t.ID = r.TagID LEFT JOIN TAGGROUP g ON g.ID = t.GroupID");

            if (q.ok())
            {
                while (q.step())
                {
                    auto tag = q.text (1).trim();
                    if (tag.isEmpty())
                        continue;

                    tagsByEntity[q.int64 (0)].addIfNotAlreadyThere (tag);

                    if (q.text (2) == "Play Style")
                        playStyleTags.addIfNotAlreadyThere (tag);
                }
            }
        }

        Stmt q (db.handle,
                "SELECT e.ID, p.Path, f.Name, e.Name, e.Tempo, e.Length, ts.Name, e.Intensity, g.Name "
                "FROM MIDIENTITY e "
                "JOIN PATHS p ON p.ID = e.PathID "
                "JOIN FILENAMES f ON f.ID = e.FilenameID "
                "LEFT JOIN TIMESIGS ts ON ts.ID = e.TimeSigID "
                "LEFT JOIN GENRE g ON g.ID = e.GenreID");

        if (! q.ok())
        {
            error = "Unexpected midiDB schema in " + dbFile.getFullPathName();
            return false;
        }

        while (q.step())
        {
            DbEntry e;
            auto rel = relativeFromDbPath (q.text (1));
            if (rel.isNotEmpty() && ! rel.endsWithChar ('/'))
                rel << '/';
            rel << q.text (2);

            e.name = q.text (3).trim();
            e.tempo = q.integer (4);
            e.lengthBeats = q.integer (5);

            auto sig = q.text (6);
            if (sig.containsChar ('/'))
            {
                e.num = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue();
                e.den = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
            }

            e.intensity = q.integer (7);
            e.genre = q.text (8);

            if (auto it = tagsByEntity.find (q.int64 (0)); it != tagsByEntity.end())
                e.tags = it->second;

            entries[rel.toLowerCase()] = std::move (e);
        }

        return true;
    }

    const DbEntry* MidiDatabase::find (const juce::String& relativePath) const
    {
        auto it = entries.find (relativePath.toLowerCase());
        return it != entries.end() ? &it->second : nullptr;
    }
}
