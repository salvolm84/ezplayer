cmake_minimum_required(VERSION 3.22)

# Must be set before project(), otherwise project() caches an empty value and the
# binary targets the build machine's macOS version (it then refuses to load on older ones).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")

project(GroovePlayer VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE repository")
add_subdirectory("${JUCE_DIR}" JUCE)

# Two plugins are built from the same sources:
#  - GroovePlayer: an instrument with a silent stereo output and a MIDI output. Cubase
#    (and most VST3 hosts) only route MIDI out of *instrument* plugins.
#  - GroovePlayer MIDI: a pure MIDI effect (no audio buses) for hosts with MIDI FX
#    chains such as Reaper. It goes in front of the drum instrument and audio passes through.
# They have different plugin IDs, so both can be installed side by side.
option(GP_BUILD_MIDI_FX "Also build the GroovePlayer MIDI effect plugin" ON)
option(GP_BUILD_TESTS "Build the console test runner" ON)
option(GP_BUILD_PLUGIN "Build the plugin" ON)

# On macOS link the SDK's SQLite (/usr/lib/libsqlite3.dylib, present on every Mac).
# find_package can pick up a Homebrew/MacPorts copy (/opt/homebrew, /opt/local) that
# other machines don't have, and the plugin then fails to load.
if(APPLE)
    add_library(GP_SQLite3 INTERFACE)
    target_link_libraries(GP_SQLite3 INTERFACE sqlite3)
    add_library(SQLite3::SQLite3 ALIAS GP_SQLite3)
else()
    find_package(SQLite3 REQUIRED)
endif()

set(GP_CORE_SOURCES
    Source/Core/ToontrackNaming.cpp
    Source/Core/MidiDatabase.cpp
    Source/Core/LibraryIndex.cpp
    Source/Core/Groove.cpp
    Source/Core/DrumMap.cpp
    Source/Core/Song.cpp
    Source/Core/PlaybackEngine.cpp
    Source/Core/MidiExport.cpp
    Source/Core/RhythmIndex.cpp)

if(GP_BUILD_PLUGIN)
function(gp_add_plugin target)
    cmake_parse_arguments(GP "MIDI_EFFECT" "PRODUCT_NAME;BUNDLE_ID;PLUGIN_CODE" "FORMATS;VST3_CATEGORIES" ${ARGN})

    if(GP_MIDI_EFFECT)
        set(is_synth FALSE)
        set(is_midi_fx TRUE)
    else()
        set(is_synth TRUE)
        set(is_midi_fx FALSE)
    endif()

    juce_add_plugin(${target}
        COMPANY_NAME "SLM"
        BUNDLE_ID "${GP_BUNDLE_ID}"
        IS_SYNTH ${is_synth}
        IS_MIDI_EFFECT ${is_midi_fx}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE SLMm
        PLUGIN_CODE ${GP_PLUGIN_CODE}
        VST3_CATEGORIES ${GP_VST3_CATEGORIES}
        FORMATS ${GP_FORMATS}
        PRODUCT_NAME "${GP_PRODUCT_NAME}")

    target_sources(${target}
        PRIVATE
            ${GP_CORE_SOURCES}
            Source/Core/AppContext.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/UI/Theme.cpp
            Source/UI/PatternView.cpp
            Source/UI/BrowserComponent.cpp
            Source/UI/SongLane.cpp
            Source/UI/TransportBar.cpp
            Source/UI/RemapEditor.cpp
            Source/UI/SettingsPanel.cpp
            Source/UI/BeatSearchPanel.cpp)

    target_include_directories(${target} PRIVATE Source)

    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            SQLite3::SQLite3
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

gp_add_plugin(GroovePlayer
    PRODUCT_NAME "GroovePlayer"
    BUNDLE_ID "com.slm.grooveplayer"
    PLUGIN_CODE GrPl
    VST3_CATEGORIES Instrument Drum
    FORMATS VST3 Standalone)

if(GP_BUILD_MIDI_FX)
    # VST3 has no MIDI-effect category; "Fx" puts it with the effects.
    gp_add_plugin(GroovePlayerMIDI
        MIDI_EFFECT
        PRODUCT_NAME "GroovePlayer MIDI"
        BUNDLE_ID "com.slm.grooveplayer.midifx"
        PLUGIN_CODE GrPm
        VST3_CATEGORIES Fx
        FORMATS VST3)
endif()
endif()

if(GP_BUILD_TESTS)
    juce_add_console_app(GroovePlayerTests PRODUCT_NAME "GroovePlayerTests")
    target_sources(GroovePlayerTests PRIVATE ${GP_CORE_SOURCES} Tests/TestMain.cpp)
    target_include_directories(GroovePlayerTests PRIVATE Source)
    target_compile_definitions(GroovePlayerTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(GroovePlayerTests
        PRIVATE
            juce::juce_core
            juce::juce_events
            juce::juce_data_structures
            juce::juce_audio_basics
            SQLite3::SQLite3
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
