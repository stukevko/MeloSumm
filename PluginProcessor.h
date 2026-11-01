#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Model.h"
#include "Player.h"
#include "PitchAnalyzer.h"
#include <atomic>
#include <deque>
#include <memory>

namespace ParamIDs
{
    // Erkennung
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* minNote     = "minNote";
    inline constexpr const char* grid        = "grid";
    inline constexpr const char* qStrength   = "qStrength";
    inline constexpr const char* snapScale   = "snapScale";
    inline constexpr const char* keyRoot     = "keyRoot";
    inline constexpr const char* scale       = "scale";
    inline constexpr const char* transpose   = "transpose";
    inline constexpr const char* trimStart   = "trimStart";
    inline constexpr const char* loopBars    = "loopBars";
    // Arrangement
    inline constexpr const char* chordStyle  = "chordStyle";
    inline constexpr const char* chordRate   = "chordRate";
    inline constexpr const char* complexity  = "chordComplexity";
    inline constexpr const char* chordReg    = "chordRegister";
    inline constexpr const char* variation   = "variation";
    inline constexpr const char* bassPattern = "bassPattern";
    inline constexpr const char* bassReg     = "bassRegister";
    inline constexpr const char* humanize    = "humanize";
    // Spuren
    inline const juce::String presetId (int t) { static const char* p[] = { "melPreset", "chdPreset", "bassPreset" }; return p[t]; }
    inline const juce::String volId    (int t) { static const char* p[] = { "melVol", "chdVol", "bassVol" }; return p[t]; }
    inline const juce::String muteId   (int t) { static const char* p[] = { "melMute", "chdMute", "bassMute" }; return p[t]; }
    // Global
    inline constexpr const char* master      = "master";
    inline constexpr const char* reverb      = "reverb";
    inline constexpr const char* audioOn     = "audioOn";
    inline constexpr const char* midiOut     = "midiOut";
    inline constexpr const char* monitor     = "monitor";
    inline constexpr const char* metronome   = "metronome";
    inline constexpr const char* countIn     = "countIn";
    inline constexpr const char* syncHost    = "syncHost";
    inline constexpr const char* loopPlay    = "loopPlay";
    inline constexpr const char* bpm         = "bpm";
}

class MeloSummProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer
{
public:
    MeloSummProcessor();
    ~MeloSummProcessor() override;

    // ---------------------------------------------------------------- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MeloSumm"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- fuer die Oberflaeche
    juce::AudioProcessorValueTreeState apvts;

    enum class RecState { Idle = 0, Armed, CountIn, Recording };

    void startRecording();
    void stopAll();                 // Aufnahme beenden bzw. Wiedergabe stoppen
    void togglePlay();
    bool isInternalPlaying() const noexcept { return internalPlay.load(); }
    RecState getRecState() const noexcept { return (RecState) recState.load(); }
    bool isAnalyzing() const noexcept { return analysisRunning.load(); }
    double getRecordedSeconds() const noexcept;
    int getCountInBeatsLeft() const noexcept { return countInBeatsLeft.load(); }

    void loadAudioFile (const juce::File& file);

    std::shared_ptr<const melosumm::Pattern> getPattern() const;
    int getPatternVersion() const noexcept { return patternVersion.load(); }

    std::vector<melosumm::NoteEvent> getMelody() const;
    void setMelody (const std::vector<melosumm::NoteEvent>& notes, bool undoable);
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void clearMelody();
    void newChordVariation();

    void previewNote (int pitch, int track = melosumm::kMelody) noexcept { previewTrack.store (track); previewRequest.store (pitch); }

    double getCurrentBpm() const noexcept { return currentBpm.load(); }
    bool isHostTempo() const noexcept { return hostTempoActive.load(); }
    double getPlayPosition() const noexcept { return playPosition.load(); }
    bool isPlayingNow() const noexcept { return playingNow.load(); }
    float getInputLevel() const noexcept { return inputLevel.load(); }

    juce::String getStatusText() const;

    juce::File exportMidi (int trackMask);
    juce::File exportWav (int trackMask);
    void saveAllTo (const juce::File& folder);

    int getTranspose() const;
    double editGridBeats() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;

    void launchAnalysis (std::vector<float> monoAudio, double sampleRate, double bpm, bool alreadyAtAnalysisRate);
    void applyAnalysisResult();
    void requantize();
    void rebuildPattern();
    melosumm::PlayerSettings currentPlayerSettings() const;
    void showMessage (const juce::String& msg);
    void pushUndo();

    float pf (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    int   pi (const char* id) const { return (int) std::lround (apvts.getRawParameterValue (id)->load()); }
    bool  pb (const char* id) const { return apvts.getRawParameterValue (id)->load() > 0.5f; }

    // ---------------------------------------------------------------- Audio-Thread-Zustand
    double sr = 44100.0;
    melosumm::Player player;
    juce::MidiBuffer midiOutBuffer;
    juce::AudioBuffer<float> monoBuffer;

    std::shared_ptr<melosumm::Pattern> pattern;
    std::deque<std::shared_ptr<melosumm::Pattern>> retiredPatterns;
    std::atomic<int> patternVersion { 0 };

    std::vector<float> recBuffer;
    std::atomic<int> recPos { 0 };
    std::atomic<int> recState { 0 };
    std::atomic<bool> recStartRequested { false }, recStopRequested { false }, recFinished { false };
    std::atomic<int> countInBeatsLeft { 0 };
    double countInRemaining = 0.0;
    double recBpm = 120.0;

    // Metronom
    bool metroActive = false;
    double metroSamplesToBeat = 0.0;
    int metroBeatIndex = 0;
    int clickRemaining = 0;
    double clickPhase = 0.0, clickFreq = 1000.0;
    float clickGain = 0.0f;

    std::atomic<bool> internalPlay { false }, internalRestart { false };
    double internalBeat = 0.0;

    std::atomic<int> previewRequest { -1 }, previewTrack { 0 };

    std::atomic<double> currentBpm { 120.0 }, playPosition { 0.0 };
    std::atomic<bool> playingNow { false }, hostTempoActive { false };
    std::atomic<float> inputLevel { 0.0f };

    // ---------------------------------------------------------------- Message-Thread-Zustand
    mutable juce::CriticalSection melodyLock;
    std::vector<melosumm::NoteEvent> melody;
    std::vector<std::vector<melosumm::NoteEvent>> undoStack, redoStack;

    melosumm::AnalysisResult rawAnalysis;
    bool hasRaw = false;
    std::vector<float> analysisAudio;   // Aufnahme in Analyse-Samplerate (fuer Neu-Analyse)

    juce::ThreadPool pool { 1 };
    std::atomic<bool> cancelAnalysis { false }, analysisRunning { false }, analysisDone { false };
    juce::CriticalSection resultLock;
    melosumm::AnalysisResult pendingResult;
    std::vector<float> pendingAudio;

    std::atomic<bool> needRequantize { false }, needRearrange { true }, needReanalyze { false };

    juce::String message;
    juce::uint32 messageTime = 0;

    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeloSummProcessor)
};
