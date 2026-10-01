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

**Preview**
- Click a groove to audition it (auto-preview can be switched off). It loops and is sent out of the plugin's MIDI output.
- While the DAW plays, the preview locks to the DAW's bar grid and tempo. While the DAW is stopped it runs on its own clock, at the DAW tempo or at the groove's original tempo.

**Song track (one lane)**
- Drag grooves from the browser (or `.mid` files from Finder) onto the lane, or double-click a groove to append it.
- Move parts by dragging; Alt-drag copies. Drag a part's right edge to loop or shorten the groove. Dropped parts overwrite whatever they overlap (it is trimmed or split).
- Snap to bar, beat, 1/8 or off. Zoom with the buttons or Cmd+wheel. Undo/redo, duplicate (Cmd+D), delete (Backspace).
- Playback follows the DAW transport, tempo, position and cycle, with sample-accurate timing. Song bar 1 is DAW bar 1.
- Internal Play/Loop for when the DAW is stopped.
- The song is saved in the DAW project with the MIDI embedded, so projects still open if the library moves.

**Remap**
- A 128-note In→Out table. Each note can be sent to any output note or muted.
- Built-in presets: *Toontrack (no remap)* and *Toontrack → General MIDI*. The GM preset is a best-effort mapping built from Toontrack's `instrumentgroups` data; check it against your kit.
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

To build a pure MIDI effect instead (no audio bus), configure with `-DGP_BUILD_AS_MIDI_EFFECT=ON`.

## Building

You need macOS, Xcode command line tools, CMake ≥ 3.22 and [JUCE 8](https://github.com/juce-framework/JUCE). SQLite comes from the macOS SDK.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=$HOME/JUCE
cmake --build build --target GroovePlayer_VST3 -j
```

The VST3 is copied to `~/Library/Audio/Plug-Ins/VST3/GroovePlayer.vst3`. There is also a
`GroovePlayer_Standalone` target.

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
