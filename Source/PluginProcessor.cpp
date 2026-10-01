#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties GroovePlayerProcessor::makeBuses()
{
   #if JucePlugin_IsMidiEffect
    return {};
   #else
    // Silent output: lets hosts like Cubase treat the plugin as an instrument whose
    // MIDI output can be routed to a drum track.
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

GroovePlayerProcessor::GroovePlayerProcessor()
    : AudioProcessor (makeBuses()),
      params (*this, nullptr, "Params", createLayout())
{
    velocityParam = params.getRawParameterValue ("velocity");
    channelParam = params.getRawParameterValue ("channel");
    thruParam = params.getRawParameterValue ("thru");

    song.addChangeListener (this);
    engine.setSongTimeline (song.buildTimeline());
    setDrumMap (tt::DrumMap());

    tt::cleanOldDragFiles();
    startTimer (500);
}

GroovePlayerProcessor::~GroovePlayerProcessor()
{
    *alive = false;
    song.removeChangeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout GroovePlayerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "velocity", 1 }, "Velocity",
                                                  NormalisableRange<float> (0.25f, 2.0f, 0.01f), 1.0f,
                                                  AudioParameterFloatAttributes()
                                                      .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                                                      .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; })));

    l.add (std::make_unique<AudioParameterInt> (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 10));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "thru", 1 }, "MIDI Thru", true));
    return l;
}

bool GroovePlayerProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

bool GroovePlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::disabled();
   #endif
}

void GroovePlayerProcessor::prepareToPlay (double sampleRate, int)
{
    generated.ensureSize (8192);
    engine.prepare (sampleRate);
}

void GroovePlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    tt::PlaybackEngine::HostInfo host;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                host.bpm = *bpm;

            if (auto sig = pos->getTimeSignature())
            {
                host.sigNum = juce::jmax (1, sig->numerator);
                host.sigDen = juce::jmax (1, sig->denominator);
            }

            if (auto ppq = pos->getPpqPosition())
            {
                host.ppq = *ppq;
                host.isPlaying = pos->getIsPlaying();
            }

            if (auto bar = pos->getPpqPositionOfLastBarStart())
                host.barStartPpq = *bar;
        }
    }

    engine.velocityScale.store (velocityParam->load());
    engine.channel.store ((int) channelParam->load());

    if (thruParam->load() < 0.5f)
        midi.clear();

    generated.clear();
    engine.process (generated, buffer.getNumSamples(), host);

    if (! generated.isEmpty())
        midi.addEvents (generated, 0, buffer.getNumSamples(), 0);
}

//==============================================================================
void GroovePlayerProcessor::setDrumMap (const tt::DrumMap& m)
{
    drumMap = m;
    for (int i = 0; i < 128; ++i)
        engine.remap[(size_t) i].store (drumMap.map[(size_t) i]);

    drumMapListeners.call ([] (juce::ChangeListener& l) { l.changeListenerCallback (nullptr); });
}

int GroovePlayerProcessor::startPreview (tt::Groove::Ptr g)
{
    if (g == nullptr)
        return 0;

    currentPreview = g;
    tt::Timeline::Ptr tl = new tt::Timeline();
    for (auto& n : g->notes)
        tl->notes.push_back ({ n.start, n.start + n.length, n.note, n.velocity });

    tl->length = g->lengthBeats;
    tl->tempo = g->tempo;
    tl->id = ++previewCounter;
    engine.startPreview (tl);
    return tl->id;
}

void GroovePlayerProcessor::stopPreview()
{
    currentPreview = nullptr;
    engine.stopPreview();
}

tt::ExportOptions GroovePlayerProcessor::makeExportOptions (const juce::String& trackName) const
{
    tt::ExportOptions o;
    o.map = drumMap;
    o.channel = (int) channelParam->load();
    o.velocityScale = velocityParam->load();
    o.tempo = engine.currentTempo.load();
    o.sigNum = engine.hostSigNum.load();
    o.sigDen = engine.hostSigDen.load();
    o.trackName = trackName;
    return o;
}

void GroovePlayerProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    engine.setSongTimeline (song.buildTimeline());
}

void GroovePlayerProcessor::timerCallback()
{
    engine.collectGarbage();
}

//==============================================================================
juce::AudioProcessorEditor* GroovePlayerProcessor::createEditor()
{
    return new GroovePlayerEditor (*this);
}

void GroovePlayerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("GroovePlayer");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("loop", engine.loopSong.load(), nullptr);
    root.setProperty ("previewOriginalTempo", engine.previewAtOriginalTempo.load(), nullptr);
    root.setProperty ("fallbackTempo", engine.fallbackTempo.load(), nullptr);

    root.appendChild (params.copyState(), nullptr);
    root.appendChild (song.toValueTree(), nullptr);
    root.appendChild (drumMap.toValueTree(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void GroovePlayerProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("GroovePlayer"))
        return;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyState (root);
    }
    else
    {
        juce::MessageManager::callAsync ([this, root, weak = std::weak_ptr<bool> (alive)]
        {
            if (auto a = weak.lock(); a != nullptr && *a)
                applyState (root);
        });
    }
}

void GroovePlayerProcessor::applyState (const juce::ValueTree& root)
{
    engine.loopSong.store ((bool) root.getProperty ("loop", false));
    engine.previewAtOriginalTempo.store ((bool) root.getProperty ("previewOriginalTempo", false));
    engine.fallbackTempo.store ((double) root.getProperty ("fallbackTempo", 120.0));

    auto p = root.getChildWithName (params.state.getType());
    if (p.isValid())
        params.replaceState (p);

    auto s = root.getChildWithName ("Song");
    if (s.isValid())
        song.fromValueTree (s);

    auto m = root.getChildWithName ("DrumMap");
    if (m.isValid())
        setDrumMap (tt::DrumMap::fromValueTree (m));

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
        uiState.copyPropertiesAndChildrenFrom (ui, nullptr);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GroovePlayerProcessor();
}
