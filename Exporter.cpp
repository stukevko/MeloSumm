#include "Exporter.h"
#include <algorithm>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace melosumm
{
namespace Exporter
{

juce::File exportFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("MeloSumm Exporte");
    if (! dir.isDirectory())
        dir.createDirectory();
    if (! dir.isDirectory())
    {
        dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MeloSumm");
        dir.createDirectory();
    }
    return dir;
}

juce::String fileBaseName (const Pattern& p, double bpm, int trackMask)
{
    juce::String part;
    if (trackMask == kAll)              part = "Komplett";
    else if (trackMask == 1 << kMelody) part = "Melodie";
    else if (trackMask == 1 << kChords) part = "Akkorde";
    else if (trackMask == 1 << kBass)   part = "Bass";
    else                                part = "Mix";

    juce::String key = pitchClassName (p.keyRoot) + (p.minor ? "m" : "");
    return "MeloSumm " + part + " " + juce::String (juce::roundToInt (bpm)) + "bpm " + key;
}

juce::File makeExportFile (const juce::String& baseName, const juce::String& extension)
{
    auto dir = exportFolder();
    auto f = dir.getChildFile (juce::File::createLegalFileName (baseName) + extension);
    if (f.existsAsFile() && ! f.deleteFile())
        f = dir.getNonexistentChildFile (juce::File::createLegalFileName (baseName), extension, false);
    return f;
}

bool writeMidi (const Pattern& p, double bpm, int trackMask, const juce::File& dest)
{
    constexpr int tpq = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);

    static const char* names[kNumTracks] = { "Melodie", "Akkorde", "Bass" };

    int numSelected = 0;
    for (int t = 0; t < kNumTracks; ++t)
        if (trackMask & (1 << t)) ++numSelected;
    if (numSelected == 0)
        return false;

    auto addMeta = [&] (juce::MidiMessageSequence& seq)
    {
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm)), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    };

    if (numSelected > 1)
    {
        juce::MidiMessageSequence meta;
        meta.addEvent (juce::MidiMessage::textMetaEvent (3, "MeloSumm"), 0.0);
        addMeta (meta);
        meta.addEvent (juce::MidiMessage::endOfTrack(), p.lengthBeats * tpq);
        mf.addTrack (meta);
    }

    for (int t = 0; t < kNumTracks; ++t)
    {
        if (! (trackMask & (1 << t)))
            continue;

        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, names[t]), 0.0);
        if (numSelected == 1)
            addMeta (seq);

        for (const auto& n : p.tracks[(size_t) t])
        {
            if (n.start >= p.lengthBeats) continue;
            const double endB = std::min (n.end(), p.lengthBeats);
            const double on  = std::round (n.start * tpq);
            const double off = std::max (on + 1.0, std::round (endB * tpq) - 1.0);
            seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, (juce::uint8) juce::jlimit (1, 127, n.velocity)), on);
            seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), off);
        }
        seq.updateMatchedPairs();
        seq.addEvent (juce::MidiMessage::endOfTrack(), p.lengthBeats * tpq);
        mf.addTrack (seq);
    }

    if (dest.existsAsFile())
        dest.deleteFile();
    juce::FileOutputStream os (dest);
    if (! os.openedOk())
        return false;
    const bool ok = mf.writeTo (os, 1);
    os.flush();
    return ok;
}

juce::AudioBuffer<float> renderLoop (const Pattern& p, double bpm, PlayerSettings settings,
                                     int trackMask, double sampleRate)
{
    const double secondsPerBeat = 60.0 / bpm;
    const int loopSamples = (int) std::round (p.lengthBeats * secondsPerBeat * sampleRate);
    juce::AudioBuffer<float> out (2, std::max (1, loopSamples));
    out.clear();
    if (loopSamples <= 0)
        return out;

    // Einzelspuren immer hoerbar exportieren; beim Komplett-Mix gelten die Mute-Schalter
    if (trackMask != kAll)
        for (int t = 0; t < kNumTracks; ++t)
            settings.mute[(size_t) t] = ! (trackMask & (1 << t));
    settings.audioOn = true;
    settings.midiOut = false;

    Player player;
    constexpr int block = 1024;
    player.prepare (sampleRate, block);
    player.setSettings (settings);

    const double bps = 1.0 / (secondsPerBeat * sampleRate);
    juce::AudioBuffer<float> tmp (2, block);

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int pos = 0; pos < loopSamples; pos += block)
        {
            const int n = std::min (block, loopSamples - pos);
            tmp.clear();
            const double beat = (double) (pass * loopSamples + pos) * bps;
            player.process (&p, true, beat, bps, tmp, 0, n, nullptr);
            if (pass == 1)
            {
                out.copyFrom (0, pos, tmp, 0, 0, n);
                out.copyFrom (1, pos, tmp, 1, 0, n);
            }
        }
    }
    return out;
}

bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& dest)
{
    if (dest.existsAsFile())
        dest.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (dest);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return false;

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (sampleRate)
                                                   .withNumChannels (audio.getNumChannels())
                                                   .withBitsPerSample (24));
    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

} // namespace Exporter
} // namespace melosumm
