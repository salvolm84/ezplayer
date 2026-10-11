#include "PlaybackEngine.h"

namespace tt
{
    // Block boundaries are evaluated as [b0 - eps, b1 - eps) so that consecutive
    // blocks partition the timeline exactly despite floating point accumulation.
    static constexpr double beatEps = 1.0e-9;

    PlaybackEngine::PlaybackEngine()
    {
        for (int i = 0; i < 128; ++i)
            remap[(size_t) i].store (i);
    }

    void PlaybackEngine::startPreview (Timeline::Ptr t)
    {
        preview.set (std::move (t));
    }

    void PlaybackEngine::stopPreview()
    {
        preview.set (nullptr);
    }

    void PlaybackEngine::setInternalPlaying (bool shouldPlay)
    {
        internalPlaying.store (shouldPlay);
    }

    void PlaybackEngine::prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        songSrc = {};
        prevSrc = {};
    }

    int PlaybackEngine::toSample (const Segment& s, double beat)
    {
        auto pos = s.s0 + (int) std::floor ((beat - s.b0) * s.samplesPerBeat + 1.0e-6);
        return juce::jlimit (0, juce::jmax (0, s.numSamples - 1), pos);
    }

    double PlaybackEngine::firstBarAtOrAfter (const HostInfo& host, double ppq)
    {
        const auto barLen = host.sigNum * 4.0 / juce::jmax (1, host.sigDen);
        const auto ref = host.barStartPpq >= 0.0 && host.barStartPpq <= host.ppq + 1.0e-9 ? host.barStartPpq : 0.0;
        auto bar = ref + std::floor ((ppq - ref) / barLen + 1.0e-9) * barLen;
        return ppq - bar > 1.0e-3 ? bar + barLen : bar;
    }

    void PlaybackEngine::trackHostAnchor (const HostInfo& host, double samplesPerBeat, int numSamples)
    {
        if (! host.isPlaying)
        {
            hostWasPlaying = false;
            return;
        }

        const auto blockBeats = numSamples / samplesPerBeat;
        const bool jumped = std::abs (host.ppq - hostExpected) > juce::jmax (1.0e-6, 2.0 / samplesPerBeat);

        if (! hostWasPlaying)
        {
            songAnchor = firstBarAtOrAfter (host, host.ppq);
        }
        else if (jumped)
        {
            const bool cycleWrap = host.isLooping && host.loopStart >= 0.0 && host.loopEnd > host.loopStart
                                && host.ppq < hostExpected
                                && host.ppq > host.loopStart - 1.0e-6
                                && host.ppq < host.loopStart + juce::jmax (blockBeats, 0.25);

            if (! cycleWrap)
                songAnchor = firstBarAtOrAfter (host, host.ppq);        // locate: restart the song
            else if (songAnchor > host.loopStart + 1.0e-6)
                songAnchor = firstBarAtOrAfter (host, host.loopStart);  // started inside the cycle
            // else: the cycle repeats what played over it on the first pass
        }

        hostWasPlaying = true;
        hostExpected = host.ppq + blockBeats;
    }

    void PlaybackEngine::flush (Source& src, juce::MidiBuffer& out, int samplePos)
    {
        for (int i = 0; i < src.numVoices; ++i)
            out.addEvent (juce::MidiMessage::noteOff (src.voices[(size_t) i].ch, src.voices[(size_t) i].note), samplePos);

        src.numVoices = 0;
    }

    void PlaybackEngine::allNotesOff (juce::MidiBuffer& out, int samplePos)
    {
        flush (songSrc, out, samplePos);
        flush (prevSrc, out, samplePos);
        songSrc.running = prevSrc.running = false;
    }

    void PlaybackEngine::emitOffs (Source& src, juce::MidiBuffer& out, const Segment& seg, bool all)
    {
        for (int i = 0; i < src.numVoices;)
        {
            auto& v = src.voices[(size_t) i];
            if (all || v.off < seg.b1 - beatEps)
            {
                out.addEvent (juce::MidiMessage::noteOff (v.ch, v.note), all ? seg.s0 : toSample (seg, juce::jmax (v.off, seg.b0)));
                v = src.voices[(size_t) --src.numVoices];
                continue;
            }
            ++i;
        }
    }

    void PlaybackEngine::noteOn (Source& src, juce::MidiBuffer& out, int sample, int note, int vel, double offBeat)
    {
        const auto ch = (juce::uint8) outChannel;

        // Re-trigger: close a still-sounding note of the same pitch first.
        for (int i = 0; i < src.numVoices; ++i)
        {
            if (src.voices[(size_t) i].note == note && src.voices[(size_t) i].ch == ch)
            {
                out.addEvent (juce::MidiMessage::noteOff (ch, note), sample);
                src.voices[(size_t) i] = src.voices[(size_t) --src.numVoices];
                break;
            }
        }

        if (src.numVoices == (int) src.voices.size())
        {
            out.addEvent (juce::MidiMessage::noteOff (src.voices[0].ch, src.voices[0].note), sample);
            src.voices[0] = src.voices[(size_t) --src.numVoices];
        }

        out.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel), sample);
        src.voices[(size_t) src.numVoices++] = { offBeat, (juce::uint8) note, ch };
    }

    void PlaybackEngine::render (Source& src, const Timeline& tl, juce::MidiBuffer& out,
                                 const Segment& seg, double offset, bool loop)
    {
        const auto& notes = tl.notes;

        auto emitRange = [&] (double r0, double r1, double base, double cap)
        {
            auto it = std::lower_bound (notes.begin(), notes.end(), r0,
                                        [] (const TimelineNote& n, double v) { return n.start < v; });

            for (; it != notes.end() && it->start < r1; ++it)
            {
                auto mapped = remapCache[it->note];
                if (mapped < 0)
                    continue;

                auto vel = juce::jlimit (1, 127, juce::roundToInt (it->velocity * velScale));
                auto onBeat = base + it->start;
                auto offBeat = base + juce::jmin (it->end, cap);
                noteOn (src, out, toSample (seg, onBeat), mapped, vel, offBeat);
            }
        };

        const auto l0 = seg.b0 - offset - beatEps, l1 = seg.b1 - offset - beatEps;

        if (! loop)
        {
            emitRange (l0, l1, offset, std::numeric_limits<double>::max());
            return;
        }

        const auto len = tl.length;
        if (len <= 0.0)
            return;

        auto k0 = (juce::int64) std::floor (l0 / len);
        auto k1 = (juce::int64) std::floor (l1 / len);

        for (auto k = k0; k <= k1; ++k)
        {
            auto base = (double) k * len;
            auto r0 = juce::jmax (l0 - base, 0.0);
            auto r1 = juce::jmin (l1 - base, len);
            if (r1 > r0)
                emitRange (r0, r1, offset + base, len);
        }
    }

    void PlaybackEngine::process (juce::MidiBuffer& out, int numSamples, const HostInfo& host)
    {
        if (numSamples <= 0)
            return;

        for (int i = 0; i < 128; ++i)
            remapCache[(size_t) i] = remap[(size_t) i].load (std::memory_order_relaxed);

        outChannel = juce::jlimit (1, 16, channel.load());
        velScale = velocityScale.load();

        const auto hostBpm = host.bpm > 0.0 ? host.bpm : fallbackTempo.load();
        hostPlaying.store (host.isPlaying);
        hostSigNum.store (host.sigNum);
        hostSigDen.store (host.sigDen);
        currentTempo.store (hostBpm);

        auto spbFor = [this] (double bpm) { return sampleRate * 60.0 / juce::jmax (1.0, bpm); };

        trackHostAnchor (host, spbFor (hostBpm), numSamples);

        auto previewTl = preview.get();
        auto songTl = song.get();
        const bool previewActive = previewTl != nullptr && previewTl->length > 0.0;

        //==========================================================================
        // Preview: loops the auditioned groove. Locked to the host bar grid while the
        // host plays, otherwise runs on its own clock.
        if (previewActive)
        {
            const int mode = host.isPlaying ? 1 : 0;
            const bool restart = prevSrc.lastId != previewTl->id || prevSrc.mode != mode || ! prevSrc.running;

            if (restart)
            {
                flush (prevSrc, out, 0);
                prevSrc.mode = mode;
                prevSrc.running = true;
                prevSrc.lastId = previewTl->id;
                prevSrc.clock = 0.0;

                if (mode == 1)
                {
                    auto barLen = host.sigNum * 4.0 / juce::jmax (1, host.sigDen);
                    prevSrc.anchor = host.barStartPpq >= 0.0 && host.barStartPpq <= host.ppq + 1.0e-9
                                         ? host.barStartPpq
                                         : std::floor (host.ppq / barLen + 1.0e-9) * barLen;
                }
                else
                {
                    prevSrc.anchor = 0.0;
                }

                prevSrc.expected = mode == 1 ? host.ppq : 0.0;
                playingPreviewId.store (previewTl->id);
            }

            auto bpm = hostBpm;
            if (mode == 0 && previewAtOriginalTempo.load() && previewTl->tempo > 0.0)
                bpm = previewTl->tempo;

            Segment seg;
            seg.samplesPerBeat = spbFor (bpm);
            seg.numSamples = numSamples;
            seg.s0 = 0;
            seg.b0 = mode == 1 ? host.ppq : prevSrc.clock;
            seg.b1 = seg.b0 + numSamples / seg.samplesPerBeat;

            if (mode == 1 && std::abs (seg.b0 - prevSrc.expected) > juce::jmax (1.0e-6, 2.0 / seg.samplesPerBeat))
            {
                // Host jumped (cycle / locate)
                flush (prevSrc, out, 0);
            }

            render (prevSrc, *previewTl, out, seg, prevSrc.anchor, true);
            emitOffs (prevSrc, out, seg, false);

            prevSrc.expected = seg.b1;
            if (mode == 0)
                prevSrc.clock = seg.b1;

            auto phase = std::fmod (seg.b0 - prevSrc.anchor, previewTl->length);
            previewPosition.store (phase < 0.0 ? phase + previewTl->length : phase);
        }
        else if (prevSrc.running || prevSrc.numVoices > 0)
        {
            flush (prevSrc, out, 0);
            prevSrc.running = false;
            prevSrc.lastId = -1;
            previewPosition.store (-1.0);
            playingPreviewId.store (0);
        }

        //==========================================================================
        // Song: follows the host timeline, or the internal transport when the host is
        // stopped. Muted while a preview is playing.
        if (host.isPlaying && internalPlaying.load())
            internalPlaying.store (false);

        auto seek = seekRequest.exchange (-1.0);
        if (seek >= 0.0)
        {
            internalPos = seek;
            flush (songSrc, out, 0);
            songSrc.expected = -1.0e9;
        }

        const bool internal = ! host.isPlaying && internalPlaying.load();
        const bool songRolling = host.isPlaying || internal;
        const double songOffset = host.isPlaying && songFromStartOnHostPlay.load() ? songAnchor : 0.0;

        if (songRolling && ! previewActive && songTl != nullptr)
        {
            Segment seg;
            seg.samplesPerBeat = spbFor (hostBpm);
            seg.numSamples = numSamples;
            seg.s0 = 0;
            seg.b0 = host.isPlaying ? host.ppq : internalPos;
            seg.b1 = seg.b0 + numSamples / seg.samplesPerBeat;

            if (! songSrc.running || songSrc.anchor != songOffset
                || std::abs (seg.b0 - songSrc.expected) > juce::jmax (1.0e-6, 2.0 / seg.samplesPerBeat))
                flush (songSrc, out, 0);

            songSrc.running = true;
            songSrc.anchor = songOffset;

            const auto songEnd = songTl->length;

            if (internal && songEnd > 0.0 && seg.b1 > songEnd && seg.b0 < songEnd)
            {
                // Internal transport reaches the end of the song inside this block.
                Segment first = seg;
                first.b1 = songEnd;
                render (songSrc, *songTl, out, first, 0.0, false);
                emitOffs (songSrc, out, first, false);

                const int endSample = toSample (seg, songEnd);
                Segment endSeg = seg;
                endSeg.s0 = endSample;
                emitOffs (songSrc, out, endSeg, true);

                if (loopSong.load())
                {
                    Segment second;
                    second.samplesPerBeat = seg.samplesPerBeat;
                    second.numSamples = numSamples;
                    second.s0 = endSample;
                    second.b0 = 0.0;
                    second.b1 = (numSamples - endSample) / seg.samplesPerBeat;
                    render (songSrc, *songTl, out, second, 0.0, false);
                    emitOffs (songSrc, out, second, false);
                    internalPos = second.b1;
                }
                else
                {
                    internalPos = 0.0;
                    internalPlaying.store (false);
                    songSrc.running = false;
                }

                songSrc.expected = internalPos;
            }
            else
            {
                render (songSrc, *songTl, out, seg, songOffset, false);
                emitOffs (songSrc, out, seg, false);
                songSrc.expected = seg.b1;

                if (internal)
                {
                    internalPos = seg.b1;
                    if (songEnd <= 0.0 || seg.b0 >= songEnd)
                    {
                        // Nothing to play (empty song or past the end)
                        if (loopSong.load() && songEnd > 0.0)
                            internalPos = 0.0;
                        else
                        {
                            internalPos = 0.0;
                            internalPlaying.store (false);
                        }
                    }
                }
            }

            songPosition.store (host.isPlaying ? juce::jmax (0.0, seg.b0 - songOffset) : internalPos);
        }
        else
        {
            if (songSrc.running || songSrc.numVoices > 0)
                flush (songSrc, out, 0);

            songSrc.running = false;
            songPosition.store (host.isPlaying ? juce::jmax (0.0, host.ppq - songOffset) : internalPos);

            if (internal)
                internalPos += numSamples / spbFor (hostBpm);
        }
    }
}
