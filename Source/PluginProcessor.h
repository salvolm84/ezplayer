#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Core/AppContext.h"
#include "Core/DrumMap.h"
#include "Core/MidiExport.h"
#include "Core/PlaybackEngine.h"
#include "Core/Song.h"

class GroovePlayerProcessor : public juce::AudioProcessor,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    GroovePlayerProcessor();
    ~GroovePlayerProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState params;
    juce::SharedResourcePointer<AppContext> app;
    tt::Song song;
    tt::PlaybackEngine engine;

    // Persisted per-instance UI state (editor size, zoom, snap, selected folder...)
    juce::ValueTree uiState { "UI" };

    //==============================================================================
    const tt::DrumMap& getDrumMap() const { return drumMap; }
    void setDrumMap (const tt::DrumMap&);

    // Preview playback of a groove (looped). Returns the preview id.
    int startPreview (tt::Groove::Ptr);
    void stopPreview();
    bool isPreviewing() const { return currentPreview != nullptr; }
    tt::Groove::Ptr getPreviewGroove() const { return currentPreview; }
    int getPreviewId() const { return currentPreview != nullptr ? previewCounter : 0; }

    void setLoopSong (bool b) { engine.loopSong.store (b); }
    void setPreviewAtOriginalTempo (bool b) { engine.previewAtOriginalTempo.store (b); }

    double getBarLengthBeats() const
    {
        return engine.hostSigNum.load() * 4.0 / juce::jmax (1, engine.hostSigDen.load());
    }

    tt::ExportOptions makeExportOptions (const juce::String& trackName) const;

    juce::ListenerList<juce::ChangeListener>& getDrumMapListeners() { return drumMapListeners; }

private:
    tt::DrumMap drumMap;
    tt::Groove::Ptr currentPreview;
    int previewCounter = 0;
    juce::MidiBuffer generated;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
    juce::ListenerList<juce::ChangeListener> drumMapListeners;

    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* channelParam = nullptr;
    std::atomic<float>* thruParam = nullptr;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void applyState (const juce::ValueTree&);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovePlayerProcessor)
};
