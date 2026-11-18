cmake_minimum_required(VERSION 3.22)
project(SwellGuimik VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SwellGuimik
    COMPANY_NAME "Swell Audio"
    PLUGIN_MANUFACTURER_CODE Swel
    PLUGIN_CODE Gmk1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Swell Guimik"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator)

file(GLOB GUITAR_SAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/guitar/*.flac")
juce_add_binary_data(SwellGuimikAssets SOURCES Assets/swell-mark.png Assets/BebasNeue-Regular.ttf ${GUITAR_SAMPLES})

target_sources(SwellGuimik PRIVATE
    Source/GuitarSampler.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SwellGuimik PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(SwellGuimik PRIVATE /utf-8)
endif()

target_link_libraries(SwellGuimik
    PRIVATE
        SwellGuimikAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)

# Test hors-ligne du moteur (sans JUCE)
add_executable(engine_test Tests/engine_test.cpp)
target_include_directories(engine_test PRIVATE Source)
if(MSVC)
    target_compile_options(engine_test PRIVATE /utf-8)
    target_compile_definitions(engine_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
