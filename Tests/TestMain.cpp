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
#include "Core/RhythmIndex.h"

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
                        double totalBeats, bool playing, juce::int64& sampleClock, juce::Random& rnd,
                        std::function<void (PlaybackEngine::HostInfo&)> tweakHost = {})
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
        if (tweakHost)
            tweakHost (h);

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

static void testEngineSongFromStart()
{
    std::cout << "engine: song starts from the beginning on host play\n";
    juce::Random rnd (11);
    const double sr = 48000.0, bpm = 120.0, spb = sr * 60.0 / bpm;

    // 16 beats, one distinct note per beat: note 40 + beat
    Groove::Ptr g = new Groove();
    g->name = "Steps";
    g->lengthBeats = 16;
    for (int i = 0; i < 16; ++i)
        g->notes.push_back ({ (double) i, 0.25, (juce::uint8) (40 + i), 100 });

    Song song;
    song.addClip (g, 0.0);

    auto firstOn = [] (const Rendered& r) -> const Rendered::Ev*
    {
        for (auto& e : r.events)
            if (e.on)
                return &e;
        return nullptr;
    };

    PlaybackEngine eng;
    eng.prepare (sr);
    eng.setSongTimeline (song.buildTimeline());
    eng.songFromStartOnHostPlay.store (true);

    // Start on bar 9: song beat 0 plays immediately.
    {
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 32.0, 4.0, true, clock, rnd);
        CHECK (r.ons() == 4);
        if (auto* e = firstOn (r)) { CHECK (e->note == 40); CHECK (e->sample == 0); }
        CHECK_NEAR (eng.songPosition.load(), 4.0, 0.05);
        renderHost (eng, r, sr, bpm, 36.0, 0.5, false, clock, rnd);
        CHECK (r.ons() == r.offs());
    }

    // Start mid-bar: the song starts at the next bar line (ppq 36).
    {
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 33.5, 4.0, true, clock, rnd);
        CHECK (r.ons() == 2);
        if (auto* e = firstOn (r)) { CHECK (e->note == 40); CHECK (e->sample == (juce::int64) std::llround (2.5 * spb)); }
        renderHost (eng, r, sr, bpm, 37.5, 0.5, false, clock, rnd);
    }

    auto cycle = [] (PlaybackEngine::HostInfo& h) { h.isLooping = true; h.loopStart = 32.0; h.loopEnd = 40.0; };

    // Cycle 32..40, started before it at 28: the wrap repeats what played over the
    // cycle on the first pass (song beats 4..11).
    {
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 28.0, 12.0, true, clock, rnd, cycle);
        CHECK (r.ons() == 12);
        Rendered wrap;
        renderHost (eng, wrap, sr, bpm, 32.0, 8.0, true, clock, rnd, cycle);
        CHECK (wrap.ons() == 8);
        if (auto* e = firstOn (wrap)) CHECK (e->note == 44);
        renderHost (eng, wrap, sr, bpm, 40.0, 0.5, false, clock, rnd);
    }

    // Cycle 32..40, started inside it at 34: the song starts at 36, and every wrap
    // starts it again from the beginning at the cycle start.
    {
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 34.0, 6.0, true, clock, rnd, cycle);
        CHECK (r.ons() == 4);
        if (auto* e = firstOn (r)) CHECK (e->note == 40);
        for (int pass = 0; pass < 2; ++pass)
        {
            Rendered wrap;
            renderHost (eng, wrap, sr, bpm, 32.0, 8.0, true, clock, rnd, cycle);
            CHECK (wrap.ons() == 8);
            if (auto* e = firstOn (wrap)) { CHECK (e->note == 40); }
        }
        renderHost (eng, r, sr, bpm, 40.0, 0.5, false, clock, rnd);
    }

    // Locating while playing (no cycle) restarts the song at the new position.
    {
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 8.0, 6.0, true, clock, rnd);
        Rendered after;
        renderHost (eng, after, sr, bpm, 64.0, 2.0, true, clock, rnd);
        CHECK (after.ons() == 2);
        if (auto* e = firstOn (after)) CHECK (e->note == 40);
        renderHost (eng, after, sr, bpm, 66.0, 0.5, false, clock, rnd);
    }

    // Option off: the song follows the host bar position (song beat = ppq).
    {
        eng.songFromStartOnHostPlay.store (false);
        Rendered r;
        juce::int64 clock = 0;
        renderHost (eng, r, sr, bpm, 4.0, 2.0, true, clock, rnd);
        if (auto* e = firstOn (r)) CHECK (e->note == 44);
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

    std::cout << "engine: preview updated in place keeps its position\n";
    {
        PlaybackEngine e2;
        e2.prepare (sr);
        e2.startPreview (tl);                       // notes 40..43 on beats 0..3
        Rendered a;
        juce::int64 c2 = 0;
        renderHost (e2, a, sr, 120.0, 0.0, 1.5, false, c2, rnd);   // plays beats 0 and 1

        Timeline::Ptr edited = new Timeline (*tl);  // same id: an edit of the pattern
        for (auto& n : edited->notes) n.note = (juce::uint8) (n.note + 10);
        e2.startPreview (edited);
        Rendered b;
        renderHost (e2, b, sr, 120.0, 0.0, 1.0, false, c2, rnd);   // continues: beat 2 only
        CHECK (b.ons() == 1);
        if (b.ons() == 1) CHECK (b.events[0].note == 52);

        Timeline::Ptr other = new Timeline (*edited);
        other->id = 99;                             // a different groove restarts at beat 0
        e2.startPreview (other);
        Rendered c;
        renderHost (e2, c, sr, 120.0, 0.0, 0.4, false, c2, rnd);
        CHECK (c.ons() == 1);
        if (c.ons() >= 1) { CHECK (c.events[0].note == 50); }
    }

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

    std::cout << "export: built-in library presets\n";
    auto presets = DrumMap::builtInPresets();
    auto find = [&] (const juce::String& n) { for (auto& p : presets) if (p.name == n) return p; return DrumMap(); };
    CHECK (presets.size() == 11);

    auto halpern = find ("GGD Matt Halpern Signature");
    CHECK (halpern.apply (36) == 24);   // kick
    CHECK (halpern.apply (38) == 26);   // snare
    CHECK (halpern.apply (40) == 26);   // rimshot folds onto snare
    CHECK (halpern.apply (42) == 47);   // HH closed tip
    CHECK (halpern.apply (22) == 48);   // HH closed edge
    CHECK (halpern.apply (44) == 43);   // pedal chick
    CHECK (halpern.apply (48) == 33 && halpern.apply (41) == 39); // 4 toms
    CHECK (halpern.apply (53) == 74);   // ride bell
    CHECK (halpern.apply (0) == -1);    // unknown notes are muted

    auto okwRock = find ("GGD One Kit Wonder: Aggressive Rock"); // 2 toms
    CHECK (okwRock.apply (48) == 33 && okwRock.apply (45) == 33 && okwRock.apply (43) == 34);

    auto invasion = find ("GGD Invasion"); // 6 toms
    CHECK (invasion.apply (41) == 37);

    auto mm2 = find ("GGD Modern & Massive 2");
    CHECK (mm2.apply (36) == 36 && mm2.apply (38) == 38 && mm2.apply (37) == 37);
    CHECK (mm2.apply (48) == 42 && mm2.apply (41) == 47);
    CHECK (mm2.apply (44) == 56 && mm2.apply (46) == 55 && mm2.apply (49) == 60 && mm2.apply (57) == 64);

    auto iof = find ("IOF Drummer 6");
    CHECK (iof.apply (36) == 36 && iof.apply (38) == 38 && iof.apply (37) == 43);
    CHECK (iof.apply (48) == 45 && iof.apply (47) == 47 && iof.apply (45) == 49 && iof.apply (43) == 51 && iof.apply (41) == 51);
    CHECK (iof.apply (42) == 57 && iof.apply (22) == 58 && iof.apply (46) == 70 && iof.apply (44) == -1);
    CHECK (iof.apply (51) == 81 && iof.apply (53) == 86 && iof.apply (49) == 74 && iof.apply (57) == 79);
    CHECK (iof.apply (52) == 88 && iof.apply (55) == 95);

    for (auto& p : presets)
        for (auto m : p.map)
            CHECK (m >= -1 && m <= 127);

    DrumMap back;
    auto mapFile = f.getSiblingFile ("gp-test-map.xml");
    CHECK (gm.saveToFile (mapFile) && DrumMap::loadFromFile (mapFile, back));
    CHECK (back.map == gm.map);
    f.deleteFile();
    mapFile.deleteFile();
}

//==============================================================================
// Builds a 1-bar 4/4 groove from step strings ("x...x...") per voice.
static Groove::Ptr grooveFromSteps (const char* kick, const char* snare, const char* hat, int hatNote = 42)
{
    Groove::Ptr g = new Groove();
    g->lengthBeats = 4.0;
    for (int i = 0; i < 16; ++i)
    {
        if (kick[i] == 'x')  g->notes.push_back ({ i * 0.25, 0.1, 36, 110 });
        if (snare[i] == 'x') g->notes.push_back ({ i * 0.25, 0.1, 38, 110 });
        if (snare[i] == 'g') g->notes.push_back ({ i * 0.25, 0.1, 38, 15 }); // ghost
        if (hat[i] == 'x')   g->notes.push_back ({ i * 0.25, 0.1, (juce::uint8) hatNote, 90 });
    }
    std::sort (g->notes.begin(), g->notes.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    return g;
}

static BarPrint barFromSteps (const char* kick, const char* snare, const char* cym)
{
    BarPrint b;
    for (int i = 0; i < 16; ++i)
    {
        if (kick[i] == 'x')  b.kick |= 1u << i;
        if (snare[i] == 'x') b.snare |= 1u << i;
        if (cym[i] == 'x')   b.cymbal |= 1u << i;
    }
    return b;
}

static void testBeatSearch()
{
    std::cout << "beat search: fingerprints and scoring\n";
    const char* rockK = "x.......x.x.....";
    const char* rockS = "....x.......x...";
    const char* rockH = "x.x.x.x.x.x.x.x.";

    auto rock = makeRhythmPrint (*grooveFromSteps (rockK, rockS, rockH), 4, 4);
    CHECK (rock.bars.size() == 1 && rock.getStepsPerBar() == 16);
    CHECK (rock.bars[0] == barFromSteps (rockK, rockS, rockH));

    // Ghost notes are ignored, ride (51) and crash (49) count as "cymbal", pedal (44) doesn't.
    auto ghosty = makeRhythmPrint (*grooveFromSteps (rockK, "..g.x...g...x...", rockH, 51), 4, 4);
    CHECK (ghosty.bars[0] == barFromSteps (rockK, rockS, rockH));
    auto pedal = makeRhythmPrint (*grooveFromSteps (rockK, rockS, rockH, 44), 4, 4);
    CHECK (pedal.bars[0].cymbal == 0);

    std::vector<BarPrint> query { barFromSteps (rockK, rockS, rockH) };
    CHECK_NEAR (scoreRhythm (query, 16, rock), 1.0f, 1e-6);

    // Kick + snare only query ignores the cymbal voice
    std::vector<BarPrint> ksOnly { barFromSteps (rockK, rockS, "................") };
    CHECK_NEAR (scoreRhythm (ksOnly, 16, rock), 1.0f, 1e-6);

    // One kick pushed by a 16th scores high but below exact; a different beat scores lower.
    auto pushed = makeRhythmPrint (*grooveFromSteps ("x........xx.....", rockS, rockH), 4, 4);
    auto halftime = makeRhythmPrint (*grooveFromSteps ("x.........x.....", "........x.......", rockH), 4, 4);
    auto sPushed = scoreRhythm (query, 16, pushed), sHalf = scoreRhythm (query, 16, halftime);
    CHECK (sPushed > 0.8f && sPushed < 1.0f);
    CHECK (sHalf < sPushed);

    // Multi-bar grooves: the best matching bar wins.
    Groove::Ptr twoBar = new Groove();
    twoBar->lengthBeats = 8.0;
    auto bar1 = grooveFromSteps ("x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "................");
    auto bar2 = grooveFromSteps (rockK, rockS, rockH);
    for (auto& n : bar1->notes)
        twoBar->notes.push_back (n);
    for (auto n : bar2->notes) { n.start += 4.0; twoBar->notes.push_back (n); }
    auto tb = makeRhythmPrint (*twoBar, 4, 4);
    CHECK (tb.bars.size() == 2);
    CHECK (scoreRhythm (query, 16, tb) > 0.85f);

    // Ranking through the index, and meters never mix.
    RhythmIndex idx;
    idx.prints = { halftime, rock, pushed, makeRhythmPrint (*grooveFromSteps (rockK, rockS, rockH), 3, 4) };
    idx.printForGroove = { 0, 1, 2, 3 };
    auto res = idx.search (query, 4, 4, { 0, 1, 2, 3 }, 0.0f, 10);
    CHECK (res.size() == 3);
    if (res.size() == 3)
        CHECK (res[0].groove == 1 && res[1].groove == 2 && res[2].groove == 0);
    CHECK (idx.search ({ BarPrint() }, 4, 4, { 0, 1, 2 }, 0.0f, 10).empty());

    // Cache round trip
    RhythmCache cache, back;
    cache.byPath["/a.mid"] = rock;
    cache.byPath["/b.mid"] = tb;
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("gp-rhythm-test.bin");
    CHECK (cache.save (f) && back.load (f));
    CHECK (back.byPath.size() == 2 && back.byPath["/b.mid"].bars.size() == 2 && back.byPath["/a.mid"].bars[0] == rock.bars[0]);
    f.deleteFile();
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
    testEngineSongFromStart();
    testEnginePreview();
    testSongEditing();
    testExport();
    testBeatSearch();

    juce::File root (defaultLibrary);
    testRealLibrary (root);

    if (argc > 1 && juce::String (argv[1]) == "--scan")
        scanWholeLibrary (argc > 2 ? juce::File (juce::String (argv[2])) : root);

    std::cout << "\n" << checks - failures << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
