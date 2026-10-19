cmake_minimum_required(VERSION 3.22)

project(BOTHER VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

option(BOTHER_BUILD_TESTS "Build the offline DSP test, demo render and UI snapshot" OFF)

set(BOTHER_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(BOTHER_FORMATS VST3 Standalone)
endif()

juce_add_plugin(BOTHER
    COMPANY_NAME                "manners! DSP"
    BUNDLE_ID                   com.mannersdsp.bother
    PLUGIN_MANUFACTURER_CODE    Mnrs
    PLUGIN_CODE                 Bthr
    PRODUCT_NAME                "BOTHER"
    DESCRIPTION                 "Tremolo with a temper"
    FORMATS                     ${BOTHER_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Modulation Distortion)

# The manners! face: the worn card, its wear maps and the house font. Every plugin in the family ships these.
juce_add_binary_data(BOTHER_Assets SOURCES
    Resources/plate.jpg
    Resources/maps.png
    Resources/BarlowSemiCondensed-Medium.ttf)

set(MANNERS_SOURCES
    Source/manners/MannersFace.cpp
    Source/manners/MannersMenu.cpp
    Source/manners/MannersPresets.cpp)

target_sources(BOTHER PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${MANNERS_SOURCES})

target_compile_definitions(BOTHER PUBLIC
    BOTHER_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BOTHER
    PRIVATE
        BOTHER_Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(BOTHER_BUILD_TESTS)
    # Engine-only checks: no GUI, runs on the CI machine.
    juce_add_console_app(BOTHER_Test PRODUCT_NAME "BOTHER_Test")
    target_sources(BOTHER_Test PRIVATE Tests/OfflineTest.cpp)
    target_include_directories(BOTHER_Test PRIVATE Source)
    target_compile_definitions(BOTHER_Test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BOTHER_Test PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags)

    # Full plugin: renders a demo WAV and a PNG of the editor.
    juce_add_console_app(BOTHER_Tool PRODUCT_NAME "BOTHER_Tool")
    target_sources(BOTHER_Tool PRIVATE
        Tests/Tool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${MANNERS_SOURCES})
    target_include_directories(BOTHER_Tool PRIVATE Source)
    target_compile_definitions(BOTHER_Tool PRIVATE BOTHER_VERSION="${PROJECT_VERSION}" JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BOTHER_Tool PRIVATE
        BOTHER_Assets
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
