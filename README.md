# GroovePlayer

A JUCE VST3 MIDI groove player in the spirit of Toontrack EZplayer pro. It has a
browser for Toontrack-format MIDI drum libraries, previews that follow the DAW, a
simple one-lane song track synced to the host, note remapping, and drag & drop of
remapped MIDI into the DAW.

There is no MIDI editing. It is a browser and an arranger only.

## Features

**Browser**
- Reads Toontrack MIDI libraries as they are on disk:
  - `sortkey@NAME` folders and files. `_` is shown as a space, `#` as `/`, and entries are sorted by sort key.
  - Folders that contain only an empty `header` file show as section headings in the tree.
  - Empty `NN@_TEXT.head` files show as headings inside a groove list (e.g. `92 BPM`).
  - Each pack's `midiDB` (SQLite) supplies display names, tempo, meter, length, genre and tags (Beat/Fill, Straight/Swing, Half/Double time, play styles…).
- Packs without a `midiDB` (Groove Monkee, Platinum Samples, plain `.mid` folders) still work. Tempo is inferred from names (`090-S012`, `_92_BPM`, `150 Fill 3`, `120BPM`), meter from folders (`STRAIGHT_4#4`, `6-4 FILLS`), and Beat/Fill/feel from names.
- Text search across groove names, folders and section headings. Filters: Beats/Fills, feel, meter, tempo range ("near DAW tempo" included) and play style. Search can be limited to the selected folder.
- Favourites, sortable columns, and a drum-grid pattern view of the selected groove.
- The index is built on a background thread and cached. A ~166k groove library scans in about 13 s, and later sessions load it from cache in under half a second.

**Beat search**
- Click **Beat search** to open a small step grid: Kick / Snare / Cymbal × 16ths, 1 or 2 bars, in 4/4, 3/4, 6/8, 12/8, 5/4, 7/8 or 2/4.
- "Cymbal" matches any hi-hat, ride, crash, china or splash. Rows you leave empty are ignored ("don't care"). Click or drag to paint cells, and press *Play* to hear your pattern.
- Results update as you edit. They are ranked by a **Match %** column and respect the other filters (type, feel, tempo, text, folder scope).
- Scoring: each pattern bar is compared with every bar window of each groove, wrapping like a loop. Hits one 16th off get half credit, so swung or pushed hits still match. Ghost notes and the hi-hat pedal are ignored.
- The fingerprints are computed in the background after a library scan (first run reads every file once) and cached in `rhythm-index.bin`.

**Preview**
- Click a groove to audition it (auto-preview can be switched off). It loops and is sent out of the plugin's MIDI output.
- While the DAW plays, the preview locks to the DAW's bar grid and tempo. While the DAW is stopped it runs on its own clock, at the DAW tempo or at the groove's original tempo.

**Song track (one lane)**
- Drag grooves from the browser (or `.mid` files from Finder) onto the lane, or double-click a groove to append it.
- Move parts by dragging; Alt-drag copies. Drag a part's right edge to loop or shorten the groove. Dropped parts overwrite whatever they overlap (it is trimmed or split).
- Snap to bar, beat, 1/8 or off. Zoom with the buttons or Cmd+wheel. Undo/redo, duplicate (Cmd+D), delete (Backspace).
- Playback follows the DAW transport, tempo, position and cycle, with sample-accurate timing. Song bar 1 is DAW bar 1.
- Optional **start from the beginning** mode (Settings, saved with the project): when the DAW starts playing, the song starts from bar 1 wherever the playhead is, at the first DAW bar line reached, so it stays on the DAW grid. Locating while playing restarts the song too. With cycle on, each pass repeats what played over the cycle. If playback started inside the cycle, each pass starts the song from the beginning.
- Internal Play/Loop for when the DAW is stopped.
- The song is saved in the DAW project with the MIDI embedded, so projects still open if the library moves.

**Remap**
- A 128-note In→Out table. Each note can be sent to any output note or muted.
- Built-in presets: *Toontrack (no remap)*, *Toontrack → General MIDI* (best effort), and libraries:
  - **GetGood Drums:** Matt Halpern Signature, Modern & Massive, Modern & Massive 2, P IV Matt Halpern, Invasion, and One Kit Wonder Aggressive Rock / Architects / Metal. Each preset sends every Toontrack articulation (toms, hi-hat openings, ride, crashes) to the library's default mapping. Toms are spread over however many the kit has, and articulations the kit lacks are muted.
  - **IOF Drummer 6:** IOF's default mapping (kick 36, snare 38, toms 45–52, hats 57–70, cymbals 74–96). IOF has no hi-hat pedal notes, so Toontrack pedal hits are muted.
- You can save, import and share presets as XML files in `~/Library/Application Support/SLM/GroovePlayer/Remaps`.
- The remap applies to playback, previews and **every MIDI file dragged or exported**.

**MIDI out of the plugin**
- Drag a groove row, the pattern view, a song part, or the "Drag song to DAW" handle outside the plugin window. It turns into a `.mid` file drop with the remap, channel and velocity applied.
- *Export MIDI…* saves the song (or the selected parts) to a file.
- Output MIDI channel, velocity scaling and MIDI thru are host-automatable parameters.

## Using it in Cubase

Cubase only routes MIDI *out of* instrument plugins, so GroovePlayer builds as an
**instrument with a silent stereo output and a MIDI output** (that is how EZplayer works too).

1. Add GroovePlayer on an Instrument track.
2. To hear it live, set the drum instrument's MIDI track input to the GroovePlayer MIDI output and enable monitoring.
3. Or drag parts / the whole song straight from GroovePlayer onto the drum track.

## Using it in Reaper

Use **GroovePlayer MIDI**, the MIDI-effect build. It has no audio buses, only MIDI in and out.

1. Insert GroovePlayer MIDI in the drum track's FX chain, above the drum instrument.
2. Its MIDI goes straight to the next plugin in the chain, and audio passes through untouched.
   Turn *MIDI Thru* off to stop the track's own MIDI from reaching the instrument.
3. Or drag parts / the whole song from GroovePlayer onto a track.

Both plugins share settings, favourites, remap presets and the library cache. They have
different plugin IDs, so a project saved with one doesn't load into the other.

## Building

You need macOS, Xcode command line tools, CMake ≥ 3.22 and [JUCE 8](https://github.com/juce-framework/JUCE). SQLite comes from the macOS SDK.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=$HOME/JUCE
cmake --build build --target GroovePlayer_VST3 GroovePlayerMIDI_VST3 -j
```

The VST3s are copied to `~/Library/Audio/Plug-Ins/VST3/` as `GroovePlayer.vst3` (instrument)
and `GroovePlayer MIDI.vst3` (MIDI effect). There is also a `GroovePlayer_Standalone` target.
Configure with `-DGP_BUILD_MIDI_FX=OFF` to skip the MIDI effect.

For a build to share with other Macs, use a fresh build folder and make it universal:

```sh
cmake -S . -B build-dist -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-dist --target GroovePlayer_VST3 GroovePlayerMIDI_VST3 GroovePlayer_Standalone -j
```

The build targets macOS 11 and links only system libraries. Check with
`otool -L` that no `/opt/homebrew` or `/opt/local` path shows up in the binary.

On first launch the library folder defaults to
`~/Library/Mobile Documents/com~apple~CloudDocs/Cubase/drums midi/Toontrack` if that folder exists.
Add or remove folders under **Settings**.

### Tests

```sh
cmake -S . -B build-tests -DGP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests --target GroovePlayerTests -j
./build-tests/GroovePlayerTests_artefacts/Debug/GroovePlayerTests          # unit tests
./build-tests/GroovePlayerTests_artefacts/Debug/GroovePlayerTests --scan   # + full library scan benchmark
```

The tests cover:
- name decoding and tempo/meter inference
- groove length resolution
- engine timing (sample positions, cycle jumps, preview bar-locking, internal transport, note-off balance)
- song editing and serialisation
- remapped export
- real-library checks (`midiDB` parsing, file loading, search), when the library is present

The plugin also passes [pluginval](https://github.com/Tracktion/pluginval) at strictness level 8.

## Code layout

| Path | Contents |
| --- | --- |
| `Source/Core/ToontrackNaming` | Toontrack name convention decoding and inference |
| `Source/Core/MidiDatabase` | Read-only `midiDB` (SQLite) reader |
| `Source/Core/LibraryIndex` | Library scan, index cache, search |
| `Source/Core/Groove` | MIDI file → beat-based notes, loop length, LRU cache |
| `Source/Core/Song` | One-lane song model: placement, undo, serialisation, flattening |
| `Source/Core/PlaybackEngine` | Real-time scheduler (host sync, preview, internal transport) |
| `Source/Core/DrumMap`, `MidiExport` | Remap tables / presets and `.mid` writing |
| `Source/Core/AppContext` | Process-wide shared state: settings, index, favourites, loader |
| `Source/UI/*` | Browser, pattern view, song lane, transport, remap and settings panels |

Not affiliated with Toontrack. EZplayer, EZdrummer and Superior Drummer are trademarks of Toontrack Music AB.
