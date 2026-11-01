cmake_minimum_required(VERSION 3.22)

# Statische MSVC-Runtime: das Plugin laeuft ohne extra "Visual C++ Redistributable"
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MeloSumm VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MELOSUMM_LOCAL_TEST "Linux-Testbuild (nur Entwicklung)" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

if(MELOSUMM_LOCAL_TEST)
    set(MELOSUMM_FORMATS VST3)
else()
    set(MELOSUMM_FORMATS VST3 Standalone)
endif()

juce_add_plugin(MeloSumm
    COMPANY_NAME                "MeloSumm"
    PLUGIN_MANUFACTURER_CODE    Mlsm
    PLUGIN_CODE                 Msm1
    FORMATS                     ${MELOSUMM_FORMATS}
    PRODUCT_NAME                "MeloSumm"
    DESCRIPTION                 "Summen -> MIDI -> Melodie, Akkorde und Bass"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Generator
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "MeloSumm braucht das Mikrofon, um deine Melodie aufzunehmen."
)

target_sources(MeloSumm PRIVATE
    Model.cpp PitchAnalyzer.cpp Quantizer.cpp Arranger.cpp Synth.cpp Player.cpp Exporter.cpp
    PluginProcessor.cpp PluginEditor.cpp LookAndFeel.cpp PianoRoll.cpp Widgets.cpp
)

target_include_directories(MeloSumm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(MeloSumm PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    $<$<BOOL:${MELOSUMM_LOCAL_TEST}>:JUCE_ALSA=0>
    $<$<BOOL:${MELOSUMM_LOCAL_TEST}>:JUCE_JACK=0>
)
target_compile_options(MeloSumm PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /bigobj>)

target_link_libraries(MeloSumm
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
