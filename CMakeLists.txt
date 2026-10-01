cmake_minimum_required(VERSION 3.22)

project(GroovePlayer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")

set(JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE repository")
add_subdirectory("${JUCE_DIR}" JUCE)

# Cubase (and most VST3 hosts) only offer MIDI output routing from *instrument*
# plugins, so by default GroovePlayer is an instrument with a silent stereo output
# and a MIDI output. Turn this ON to build a pure MIDI effect (no audio buses).
option(GP_BUILD_AS_MIDI_EFFECT "Build as a pure MIDI effect instead of instrument+MIDI out" OFF)
option(GP_BUILD_TESTS "Build the console test runner" ON)
option(GP_BUILD_PLUGIN "Build the plugin" ON)

find_package(SQLite3 REQUIRED)

if(GP_BUILD_AS_MIDI_EFFECT)
    set(GP_IS_SYNTH FALSE)
    set(GP_IS_MIDI_FX TRUE)
    set(GP_VST3_CATS Fx)
else()
    set(GP_IS_SYNTH TRUE)
    set(GP_IS_MIDI_FX FALSE)
    set(GP_VST3_CATS Instrument Drum)
endif()

set(GP_CORE_SOURCES
    Source/Core/ToontrackNaming.cpp
    Source/Core/MidiDatabase.cpp
    Source/Core/LibraryIndex.cpp
    Source/Core/Groove.cpp
    Source/Core/DrumMap.cpp
    Source/Core/Song.cpp
    Source/Core/PlaybackEngine.cpp
    Source/Core/MidiExport.cpp)

if(GP_BUILD_PLUGIN)
juce_add_plugin(GroovePlayer
    COMPANY_NAME "SLM"
    BUNDLE_ID "com.slm.grooveplayer"
    IS_SYNTH ${GP_IS_SYNTH}
    IS_MIDI_EFFECT ${GP_IS_MIDI_FX}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE SLMm
    PLUGIN_CODE GrPl
    VST3_CATEGORIES ${GP_VST3_CATS}
    FORMATS VST3 Standalone
    PRODUCT_NAME "GroovePlayer")

target_sources(GroovePlayer
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
        Source/UI/SettingsPanel.cpp)

target_include_directories(GroovePlayer PRIVATE Source)

target_compile_definitions(GroovePlayer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GroovePlayer
    PRIVATE
        juce::juce_audio_utils
        SQLite::SQLite3
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

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
            SQLite::SQLite3
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
