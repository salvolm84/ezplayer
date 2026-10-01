// Console test runner for the GroovePlayer core (no GUI / plugin dependencies).
//   GroovePlayerTests            -> unit tests (+ real-library checks if the library exists)
//   GroovePlayerTests --scan     -> also scans the whole library and benchmarks search
//   GroovePlayerTests --scan <dir>

#include "Core/ToontrackNaming.h"
#include "Core/MidiDatabase.h"
#include "Core/LibraryIndex.h"
#include "Core/Groove.h"
#include "Core/DrumMap.h"
#include "Core/Song.h"
#include "Core/PlaybackEngine.h"
#include "Core/MidiExport.h"

#include <iostream>

using namespace tt;

static int failures = 0, checks = 0;

#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::cout << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; auto _a = (a); auto _b = (b); if (std::abs (_a - _b) > (tol)) { ++failures; std::cout << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #a " = " << _a << ", expected " << _b << "\n"; } } while (0)

static const juce::String defaultLibrary = "/Users/salvolm/Library/Mobile Documents/com~apple~CloudDocs/Cubase/drums midi/Toontrack";

//==============================================================================
static void testNaming()
{
    std::cout << "naming\n";
    auto d = decodeName ("406@SWING_4#4");
    CHECK (d.sortKey == "406");
    CHECK (d.display == "SWING 4/4");
    CHECK (! d.headingMarker);

    d = decodeName ("300@_SLOW_MIDTEMPO");
    CHECK (d.headingMarker);
    CHECK (d.display == "SLOW MIDTEMPO");

    d = decodeName ("GROOVE 22");
    CHECK (d.sortKey.isEmpty());
    CHECK (d.display == "GROOVE 22");

    d = decodeName ("M@13._Hats_8th_Rock_Open#Ghost");
    CHECK (d.display == "13. Hats 8th Rock Open/Ghost");

    CHECK_NEAR (tempoFromName ("SMOKEY IKO 90 BPM"), 90.0, 0.0);
    CHECK_NEAR (tempoFromName ("_92_BPM"), 92.0, 0.0);
    CHECK_NEAR (tempoFromName ("VERSE", "090-S012"), 90.0, 0.0);
    CHECK_NEAR (tempoFromName ("150 6-4 Fill 13"), 150.0, 0.0);
    CHECK_NEAR (tempoFromName ("089 West Coast 06 Fill 2"), 89.0, 0.0);
    CHECK_NEAR (tempoFromName ("Variation 01"), 0.0, 0.0);
    CHECK_NEAR (tempoFromName ("EZ Demo Straight - 120BPM"), 120.0, 0.0);

    int n = 0, den = 0;
    CHECK (timeSigFromName ("STRAIGHT_4#4", n, den) && n == 4 && den == 4);
    CHECK (timeSigFromName ("SWING_12#8", n, den) && n == 12 && den == 8);
    CHECK (timeSigFromName ("6-4 FILLS", n, den) && n == 6 && den == 4);
    CHECK (! timeSigFromName ("120-S030", n, den));
    CHECK (! timeSigFromName ("METAL_(1#2)", n, den));

    CHECK (compareRawNames ("090-S010@_X", "090-S011@INTRO") < 0);
    CHECK (compareRawNames ("M@2._A", "M@13._B") < 0);
}

static void testLengths()
{
    std::cout << "length resolution\n";
    CHECK_NEAR (Groove::resolveLength (307199.0 / 9600.0, 31.0, 0.0), 32.0, 1e-9);
    CHECK_NEAR (Groove::resolveLength (951.0 / 240.0, 3.5, 0.0), 4.0, 1e-9);
    CHECK_NEAR (Groove::resolveLength (1372.0 / 240.0, 5.0, 0.0), 6.0, 1e-9);
    CHECK_NEAR (Groove::resolveLength (959.0 / 480.0, 1.5, 0.0), 2.0, 1e-9);
    CHECK_NEAR (Groove::resolveLength (0.0, 3.2, 0.0), 4.0, 1e-9);
    CHECK_NEAR (Groove::resolveLength (5.0, 2.0, 8.0), 8.0, 1e-9);
}

//==============================================================================
static Groove::Ptr makeGroove (int beats, int note = 36)
{
    Groove::Ptr g = new Groove();
    g->name = "Test";
    g->lengthBeats = beats;
    for (int i = 0; i < beats; ++i)
        g->notes.push_back ({ (double) i, 0.25, (juce::uint8) (note + (i % 2)), 100 });
    return g;
}

struct Rendered
{
    struct Ev { juce::int64 sample; bool on; int note; int vel; int ch; };
    std::vector<Ev> events;
    int ons() const { int n = 0; for (auto& e : events) n += e.on; return n; }
    int offs() const { int n = 0; for (auto& e : events) n += ! e.on; return n; }
};

// Renders `totalBeats` of host playback starting at `startPpq`, with random block sizes.
static void renderHost (PlaybackEngine& eng, Rendered& r, double sr, double bpm, double startPpq,
                        double totalBeats, bool playing, juce::int64& sampleClock, juce::Random& rnd)
{
    const auto samplesPerBeat = sr * 60.0 / bpm;
    const auto total = (juce::int64) std::llround (totalBeats * samplesPerBeat);
    juce::int64 done = 0;

    while (done < total)
    {
        auto n = (int) juce::jmin<juce::int64> (total - done, 1 + rnd.nextInt (1024));
        PlaybackEngine::HostInfo h;
        h.isPlaying = playing;
        h.bpm = bpm;
        h.ppq = startPpq + done / samplesPerBeat;
        h.barStartPpq = std::floor (h.ppq / 4.0) * 4.0;

        juce::MidiBuffer buf;
        eng.process (buf, n, h);

        for (const auto meta : buf)
        {
            auto m = meta.getMessage();
            CHECK (meta.samplePosition >= 0 && meta.samplePosition < n);
            if (m.isNoteOn())       r.events.push_back ({ sampleClock + meta.samplePosition, true, m.getNoteNumber(), m.getVelocity(), m.getChannel() });
            else if (m.isNoteOff()) r.events.push_back ({ sampleClock + meta.samplePosition, false, m.getNoteNumber(), 0, m.getChannel() });
        }

        done += n;
        sampleClock += n;
    }
}

static void testEngineSong()
{
    std::cout << "engine: song follows host\n";
    juce::Random rnd (42);
    const double sr = 48000.0, bpm = 120.0;

    Song song;
    song.addClip (makeGroove (4), 0.0, 8.0); // loops twice

    PlaybackEngine eng;
    eng.prepare (sr);
    eng.setSongTimeline (song.buildTimeline());

    Rendered r;
    juce::int64 clock = 0;
    renderHost (eng, r, sr, bpm, 0.0, 10.0, true, clock, rnd);

    CHECK (r.ons() == 8);
    CHECK (r.offs() == 8);

    int idx = 0;
    for (auto& e : r.events)
    {
        if (! e.on)
            continue;
        CHECK (e.sample == (juce::int64) (idx * 24000));
        CHECK (e.ch == 10);
        ++idx;
    }

    std::cout << "engine: cycle jump flushes notes\n";
    Rendered r2;
    eng.prepare (sr);
    clock = 0;
    renderHost (eng, r2, sr, bpm, 0.0, 2.1, true, clock, rnd);  // beats 0,1,2 -> 3 notes
    renderHost (eng, r2, sr, bpm, 0.0, 2.1, true, clock, rnd);  // jump back to 0
    CHECK (r2.ons() == 6);
    // every note-on must be balanced by a note-off once we stop
    renderHost (eng, r2, sr, bpm, 2.1, 0.5, false, clock, rnd);
    CHECK (r2.ons() == r2.offs());

    std::cout << "engine: remap, mute and velocity\n";
    eng.prepare (sr);
    eng.remap[36].store (35);
    eng.remap[37].store (-1);
    eng.velocityScale.store (0.5f);
    eng.channel.store (3);
    Rendered r3;
    clock = 0;
    renderHost (eng, r3, sr, bpm, 0.0, 4.0, true, clock, rnd);
    CHECK (r3.ons() == 2);
    for (auto& e : r3.events)
    {
        CHECK (e.note == 35);
        CHECK (e.ch == 3);
        if (e.on) CHECK (e.vel == 50);
    }
}

static void testEnginePreview()
{
    std::cout << "engine: preview on own clock\n";
    juce::Random rnd (7);
    const double sr = 44100.0;

    PlaybackEngine eng;
    eng.prepare (sr);

    Timeline::Ptr tl = new Timeline();
    tl->length = 4.0;
    tl->tempo = 60.0;
    tl->id = 1;
    for (int i = 0; i < 4; ++i)
        tl->notes.push_back ({ (double) i, i + 0.25, (juce::uint8) (40 + i), 90 });

    eng.startPreview (tl);
    Rendered r;
    juce::int64 clock = 0;
    renderHost (eng, r, sr, 120.0, 0.0, 8.0, false, clock, rnd);
    CHECK (r.ons() == 8);
    CHECK (eng.playingPreviewId.load() == 1);

    std::cout << "engine: preview at original tempo\n";
    eng.prepare (sr);
    eng.previewAtOriginalTempo.store (true);
    Rendered r2;
    clock = 0;
    renderHost (eng, r2, sr, 120.0, 0.0, 4.0, false, clock, rnd); // 4 host-beats @120 = 2s = 2 beats @60
    CHECK (r2.ons() == 2);
    if (r2.ons() >= 2)
        CHECK (r2.events[2].sample == (juce::int64) sr || r2.events[1].sample == (juce::int64) sr);

    std::cout << "engine: preview locks to host bar\n";
    eng.prepare (sr);
    eng.previewAtOriginalTempo.store (false);
    Rendered r3;
    clock = 0;
    renderHost (eng, r3, sr, 120.0, 5.5, 1.0, true, clock, rnd);  // host bar starts at 4
    // ppq 5.5..6.5 -> groove beat 2 (note 42) at ppq 6
    CHECK (r3.ons() == 1);
    if (r3.ons() == 1)
        CHECK (r3.events[0].note == 42);

    std::cout << "engine: stopping preview releases notes, song muted while previewing\n";
    Song song;
    song.addClip (makeGroove (4, 60), 0.0);
    eng.setSongTimeline (song.buildTimeline());
    Rendered r4;
    renderHost (eng, r4, sr, 120.0, 6.5, 1.0, true, clock, rnd);
    for (auto& e : r4.events)
        CHECK (e.note != 60 && e.note != 61);
    eng.stopPreview();
    renderHost (eng, r4, sr, 120.0, 7.5, 0.6, true, clock, rnd);
    int on = 0, off = 0;
    for (auto& e : r4.events)
        if (e.note >= 40 && e.note <= 43) { on += e.on; off += ! e.on; }
    CHECK (on == off);

    std::cout << "engine: internal transport stops at song end\n";
    eng.prepare (sr);
    eng.setInternalPlaying (true);
    Rendered r5;
    clock = 0;
    renderHost (eng, r5, sr, 120.0, 0.0, 6.0, false, clock, rnd);
    int songOns = 0;
    for (auto& e : r5.events) songOns += e.on && (e.note == 60 || e.note == 61);
    CHECK (songOns == 4);
    CHECK (! eng.internalPlaying.load());
    CHECK (r5.ons() == r5.offs());
}

static void testSongEditing()
{
    std::cout << "song: overwrite placement, split, undo, serialisation\n";
    Song song;
    auto a = song.addClip (makeGroove (8), 0.0);
    song.addClip (makeGroove (2, 50), 4.0);

    auto& c = song.getClips();
    CHECK (c.size() == 3);
    if (c.size() == 3)
    {
        CHECK (c[0].id == a && std::abs (c[0].length - 4.0) < 1e-9);
        CHECK (std::abs (c[1].start - 4.0) < 1e-9 && std::abs (c[1].length - 2.0) < 1e-9);
        CHECK (std::abs (c[2].start - 6.0) < 1e-9 && std::abs (c[2].offset - 6.0) < 1e-9);
    }

    auto tl = song.buildTimeline();
    CHECK (tl->notes.size() == 8);
    CHECK_NEAR (tl->length, 8.0, 1e-9);

    auto v = song.toValueTree();
    Song restored;
    restored.fromValueTree (v);
    CHECK (restored.getClips().size() == 3);
    CHECK (restored.buildTimeline()->notes.size() == 8);

    CHECK (song.undo());
    CHECK (song.getClips().size() == 1);
    CHECK (song.redo());
    CHECK (song.getClips().size() == 3);

    auto ids = std::vector<int> { song.getClips()[1].id };
    auto dup = song.duplicateClips (ids);
    CHECK (dup.size() == 1);
    CHECK (song.getClips().size() == 3); // copy at 6..8 replaces the tail piece

    song.moveClips ({ song.getClips()[0].id }, -10.0, false);
    CHECK_NEAR (song.getClips()[0].start, 0.0, 1e-9);

    song.resizeClip (song.getClips()[0].id, 16.0); // grows over everything
    CHECK (song.getClips().size() == 1);
    CHECK_NEAR (song.getEndBeat(), 16.0, 1e-9);
}

static void testExport()
{
    std::cout << "export: remap applied to dragged MIDI\n";
    auto notes = Song::flatten ({ SongClip { 1, 0.0, 4.0, 0.0, makeGroove (4) } }, 0.0);

    ExportOptions o;
    o.map.map[36] = 35;
    o.map.map[37] = -1;
    o.channel = 10;
    o.tempo = 100.0;

    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("gp-test.mid");
    CHECK (writeMidiFile (f, notes, 4.0, o));

    juce::FileInputStream in (f);
    juce::MidiFile mf;
    CHECK (mf.readFrom (in));
    int count = 0;
    double eot = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* e : *mf.getTrack (t))
        {
            if (e->message.isNoteOn()) { ++count; CHECK (e->message.getNoteNumber() == 35); }
            if (e->message.isEndOfTrackMetaEvent()) eot = e->message.getTimeStamp();
        }
    CHECK (count == 2);
    CHECK_NEAR (eot, 4.0 * 960, 0.5);

    auto gm = DrumMap::toontrackToGM();
    CHECK (gm.apply (22) == 42);
    CHECK (gm.apply (24) == 46);
    CHECK (gm.apply (36) == 36);
    CHECK (gm.apply (53) == 53);
    CHECK (gm.apply (21) == 44);

    DrumMap back;
    auto mapFile = f.getSiblingFile ("gp-test-map.xml");
    CHECK (gm.saveToFile (mapFile) && DrumMap::loadFromFile (mapFile, back));
    CHECK (back.map == gm.map);
    f.deleteFile();
    mapFile.deleteFile();
}

//==============================================================================
static void testRealLibrary (const juce::File& root)
{
    std::cout << "real library: midiDB + groove files\n";
    auto pack = root.getChildFile ("000352@THE_BEATS_OF_NEW_ORLEANS");
    if (! pack.isDirectory())
    {
        std::cout << "  (skipped, library not found)\n";
        return;
    }

    MidiDatabase db;
    juce::String err;
    CHECK (db.load (pack.getChildFile ("midiDB"), err));
    CHECK (db.entries.size() == 388);
    CHECK (db.libraryName == "The Beats of New Orleans");

    auto* e = db.find ("406@SWING_4#4/090-S012@VERSE/Variation_01.mid");
    CHECK (e != nullptr);
    if (e != nullptr)
    {
        CHECK_NEAR (e->tempo, 90.0, 0.0);
        CHECK_NEAR (e->lengthBeats, 32.0, 0.0);
        CHECK (e->num == 4 && e->den == 4);
        CHECK (e->tags.contains ("Swing"));
    }

    auto g = Groove::loadFromFile (pack.getChildFile ("406@SWING_4#4/090-S012@VERSE/Variation_01.mid"), {}, 0.0, err);
    CHECK (g != nullptr);
    if (g != nullptr)
    {
        CHECK_NEAR (g->lengthBeats, 32.0, 1e-9);
        CHECK_NEAR (g->tempo, 90.0, 0.01);
        CHECK (g->notes.size() == 183);
        CHECK (g->hasTimeSig && g->num == 4 && g->den == 4);
    }

    ScanProgress progress;
    auto idx = scanLibrary ({ pack.getFullPathName() }, progress, {});
    CHECK (idx != nullptr);
    if (idx != nullptr)
    {
        CHECK (idx->getNumPlayableGrooves() == 388);
        int fromDb = 0;
        for (auto& gi : idx->grooves) fromDb += gi.fromDb;
        CHECK (fromDb == 388);

        SearchFilter f;
        f.text = "smokey verse";
        auto res = idx->search (f, {});
        CHECK (res.size() == 5);

        f = {};
        f.type = SearchFilter::Type::fills;
        auto fills = idx->search (f, {});
        CHECK (! fills.empty());
        for (auto i : fills)
            CHECK (idx->getTypeText (idx->grooves[(size_t) i]).contains ("Fill"));

        int headings = 0;
        for (auto& fo : idx->folders) headings += fo.isHeading;
        CHECK (headings > 0);
    }
}

static void scanWholeLibrary (const juce::File& root)
{
    std::cout << "full scan of " << root.getFullPathName() << "\n";
    ScanProgress progress;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto idx = scanLibrary ({ root.getFullPathName() }, progress, {});
    auto t1 = juce::Time::getMillisecondCounterHiRes();
    if (idx == nullptr) { CHECK (false); return; }

    int fromDb = 0, withTempo = 0, withSig = 0, headingFolders = 0;
    for (auto& g : idx->grooves)
    {
        if (g.isHeading) continue;
        fromDb += g.fromDb;
        withTempo += g.tempo > 0;
        withSig += g.num > 0;
    }
    for (auto& f : idx->folders) headingFolders += f.isHeading;

    std::cout << "  " << idx->getNumPlayableGrooves() << " grooves, " << idx->folders.size() << " folders ("
              << headingFolders << " headings) in " << (t1 - t0) / 1000.0 << " s\n"
              << "  from midiDB: " << fromDb << ", with tempo: " << withTempo << ", with meter: " << withSig << "\n"
              << "  tags: " << idx->tagNames.joinIntoString (", ") << "\n"
              << "  meters: " << idx->timeSignatures.joinIntoString (", ") << "\n";

    auto cache = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("gp-index-test.bin");
    t0 = juce::Time::getMillisecondCounterHiRes();
    CHECK (idx->save (cache));
    t1 = juce::Time::getMillisecondCounterHiRes();
    auto loaded = LibraryIndex::load (cache);
    auto t2 = juce::Time::getMillisecondCounterHiRes();
    CHECK (loaded != nullptr && loaded->grooves.size() == idx->grooves.size());
    std::cout << "  cache " << cache.getSize() / (1024 * 1024) << " MB, save " << (t1 - t0) << " ms, load " << (t2 - t1) << " ms\n";
    cache.deleteFile();

    SearchFilter f;
    f.text = "verse";
    f.type = SearchFilter::Type::beats;
    t0 = juce::Time::getMillisecondCounterHiRes();
    auto res = idx->search (f, {});
    t1 = juce::Time::getMillisecondCounterHiRes();
    std::cout << "  search 'verse' beats: " << res.size() << " hits in " << (t1 - t0) << " ms\n";

    // Parse a sample of files from every vendor to make sure loading works.
    int loadedOk = 0, failed = 0;
    for (size_t i = 0; i < idx->grooves.size(); i += 997)
    {
        if (idx->grooves[i].isHeading) continue;
        juce::String err;
        if (Groove::loadFromFile (idx->getFile ((int) i), {}, idx->grooves[i].lengthBeats, err) != nullptr) ++loadedOk;
        else { ++failed; std::cout << "  load failed: " << err << "\n"; }
    }
    std::cout << "  sample load: " << loadedOk << " ok, " << failed << " failed\n";
    CHECK (failed <= loadedOk / 50);
}

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    testNaming();
    testLengths();
    testEngineSong();
    testEnginePreview();
    testSongEditing();
    testExport();

    juce::File root (defaultLibrary);
    testRealLibrary (root);

    if (argc > 1 && juce::String (argv[1]) == "--scan")
        scanWholeLibrary (argc > 2 ? juce::File (juce::String (argv[2])) : root);

    std::cout << "\n" << checks - failures << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
