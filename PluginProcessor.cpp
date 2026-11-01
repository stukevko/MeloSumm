#include "PluginProcessor.h"
#include <algorithm>
#include "PluginEditor.h"
#include "Quantizer.h"
#include "Arranger.h"
#include "Exporter.h"
#include "Synth.h"
#include <cmath>

using namespace melosumm;

namespace
{
    constexpr double kMaxRecordSeconds = 180.0;

    const juce::StringArray& keyChoices()
    {
        static const juce::StringArray s { "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return s;
    }

    juce::StringArray scaleChoices()
    {
        juce::StringArray s { "Auto (Dur/Moll)" };
        for (const auto& sc : getScales()) s.add (sc.name);
        return s;
    }

    const juce::StringArray& loopChoices()
    {
        static const juce::StringArray s { "Auto", "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte", "32 Takte" };
        return s;
    }

    const juce::StringArray& registerChoices()
    {
        static const juce::StringArray s { "Tief", "Mittel", "Hoch" };
        return s;
    }

    const char* const rearrangeParams[] = {
        ParamIDs::transpose, ParamIDs::loopBars, ParamIDs::chordStyle, ParamIDs::chordRate, ParamIDs::complexity,
        ParamIDs::chordReg, ParamIDs::variation, ParamIDs::bassPattern, ParamIDs::bassReg, ParamIDs::humanize,
        ParamIDs::keyRoot, ParamIDs::scale
    };
    const char* const requantizeParams[] = {
        ParamIDs::grid, ParamIDs::qStrength, ParamIDs::snapScale, ParamIDs::keyRoot, ParamIDs::scale, ParamIDs::trimStart
    };
    const char* const reanalyzeParams[] = { ParamIDs::sensitivity, ParamIDs::minNote };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MeloSummProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* label = "")
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                      AudioParameterFloatAttributes().withLabel (label)));
    };
    auto addC = [&] (const juce::String& id, const char* name, const StringArray& choices, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    auto addB = [&] (const juce::String& id, const char* name, bool def)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addI = [&] (const char* id, const char* name, int mn, int mx, int def)
    {
        l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, mn, mx, def));
    };

    // Erkennung
    addF (ParamIDs::sensitivity, "Empfindlichkeit", { 0.0f, 1.0f }, 0.5f);
    addF (ParamIDs::minNote, "Min. Notenlaenge", { 30.0f, 300.0f, 1.0f }, 70.0f, "ms");
    addC (ParamIDs::grid, "Raster", gridNames(), 2);
    addF (ParamIDs::qStrength, "Quantisierung", { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    addB (ParamIDs::snapScale, "Auf Tonart korrigieren", true);
    addC (ParamIDs::keyRoot, "Tonart", keyChoices(), 0);
    addC (ParamIDs::scale, "Skala", scaleChoices(), 0);
    addI (ParamIDs::transpose, "Transponieren", -24, 24, 0);
    addB (ParamIDs::trimStart, "Leere Takte entfernen", true);
    addC (ParamIDs::loopBars, "Loop-Laenge", loopChoices(), 0);

    // Arrangement
    addC (ParamIDs::chordStyle, "Akkord-Stil", chordStyleNames(), 0);
    addC (ParamIDs::chordRate, "Akkordwechsel", chordRateNames(), 1);
    addC (ParamIDs::complexity, "Akkord-Typ", complexityNames(), 0);
    addC (ParamIDs::chordReg, "Akkord-Lage", registerChoices(), 1);
    addI (ParamIDs::variation, "Variante", 0, 99, 0);
    addC (ParamIDs::bassPattern, "Bass-Muster", bassPatternNames(), 0);
    addC (ParamIDs::bassReg, "Bass-Lage", registerChoices(), 1);
    addF (ParamIDs::humanize, "Humanize", { 0.0f, 1.0f }, 0.0f);

    // Spuren
    const auto presets = presetNames();
    const int defaults[kNumTracks] = { presetIndex ("Synth Pluck"), presetIndex ("Soft Pad"), presetIndex ("808") };
    const char* trackNames[kNumTracks] = { "Melodie", "Akkorde", "Bass" };
    const float defVol[kNumTracks] = { 0.8f, 0.55f, 0.75f };
    for (int t = 0; t < kNumTracks; ++t)
    {
        addC (ParamIDs::presetId (t), (juce::String (trackNames[t]) + " Sound").toRawUTF8(), presets, defaults[t]);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::volId (t), 1 }, juce::String (trackNames[t]) + " Lautstaerke",
                                                      NormalisableRange<float> { 0.0f, 1.0f }, defVol[t]));
        addB (ParamIDs::muteId (t), (juce::String (trackNames[t]) + " Stumm").toRawUTF8(), false);
    }

    // Global
    addF (ParamIDs::master, "Master", { 0.0f, 1.0f }, 0.8f);
    addF (ParamIDs::reverb, "Hall", { 0.0f, 1.0f }, 0.2f);
    addB (ParamIDs::audioOn, "Sound an", true);
    addB (ParamIDs::midiOut, "MIDI-Out", true);
    addB (ParamIDs::monitor, "Eingang hoeren", false);
    addB (ParamIDs::metronome, "Metronom", true);
    addB (ParamIDs::countIn, "Vorzaehler", true);
    addB (ParamIDs::syncHost, "Mit DAW synchron", true);
    addB (ParamIDs::loopPlay, "Loop", true);
    addF (ParamIDs::bpm, "Tempo", { 40.0f, 240.0f, 0.1f }, 120.0f, "BPM");

    return l;
}

//==============================================================================
MeloSummProcessor::MeloSummProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Eingang", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    for (auto* id : rearrangeParams)  apvts.addParameterListener (id, this);
    for (auto* id : requantizeParams) apvts.addParameterListener (id, this);
    for (auto* id : reanalyzeParams)  apvts.addParameterListener (id, this);

    pattern = std::make_shared<Pattern>();
    rebuildPattern();
    startTimerHz (30);
}

MeloSummProcessor::~MeloSummProcessor()
{
    stopTimer();
    cancelAnalysis = true;
    pool.removeAllJobs (true, 5000);
    for (auto* id : rearrangeParams)  apvts.removeParameterListener (id, this);
    for (auto* id : requantizeParams) apvts.removeParameterListener (id, this);
    for (auto* id : reanalyzeParams)  apvts.removeParameterListener (id, this);
}

bool MeloSummProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MeloSummProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int block = std::max (64, samplesPerBlock);
    player.prepare (sr, block);
    player.setSettings (currentPlayerSettings());
    monoBuffer.setSize (1, block * 2);
    midiOutBuffer.ensureSize (4096);

    const size_t need = (size_t) (kMaxRecordSeconds * sr);
    if (recBuffer.size() != need && recState.load() == (int) RecState::Idle)
        recBuffer.assign (need, 0.0f);
}

//==============================================================================
void MeloSummProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // ---------------------------------------------------------------- Host
    bool hostPlaying = false;
    double hostPpq = 0.0, hostBpm = 0.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (auto p = pos->getPpqPosition()) hostPpq = *p;
            hostPlaying = pos->getIsPlaying();
        }
    }
    const bool sync = pb (ParamIDs::syncHost);
    const bool useHostTempo = sync && hostBpm > 1.0;
    const double bpm = useHostTempo ? hostBpm : (double) pf (ParamIDs::bpm);
    currentBpm.store (bpm);
    hostTempoActive.store (useHostTempo);
    const double bps = bpm / 60.0 / sr;
    const double spb = sr * 60.0 / bpm;
    const bool hostDrives = sync && hostPlaying;

    // ---------------------------------------------------------------- Eingang (mono)
    if (monoBuffer.getNumSamples() < n)
        monoBuffer.setSize (1, n, false, false, true);
    float* mono = monoBuffer.getWritePointer (0);
    juce::FloatVectorOperations::clear (mono, n);
    const int inCh = std::min (numIn, buffer.getNumChannels());
    for (int ch = 0; ch < inCh; ++ch)
        juce::FloatVectorOperations::addWithMultiply (mono, buffer.getReadPointer (ch), 1.0f / (float) inCh, n);
    {
        const float peak = juce::FloatVectorOperations::findMaximum (mono, n);
        const float trough = juce::FloatVectorOperations::findMinimum (mono, n);
        const float p = std::max (std::abs (peak), std::abs (trough));
        const float prev = inputLevel.load();
        inputLevel.store (std::max (p, prev * std::pow (0.5f, (float) n / (float) (sr * 0.12))));
    }

    // ---------------------------------------------------------------- Aufnahme-Steuerung
    auto finishRecording = [this]
    {
        recState.store ((int) RecState::Idle);
        metroActive = false;
        recFinished.store (true);
    };

    if (recStartRequested.exchange (false) && ! recBuffer.empty())
    {
        recPos.store (0);
        recBpm = bpm;
        internalPlay.store (false);
        player.stopAll (&midiOutBuffer, 0);
        metroBeatIndex = 0;
        metroSamplesToBeat = 0.0;

        if (hostDrives)
        {
            recState.store ((int) RecState::Armed);
            metroActive = false;
        }
        else if (pb (ParamIDs::countIn))
        {
            recState.store ((int) RecState::CountIn);
            countInRemaining = 4.0 * spb;
            metroActive = true;
        }
        else
        {
            recState.store ((int) RecState::Recording);
            metroActive = pb (ParamIDs::metronome);
        }
    }

    if (recStopRequested.exchange (false))
    {
        const auto st = (RecState) recState.load();
        if (st == RecState::Recording) finishRecording();
        else { recState.store ((int) RecState::Idle); metroActive = false; }
    }

    auto recordFrom = [&] (int from)
    {
        from = juce::jlimit (0, n, from);
        const int pos = recPos.load();
        const int space = (int) recBuffer.size() - pos;
        const int num = std::min (n - from, space);
        if (num > 0)
            std::copy (mono + from, mono + from + num, recBuffer.begin() + pos);
        recPos.store (pos + std::max (0, num));
        if (num < n - from)
            finishRecording();
    };

    switch ((RecState) recState.load())
    {
        case RecState::Armed:
            if (hostDrives)
            {
                const double barPos = std::ceil (hostPpq / 4.0 - 1.0e-6) * 4.0;
                const int off = (int) ((barPos - hostPpq) / bps);
                if (off < n)
                {
                    recState.store ((int) RecState::Recording);
                    recBpm = bpm;
                    recordFrom (std::max (0, off));
                }
            }
            countInBeatsLeft.store (0);
            break;

        case RecState::CountIn:
            if (countInRemaining <= n)
            {
                const int off = (int) countInRemaining;
                countInRemaining = 0.0;
                recState.store ((int) RecState::Recording);
                metroActive = pb (ParamIDs::metronome);
                recordFrom (off);
                countInBeatsLeft.store (0);
            }
            else
            {
                countInRemaining -= n;
                countInBeatsLeft.store ((int) std::ceil (countInRemaining / spb));
            }
            break;

        case RecState::Recording:
            recordFrom (0);
            if (metroActive && ! pb (ParamIDs::metronome)) metroActive = false;
            break;

        case RecState::Idle:
        default:
            break;
    }

    // ---------------------------------------------------------------- Ausgang vorbereiten
    if (pb (ParamIDs::monitor) && inCh > 0)
    {
        if (numOut >= 2 && inCh == 1)
            buffer.copyFrom (1, 0, buffer, 0, 0, n);
    }
    else
    {
        buffer.clear();
    }
    for (int ch = std::max (inCh, 2); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // ---------------------------------------------------------------- Wiedergabe
    midiOutBuffer.clear();
    auto pat = std::atomic_load (&pattern);
    const double L = pat != nullptr ? pat->lengthBeats : 16.0;

    if (internalRestart.exchange (false))
        internalBeat = 0.0;

    bool playing = false;
    double startBeat = 0.0;
    const bool idle = recState.load() == (int) RecState::Idle;

    if (idle && hostDrives)
    {
        playing = true;
        startBeat = hostPpq;
    }
    else if (idle && internalPlay.load())
    {
        playing = true;
        startBeat = internalBeat;
        internalBeat += n * bps;
        if (! pb (ParamIDs::loopPlay) && internalBeat >= L)
        {
            internalPlay.store (false);
            internalBeat = 0.0;
        }
    }

    player.setSettings (currentPlayerSettings());

    const int pr = previewRequest.exchange (-1);
    if (pr >= 0)
        player.preview (previewTrack.load(), pr, 100, (int) (0.35 * sr));

    player.process (pat.get(), playing, startBeat, bps, buffer, 0, n, &midiOutBuffer);

    playingNow.store (playing);
    if (playing && L > 0.0)
    {
        double pos = std::fmod (startBeat, L);
        if (pos < 0.0) pos += L;
        playPosition.store (pos);
    }

    // ---------------------------------------------------------------- Metronom
    if (metroActive && numOut > 0)
    {
        const int clickLen = (int) (0.035 * sr);
        for (int i = 0; i < n; ++i)
        {
            if (metroSamplesToBeat <= 0.0)
            {
                const bool accent = (metroBeatIndex % 4) == 0;
                clickFreq = accent ? 1760.0 : 1175.0;
                clickGain = accent ? 0.45f : 0.28f;
                clickRemaining = clickLen;
                clickPhase = 0.0;
                metroSamplesToBeat += spb;
                ++metroBeatIndex;
            }
            metroSamplesToBeat -= 1.0;

            if (clickRemaining > 0)
            {
                const float env = (float) clickRemaining / (float) clickLen;
                const float s = (float) std::sin (juce::MathConstants<double>::twoPi * clickPhase) * clickGain * env * env;
                clickPhase += clickFreq / sr;
                --clickRemaining;
                for (int ch = 0; ch < std::min (2, numOut); ++ch)
                    buffer.addSample (ch, i, s);
            }
        }
    }

    midiMessages.swapWith (midiOutBuffer);
}

//==============================================================================
PlayerSettings MeloSummProcessor::currentPlayerSettings() const
{
    PlayerSettings s;
    for (int t = 0; t < kNumTracks; ++t)
    {
        s.preset[(size_t) t] = (int) std::lround (apvts.getRawParameterValue (ParamIDs::presetId (t))->load());
        s.gain[(size_t) t]   = apvts.getRawParameterValue (ParamIDs::volId (t))->load();
        s.mute[(size_t) t]   = apvts.getRawParameterValue (ParamIDs::muteId (t))->load() > 0.5f;
    }
    s.master  = pf (ParamIDs::master);
    s.reverb  = pf (ParamIDs::reverb);
    s.audioOn = pb (ParamIDs::audioOn);
    s.midiOut = pb (ParamIDs::midiOut);
    return s;
}

//==============================================================================
void MeloSummProcessor::parameterChanged (const juce::String& id, float)
{
    for (auto* p : reanalyzeParams)  if (id == p) needReanalyze = true;
    for (auto* p : requantizeParams) if (id == p) needRequantize = true;
    for (auto* p : rearrangeParams)  if (id == p) needRearrange = true;
}

void MeloSummProcessor::timerCallback()
{
    if (recFinished.exchange (false))
    {
        const int len = recPos.load();
        if (len < (int) (sr * 0.3))
        {
            showMessage ("Aufnahme war zu kurz.");
        }
        else
        {
            std::vector<float> copy (recBuffer.begin(), recBuffer.begin() + len);
            launchAnalysis (std::move (copy), sr, recBpm, false);
        }
    }

    if (analysisDone.exchange (false))
        applyAnalysisResult();

    if (needReanalyze.load() && ! analysisRunning.load())
    {
        needReanalyze = false;
        if (hasRaw && ! analysisAudio.empty())
            launchAnalysis (analysisAudio, PitchAnalyzer::analysisRate, rawAnalysis.bpm, true);
    }

    if (needRequantize.exchange (false))
        requantize();

    if (needRearrange.exchange (false))
        rebuildPattern();

    while (retiredPatterns.size() > 8)
        retiredPatterns.pop_front();
}

void MeloSummProcessor::launchAnalysis (std::vector<float> monoAudio, double sampleRate, double bpm, bool atAnalysisRate)
{
    if (analysisRunning.load())
    {
        cancelAnalysis = true;
        pool.removeAllJobs (true, 3000);
    }
    cancelAnalysis = false;
    analysisRunning = true;

    PitchAnalyzer::Settings settings;
    settings.sensitivity = pf (ParamIDs::sensitivity);
    settings.minNoteMs = pf (ParamIDs::minNote);

    pool.addJob ([this, audio = std::move (monoAudio), sampleRate, bpm, atAnalysisRate, settings]
    {
        std::vector<float> lo = atAnalysisRate ? audio
                                               : PitchAnalyzer::toAnalysisRate (audio.data(), (int) audio.size(), sampleRate);
        auto res = PitchAnalyzer::analyze (lo, settings, bpm, &cancelAnalysis);
        if (! cancelAnalysis.load())
        {
            const juce::ScopedLock sl (resultLock);
            pendingResult = std::move (res);
            pendingAudio = std::move (lo);
            analysisDone = true;
        }
        analysisRunning = false;
    });
}

void MeloSummProcessor::applyAnalysisResult()
{
    {
        const juce::ScopedLock sl (resultLock);
        rawAnalysis = std::move (pendingResult);
        analysisAudio = std::move (pendingAudio);
        pendingResult = {};
        pendingAudio.clear();
    }
    hasRaw = true;

    if (rawAnalysis.notes.empty())
        showMessage ("Keine Noten erkannt. Lauter/naeher am Mikro summen oder die Empfindlichkeit erhoehen.");
    else
        showMessage (juce::String ((int) rawAnalysis.notes.size()) + " Noten erkannt.");

    requantize();
}

void MeloSummProcessor::requantize()
{
    if (! hasRaw)
        return;

    QuantizeSettings qs;
    qs.gridBeats  = gridBeatsForIndex (pi (ParamIDs::grid));
    qs.strength   = pf (ParamIDs::qStrength) / 100.0f;
    qs.snapToScale = pb (ParamIDs::snapScale);
    qs.keyRoot    = pi (ParamIDs::keyRoot) - 1;
    qs.scaleIndex = pi (ParamIDs::scale) - 1;
    qs.trimLeadingBars = pb (ParamIDs::trimStart);

    auto q = quantizeMelody (rawAnalysis, qs);
    {
        const juce::ScopedLock sl (melodyLock);
        pushUndo();
        melody = std::move (q);
    }
    rebuildPattern();
}

void MeloSummProcessor::rebuildPattern()
{
    auto p = std::make_shared<Pattern>();

    std::vector<NoteEvent> m;
    {
        const juce::ScopedLock sl (melodyLock);
        m = melody;
    }

    const int tr = pi (ParamIDs::transpose);
    for (auto& n : m)
        n.pitch = juce::jlimit (0, 127, n.pitch + tr);

    int rootParam = pi (ParamIDs::keyRoot) - 1;
    if (rootParam >= 0)
        rootParam = ((rootParam + tr) % 12 + 12) % 12;
    const int scaleParam = pi (ParamIDs::scale) - 1;
    const auto key = resolveKey (m, rootParam, scaleParam);

    // Loop-Laenge
    int bars = 4;
    const int loopIdx = pi (ParamIDs::loopBars);
    if (loopIdx == 0)
    {
        double lastEnd = 0.0;
        for (const auto& n : m) lastEnd = std::max (lastEnd, n.end());
        const int needed = std::max (1, (int) std::ceil (lastEnd / 4.0 - 1.0e-6));
        bars = 1;
        while (bars < needed && bars < 64) bars *= 2;
        if (m.empty()) bars = 4;
    }
    else
    {
        bars = 1 << (loopIdx - 1);
    }
    p->lengthBeats = bars * 4.0;

    ArrangeSettings as;
    as.keyRoot = key.root;
    as.scaleIndex = key.scaleIndex;
    as.loopLength = p->lengthBeats;
    as.chordRate = chordRateBeatsForIndex (pi (ParamIDs::chordRate));
    as.complexity = pi (ParamIDs::complexity);
    as.chordStyle = pi (ParamIDs::chordStyle);
    as.chordRegister = pi (ParamIDs::chordReg);
    as.bassPattern = pi (ParamIDs::bassPattern);
    as.bassRegister = pi (ParamIDs::bassReg);
    as.variation = pi (ParamIDs::variation);
    as.humanize = pf (ParamIDs::humanize);

    auto ar = arrange (m, as);

    p->tracks[kMelody] = std::move (m);
    p->tracks[kChords] = std::move (ar.chords);
    p->tracks[kBass]   = std::move (ar.bass);
    p->chords = std::move (ar.labels);
    p->keyRoot = key.root;
    p->scaleIndex = key.scaleIndex;
    p->minor = getScales()[(size_t) key.scaleIndex].minorFamily;
    p->keyName = keyDisplayName (key.root, key.scaleIndex);
    p->buildEvents();

    auto old = std::atomic_exchange (&pattern, p);
    if (old != nullptr)
        retiredPatterns.push_back (std::move (old));
    ++patternVersion;
}

std::shared_ptr<const Pattern> MeloSummProcessor::getPattern() const
{
    return std::atomic_load (&pattern);
}

//==============================================================================
void MeloSummProcessor::startRecording()
{
    if (analysisRunning.load() || recState.load() != (int) RecState::Idle)
        return;
    internalPlay = false;
    recStartRequested = true;
}

void MeloSummProcessor::stopAll()
{
    if (recState.load() != (int) RecState::Idle)
        recStopRequested = true;
    internalPlay = false;
}

void MeloSummProcessor::togglePlay()
{
    if (recState.load() != (int) RecState::Idle)
        return;
    const bool now = ! internalPlay.load();
    if (now) internalRestart = true;
    internalPlay = now;
}

double MeloSummProcessor::getRecordedSeconds() const noexcept
{
    return recPos.load() / sr;
}

void MeloSummProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        showMessage ("Datei konnte nicht gelesen werden: " + file.getFileName());
        return;
    }

    const double fileRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
    const auto maxSamples = (juce::int64) (300.0 * fileRate);
    const int len = (int) std::min (reader->lengthInSamples, maxSamples);
    if (len <= 0)
    {
        showMessage ("Datei ist leer.");
        return;
    }

    juce::AudioBuffer<float> buf ((int) std::max (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);

    std::vector<float> monoData ((size_t) len, 0.0f);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        juce::FloatVectorOperations::addWithMultiply (monoData.data(), buf.getReadPointer (ch), 1.0f / (float) buf.getNumChannels(), len);

    showMessage ("Analysiere " + file.getFileName() + " ...");
    launchAnalysis (std::move (monoData), fileRate, getCurrentBpm(), false);
}

//==============================================================================
std::vector<NoteEvent> MeloSummProcessor::getMelody() const
{
    const juce::ScopedLock sl (melodyLock);
    return melody;
}

void MeloSummProcessor::pushUndo()
{
    undoStack.push_back (melody);
    if (undoStack.size() > 60)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void MeloSummProcessor::setMelody (const std::vector<NoteEvent>& notes, bool undoable)
{
    {
        const juce::ScopedLock sl (melodyLock);
        if (undoable) pushUndo();
        melody = notes;
        std::stable_sort (melody.begin(), melody.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });
    }
    rebuildPattern();
}

void MeloSummProcessor::undo()
{
    {
        const juce::ScopedLock sl (melodyLock);
        if (undoStack.empty()) return;
        redoStack.push_back (melody);
        melody = undoStack.back();
        undoStack.pop_back();
    }
    rebuildPattern();
}

void MeloSummProcessor::redo()
{
    {
        const juce::ScopedLock sl (melodyLock);
        if (redoStack.empty()) return;
        undoStack.push_back (melody);
        melody = redoStack.back();
        redoStack.pop_back();
    }
    rebuildPattern();
}

void MeloSummProcessor::clearMelody()
{
    {
        const juce::ScopedLock sl (melodyLock);
        pushUndo();
        melody.clear();
    }
    hasRaw = false;
    rawAnalysis = {};
    analysisAudio.clear();
    rebuildPattern();
    showMessage ("Alles geloescht.");
}

void MeloSummProcessor::newChordVariation()
{
    if (auto* p = apvts.getParameter (ParamIDs::variation))
    {
        const int cur = pi (ParamIDs::variation);
        const int next = (cur + 1) % 100;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
    }
}

int MeloSummProcessor::getTranspose() const { return pi (ParamIDs::transpose); }

double MeloSummProcessor::editGridBeats() const
{
    const double g = gridBeatsForIndex (pi (ParamIDs::grid));
    return g > 0.0 ? g : 0.25;
}

void MeloSummProcessor::showMessage (const juce::String& msg)
{
    message = msg;
    messageTime = juce::Time::getMillisecondCounter();
}

juce::String MeloSummProcessor::getStatusText() const
{
    switch ((RecState) recState.load())
    {
        case RecState::Armed:     return "Warte auf den naechsten Takt in FL Studio ...";
        case RecState::CountIn:   return "Vorzaehler ... " + juce::String (countInBeatsLeft.load());
        case RecState::Recording:
        {
            const int secs = (int) getRecordedSeconds();
            return juce::String::formatted ("Aufnahme %d:%02d  -  summ jetzt deine Melodie!", secs / 60, secs % 60);
        }
        case RecState::Idle:
        default: break;
    }

    if (analysisRunning.load())
        return "Analysiere deine Melodie ...";

    if (message.isNotEmpty() && juce::Time::getMillisecondCounter() - messageTime < 5000)
        return message;

    auto p = getPattern();
    const auto m = getMelody();
    if (m.empty())
        return "Bereit. Druecke AUFNAHME und summ los - oder zeichne Noten direkt ein.";

    return juce::String ((int) m.size()) + " Noten  |  " + (p ? p->keyName : juce::String())
           + "  |  " + juce::String ((int) ((p ? p->lengthBeats : 16.0) / 4.0)) + " Takte";
}

//==============================================================================
juce::File MeloSummProcessor::exportMidi (int trackMask)
{
    auto p = getPattern();
    if (p == nullptr || p->isEmpty())
        return {};
    const double bpm = getCurrentBpm();
    auto f = Exporter::makeExportFile (Exporter::fileBaseName (*p, bpm, trackMask), ".mid");
    return Exporter::writeMidi (*p, bpm, trackMask, f) ? f : juce::File();
}

juce::File MeloSummProcessor::exportWav (int trackMask)
{
    auto p = getPattern();
    if (p == nullptr || p->isEmpty())
        return {};
    const double bpm = getCurrentBpm();
    const double rate = sr > 0.0 ? sr : 44100.0;
    auto audio = Exporter::renderLoop (*p, bpm, currentPlayerSettings(), trackMask, rate);
    auto f = Exporter::makeExportFile (Exporter::fileBaseName (*p, bpm, trackMask), ".wav");
    return Exporter::writeWav (audio, rate, f) ? f : juce::File();
}

void MeloSummProcessor::saveAllTo (const juce::File& folder)
{
    auto p = getPattern();
    if (p == nullptr || p->isEmpty() || ! folder.isDirectory())
        return;
    const double bpm = getCurrentBpm();
    const double rate = sr > 0.0 ? sr : 44100.0;
    const auto settings = currentPlayerSettings();

    for (int mask : { Exporter::kAll, 1 << kMelody, 1 << kChords, 1 << kBass })
    {
        const auto base = Exporter::fileBaseName (*p, bpm, mask);
        Exporter::writeMidi (*p, bpm, mask, folder.getNonexistentChildFile (base, ".mid", false));
        auto audio = Exporter::renderLoop (*p, bpm, settings, mask, rate);
        Exporter::writeWav (audio, rate, folder.getNonexistentChildFile (base, ".wav", false));
    }
    showMessage ("Gespeichert in " + folder.getFullPathName());
    folder.revealToUser();
}

//==============================================================================
void MeloSummProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr)
        return;

    {
        const juce::ScopedLock sl (melodyLock);
        auto* mel = xml->createNewChildElement ("MELODY");
        for (const auto& n : melody)
        {
            auto* e = mel->createNewChildElement ("N");
            e->setAttribute ("s", n.start);
            e->setAttribute ("l", n.length);
            e->setAttribute ("p", n.pitch);
            e->setAttribute ("v", n.velocity);
        }
    }

    if (hasRaw)
    {
        auto* raw = xml->createNewChildElement ("RAW");
        raw->setAttribute ("bpm", rawAnalysis.bpm);
        raw->setAttribute ("off", rawAnalysis.beatOffsetSec);
        raw->setAttribute ("dur", rawAnalysis.durationSec);
        for (const auto& r : rawAnalysis.notes)
        {
            auto* e = raw->createNewChildElement ("R");
            e->setAttribute ("s", r.startSec);
            e->setAttribute ("e", r.endSec);
            e->setAttribute ("m", (double) r.midi);
            e->setAttribute ("lv", (double) r.level);
        }
    }

    copyXmlToBinary (*xml, destData);
}

void MeloSummProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    std::vector<NoteEvent> loaded;
    if (auto* mel = xml->getChildByName ("MELODY"))
    {
        for (auto* e : mel->getChildIterator())
        {
            NoteEvent n;
            n.start = e->getDoubleAttribute ("s");
            n.length = e->getDoubleAttribute ("l", 0.5);
            n.pitch = juce::jlimit (0, 127, e->getIntAttribute ("p", 60));
            n.velocity = juce::jlimit (1, 127, e->getIntAttribute ("v", 100));
            loaded.push_back (n);
        }
    }

    AnalysisResult raw;
    bool gotRaw = false;
    if (auto* r = xml->getChildByName ("RAW"))
    {
        gotRaw = true;
        raw.bpm = r->getDoubleAttribute ("bpm", 120.0);
        raw.beatOffsetSec = r->getDoubleAttribute ("off", 0.0);
        raw.durationSec = r->getDoubleAttribute ("dur", 0.0);
        for (auto* e : r->getChildIterator())
        {
            RawNote n;
            n.startSec = e->getDoubleAttribute ("s");
            n.endSec = e->getDoubleAttribute ("e");
            n.midi = (float) e->getDoubleAttribute ("m", 60.0);
            n.level = (float) e->getDoubleAttribute ("lv", 0.1);
            raw.notes.push_back (n);
        }
    }

    xml->deleteAllChildElementsWithTagName ("MELODY");
    xml->deleteAllChildElementsWithTagName ("RAW");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    {
        const juce::ScopedLock sl (melodyLock);
        melody = std::move (loaded);
        undoStack.clear();
        redoStack.clear();
    }
    rawAnalysis = std::move (raw);
    hasRaw = gotRaw;
    analysisAudio.clear();

    // Parameter-Listener haben evtl. Re-Quantisierung angefordert -> gespeicherte Melodie behalten
    needRequantize = false;
    needReanalyze = false;
    needRearrange = true;
}

//==============================================================================
juce::AudioProcessorEditor* MeloSummProcessor::createEditor()
{
    return new MeloSummEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeloSummProcessor();
}
