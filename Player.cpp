#include "Player.h"
#include <algorithm>
#include <cmath>

namespace melosumm
{

void Player::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = std::max (32, maxBlockSize);
    for (auto& s : synths) s.prepare (sr);
    for (auto& b : trackBufs) b.setSize (2, maxBlock);
    mix.setSize (2, maxBlock);
    reverb.setSampleRate (sr);
    reverb.reset();
    pending.reserve (4096);
    pendingOffsets.reserve (4096);
    for (auto& m : midiActive) m.reset();
    wasPlaying = false;
    expectedBeat = -1.0;
    lastPattern = nullptr;
    setSettings (settings);
}

void Player::setSettings (const PlayerSettings& s) noexcept
{
    const bool reverbChanged = std::abs (s.reverb - settings.reverb) > 1.0e-4f;
    const bool audioTurnedOff = settings.audioOn && ! s.audioOn;
    settings = s;

    for (int t = 0; t < kNumTracks; ++t)
        synths[(size_t) t].setPreset (settings.preset[(size_t) t]);

    if (audioTurnedOff)
        for (auto& syn : synths) syn.allNotesOff (true);

    juce::Reverb::Parameters p;
    p.roomSize = 0.78f;
    p.damping = 0.45f;
    p.wetLevel = settings.reverb * 0.45f;
    p.dryLevel = 1.0f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    if (reverbChanged || std::abs (reverb.getParameters().wetLevel - p.wetLevel) > 1.0e-5f)
        reverb.setParameters (p);
}

void Player::preview (int track, int note, int velocity, int durationSamples) noexcept
{
    if (previewNote >= 0)
        synths[(size_t) previewTrack].noteOff (previewNote);
    previewTrack = juce::jlimit (0, kNumTracks - 1, track);
    previewNote = note;
    previewRemaining = durationSamples;
    synths[(size_t) previewTrack].noteOn (note, velocity);
}

void Player::stopAll (juce::MidiBuffer* midi, int sampleOffset, bool hard)
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        synths[(size_t) t].allNotesOff (hard);
        for (int n = 0; n < 128; ++n)
        {
            if (midiActive[(size_t) t][(size_t) n])
            {
                if (midi != nullptr)
                    midi->addEvent (juce::MidiMessage::noteOff (t + 1, n), sampleOffset);
                midiActive[(size_t) t][(size_t) n] = false;
            }
        }
    }
}

void Player::handleEvent (const Pattern::Event& e, juce::MidiBuffer* midi, int sampleOffset)
{
    const int t = e.track;
    auto& active = midiActive[(size_t) t];

    if (e.on)
    {
        if (settings.mute[(size_t) t])
            return;

        if (settings.audioOn)
            synths[(size_t) t].noteOn (e.pitch, e.velocity);

        if (midi != nullptr && settings.midiOut)
        {
            if (active[(size_t) e.pitch])
                midi->addEvent (juce::MidiMessage::noteOff (t + 1, e.pitch), sampleOffset);
            midi->addEvent (juce::MidiMessage::noteOn (t + 1, e.pitch, (juce::uint8) juce::jlimit (1, 127, e.velocity)), sampleOffset);
            active[(size_t) e.pitch] = true;
        }
    }
    else
    {
        synths[(size_t) t].noteOff (e.pitch);
        if (active[(size_t) e.pitch])
        {
            if (midi != nullptr)
                midi->addEvent (juce::MidiMessage::noteOff (t + 1, e.pitch), sampleOffset);
            active[(size_t) e.pitch] = false;
        }
    }
}

void Player::renderSynths (int from, int to)
{
    if (to <= from || ! settings.audioOn)
        return;
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& b = trackBufs[(size_t) t];
        synths[(size_t) t].render (b.getWritePointer (0) + from, b.getWritePointer (1) + from, to - from);
    }
}

void Player::process (const Pattern* pattern, bool playing, double startBeat, double beatsPerSample,
                      juce::AudioBuffer<float>& out, int outOffset, int numSamples, juce::MidiBuffer* midi)
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        processChunk (pattern, playing, startBeat + done * beatsPerSample, beatsPerSample, out, outOffset + done, n, midi);
        done += n;
    }
}

void Player::processChunk (const Pattern* pattern, bool playing, double startBeat, double bps,
                           juce::AudioBuffer<float>& out, int outOffset, int n, juce::MidiBuffer* midi)
{
    for (auto& b : trackBufs)
        b.clear (0, n);

    pending.clear();
    pendingOffsets.clear();

    const bool canPlay = playing && pattern != nullptr && pattern->lengthBeats > 0.0 && bps > 0.0;

    if (! canPlay)
    {
        if (wasPlaying)
            stopAll (midi, outOffset);
        wasPlaying = false;
        expectedBeat = -1.0;
        lastPattern = pattern;
    }
    else
    {
        if (wasPlaying && (pattern != lastPattern || std::abs (startBeat - expectedBeat) > 1.0e-3))
            stopAll (midi, outOffset);

        wasPlaying = true;
        lastPattern = pattern;
        expectedBeat = startBeat + n * bps;

        const double L = pattern->lengthBeats;
        double a = std::fmod (startBeat, L);
        if (a < 0.0) a += L;
        const double span = n * bps;
        const auto& ev = pattern->events;

        auto collect = [&] (double from, double to, double offsetBeats)
        {
            auto it = std::lower_bound (ev.begin(), ev.end(), from,
                                        [] (const Pattern::Event& e, double b) { return e.beat < b; });
            for (; it != ev.end() && it->beat < to; ++it)
            {
                if (pending.size() >= pending.capacity())
                    break;
                const int off = juce::jlimit (0, n - 1, (int) ((it->beat - from + offsetBeats) / bps));
                pending.push_back (&*it);
                pendingOffsets.push_back (off);
            }
        };

        collect (a, std::min (a + span, L), 0.0);
        if (a + span > L)
            collect (0.0, std::min (a + span - L, L), L - a);
    }

    int cursor = 0;
    for (size_t k = 0; k < pending.size(); ++k)
    {
        const int off = pendingOffsets[k];
        if (off > cursor)
        {
            renderSynths (cursor, off);
            cursor = off;
        }
        handleEvent (*pending[k], midi, outOffset + off);
    }

    if (previewRemaining > 0)
    {
        previewRemaining -= n;
        if (previewRemaining <= 0 && previewNote >= 0)
        {
            const int off = juce::jlimit (cursor, n, n + previewRemaining);
            renderSynths (cursor, off);
            cursor = off;
            synths[(size_t) previewTrack].noteOff (previewNote);
            previewNote = -1;
        }
    }

    renderSynths (cursor, n);

    if (! settings.audioOn)
        return;

    mix.clear (0, n);
    for (int t = 0; t < kNumTracks; ++t)
    {
        const float g = settings.mute[(size_t) t] ? 0.0f : settings.gain[(size_t) t];
        if (g <= 0.0f) continue;
        mix.addFrom (0, 0, trackBufs[(size_t) t], 0, 0, n, g);
        mix.addFrom (1, 0, trackBufs[(size_t) t], 1, 0, n, g);
    }

    if (settings.reverb > 0.001f)
        reverb.processStereo (mix.getWritePointer (0), mix.getWritePointer (1), n);

    const float m = settings.master;
    const int outCh = out.getNumChannels();
    for (int ch = 0; ch < std::min (2, outCh); ++ch)
    {
        const float* src = mix.getReadPointer (ch);
        float* dst = out.getWritePointer (ch, outOffset);
        for (int i = 0; i < n; ++i)
            dst[i] += std::tanh (src[i] * m);
    }
    if (outCh == 1)
    {
        const float* r = mix.getReadPointer (1);
        float* dst = out.getWritePointer (0, outOffset);
        for (int i = 0; i < n; ++i)
            dst[i] += std::tanh (r[i] * m);
    }
}

} // namespace melosumm
