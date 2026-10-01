#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace tt
{
    struct TimelineNote
    {
        double start = 0.0, end = 0.0; // beats
        juce::uint8 note = 0, velocity = 0;
    };

    // Immutable, flattened note list handed to the audio thread.
    class Timeline : public juce::ReferenceCountedObject
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<Timeline>;
        std::vector<TimelineNote> notes; // sorted by start
        double length = 0.0;             // beats
        double tempo = 0.0;              // original tempo (previews), 0 = unknown
        int id = 0;                      // identifies a preview request
    };

    // Hands ref-counted objects from the message thread to the audio thread without
    // the audio thread ever deleting anything: every published object is also kept in
    // a pool that is purged on the message thread once nothing else references it.
    template <typename Obj>
    class RealtimeShared
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<Obj>;

        void set (Ptr p) // message thread
        {
            if (p != nullptr)
                pool.push_back (p);

            const juce::SpinLock::ScopedLockType sl (lock);
            current = std::move (p);
        }

        Ptr get() const // any thread
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            return current;
        }

        void collectGarbage() // message thread
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            pool.erase (std::remove_if (pool.begin(), pool.end(), [this] (const Ptr& p)
                                        { return p != current && p->getReferenceCount() == 1; }),
                        pool.end());
        }

    private:
        mutable juce::SpinLock lock;
        Ptr current;
        std::vector<Ptr> pool;
    };

    class PlaybackEngine
    {
    public:
        struct HostInfo
        {
            bool isPlaying = false;
            double ppq = 0.0;
            double bpm = 0.0;           // 0 = unknown
            double barStartPpq = -1.0;  // < 0 = unknown
            int sigNum = 4, sigDen = 4;
        };

        PlaybackEngine();

        // ---- message thread -------------------------------------------------
        void setSongTimeline (Timeline::Ptr t) { song.set (std::move (t)); }
        void startPreview (Timeline::Ptr t);
        void stopPreview();
        void collectGarbage() { song.collectGarbage(); preview.collectGarbage(); }

        void setInternalPlaying (bool shouldPlay);
        void seek (double beat) { seekRequest.store (beat); }

        // ---- shared settings (any thread) -----------------------------------
        std::array<std::atomic<int>, 128> remap;
        std::atomic<int> channel { 10 };            // 1..16
        std::atomic<float> velocityScale { 1.0f };
        std::atomic<double> fallbackTempo { 120.0 };
        std::atomic<bool> previewAtOriginalTempo { false };
        std::atomic<bool> loopSong { false };
        std::atomic<bool> internalPlaying { false };

        // ---- state reported by the audio thread ----------------------------
        std::atomic<double> songPosition { 0.0 };
        std::atomic<double> previewPosition { -1.0 };   // beats inside the preview, -1 = idle
        std::atomic<int> playingPreviewId { 0 };
        std::atomic<bool> hostPlaying { false };
        std::atomic<double> currentTempo { 120.0 };
        std::atomic<int> hostSigNum { 4 }, hostSigDen { 4 };

        // ---- audio thread ---------------------------------------------------
        void prepare (double sampleRate);
        void process (juce::MidiBuffer& out, int numSamples, const HostInfo& host);
        void allNotesOff (juce::MidiBuffer& out, int samplePos);

    private:
        struct Voice { double off; juce::uint8 note, ch; };

        struct Source
        {
            std::array<Voice, 512> voices {};
            int numVoices = 0;
            double expected = -1.0e9;
            bool running = false;
            int mode = -1;        // preview: 0 = own clock, 1 = host locked
            double clock = 0.0;   // preview own clock
            double anchor = 0.0;  // beat where the preview loop starts
            const Timeline* last = nullptr;
        };

        struct Segment
        {
            double b0, b1;        // beat range on the source's clock
            int s0;               // sample position of b0
            double samplesPerBeat;
            int numSamples;
        };

        RealtimeShared<Timeline> song, preview;
        std::atomic<double> seekRequest { -1.0 };
        Source songSrc, prevSrc;
        double sampleRate = 44100.0;
        double internalPos = 0.0;
        std::array<int, 128> remapCache {};
        int outChannel = 10;
        float velScale = 1.0f;

        void flush (Source&, juce::MidiBuffer&, int samplePos);
        void emitOffs (Source&, juce::MidiBuffer&, const Segment&, bool all);
        void render (Source&, const Timeline&, juce::MidiBuffer&, const Segment&, double offset, bool loop);
        void noteOn (Source&, juce::MidiBuffer&, int sample, int note, int vel, double offBeat);
        static int toSample (const Segment& s, double beat);
    };
}
