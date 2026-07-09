cmake_minimum_required(VERSION 3.22)
project(VapeStation VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Allow a pre-downloaded tarball (JUCE_TARBALL env var) to skip the network fetch.
if(DEFINED ENV{JUCE_TARBALL} AND EXISTS "$ENV{JUCE_TARBALL}")
    set(VAPE_JUCE_URL "$ENV{JUCE_TARBALL}")
else()
    set(VAPE_JUCE_URL "https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.14.tar.gz")
endif()

FetchContent_Declare(JUCE URL "${VAPE_JUCE_URL}")
FetchContent_MakeAvailable(JUCE)

set(VAPE_SOURCES
    Source/GrainTables.cpp
    Source/GrainVoice.cpp
    Source/PluginProcessor.cpp
    Source/UI/ModKnob.cpp
    Source/UI/MatrixPanel.cpp
    Source/UI/TableViz.cpp
    Source/UI/PluginEditor.cpp
)

juce_add_plugin(VapeStation
    COMPANY_NAME "audiodude"
    BUNDLE_ID "xyz.audiodude.vapestation"
    PLUGIN_MANUFACTURER_CODE Vibz
    PLUGIN_CODE Vpst
    FORMATS VST3 Standalone
    PRODUCT_NAME "VapeStation"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
)

target_sources(VapeStation PRIVATE ${VAPE_SOURCES})

target_compile_definitions(VapeStation
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(VapeStation
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# Offline verification harness: renders audio through the processor and checks it.
juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
target_sources(RenderTest PRIVATE Tests/RenderTest.cpp ${VAPE_SOURCES})
target_compile_definitions(RenderTest
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)
target_link_libraries(RenderTest
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
)
