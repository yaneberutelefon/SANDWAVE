cmake_minimum_required(VERSION 3.22)

# --- platform defaults (must be set before project()) -----------------------
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
# Static MSVC runtime: users don't need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SANDWAVE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SANDWAVE_COPY_AFTER_BUILD "Copy the VST3 into the system VST3 folder after build" OFF)
option(SANDWAVE_BUILD_STANDALONE "Also build the standalone app (handy for testing)" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(SANDWAVE_FORMATS VST3)
if(SANDWAVE_BUILD_STANDALONE)
    list(APPEND SANDWAVE_FORMATS Standalone)
endif()

juce_add_plugin(SANDWAVE
    COMPANY_NAME                "Neptun"
    PLUGIN_MANUFACTURER_CODE    Nptn
    PLUGIN_CODE                 Sndw
    FORMATS                     ${SANDWAVE_FORMATS}
    PRODUCT_NAME                "SANDWAVE"
    VST3_CATEGORIES             Fx
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${SANDWAVE_COPY_AFTER_BUILD})

target_sources(SANDWAVE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SANDWAVE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SANDWAVE
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(SANDWAVE)
