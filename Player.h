#pragma once

#include "Model.h"
#include "Synth.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <bitset>

namespace melosumm
{

struct PlayerSettings
{
    std::array<int,   kNumTracks> preset { { 0, 0, 0 } };
    std::array<float, kNumTracks> gain   { { 0.8f, 0.6f, 0.8f } };
    std::array<bool,  kNumTracks> mute   { { false, false, false } };
    float master = 0.8f;
    float reverb = 0.15f;
    bool  audioOn = true;
    bool  midiOut = true;
};

/** Spielt ein Pattern sample-genau ab: Audio ueber die eingebauten Synths
    und/oder MIDI-Ausgabe (Kanal 1 Melodie, 2 Akkorde, 3 Bass). */
class Player
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void setSettings (const PlayerSettings& s) noexcept;

    /** startBeat ist die fortlaufende Position (wird intern geloopt).
        Audio wird in 'out' ab outOffset ADDIERT. */
    void process (const Pattern* pattern, bool playing, double startBeat, double beatsPerSample,
                  juce::AudioBuffer<float>& out, int outOffset, int numSamples, juce::MidiBuffer* midi);

    void stopAll (juce::MidiBuffer* midi, int sampleOffset, bool hard = false);
    void preview (int track, int note, int velocity, int durationSamples) noexcept;

    double getSampleRate() const noexcept { return sr; }

private:
    void processChunk (const Pattern* pattern, bool playing, double startBeat, double bps,
                       juce::AudioBuffer<float>& out, int outOffset, int n, juce::MidiBuffer* midi);
    void renderSynths (int from, int to);
    void handleEvent (const Pattern::Event& e, juce::MidiBuffer* midi, int sampleOffset);

    double sr = 44100.0;
    int maxBlock = 512;
    PlayerSettings settings;

    std::array<TrackSynth, kNumTracks> synths;
    std::array<juce::AudioBuffer<float>, kNumTracks> trackBufs;
    juce::AudioBuffer<float> mix;
    juce::Reverb reverb;

    std::array<std::bitset<128>, kNumTracks> midiActive;
    bool wasPlaying = false;
    double expectedBeat = -1.0;
    const Pattern* lastPattern = nullptr;

    std::vector<const Pattern::Event*> pending;
    std::vector<int> pendingOffsets;

    int previewNote = -1, previewTrack = 0, previewRemaining = 0;
};

} // namespace melosumm
