cmake_minimum_required(VERSION 3.22)
project(VantaBeta VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VantaBeta
    COMPANY_NAME "Vanta"
    BUNDLE_ID "com.vanta.beta"
    PLUGIN_MANUFACTURER_CODE Vnta
    PLUGIN_CODE Vntb
    FORMATS VST3
    PRODUCT_NAME "Vanta Beta 0.4"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(VantaBeta)
target_sources(VantaBeta PRIVATE
    Source/Plugin.cpp
    Source/PluginEditor.cpp
    Source/PresetCodec.cpp
    Source/SynthEngine.cpp)
target_include_directories(VantaBeta PRIVATE Source)
target_compile_definitions(VantaBeta PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VantaBeta PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(VANTA_BUILD_TESTS "Build VANTA's engine and preset tests" ON)
if(VANTA_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(VantaTests PRODUCT_NAME "VantaTests")
    target_sources(VantaTests PRIVATE tests/VantaTests.cpp Source/SynthEngine.cpp Source/PresetCodec.cpp)
    target_include_directories(VantaTests PRIVATE Source)
    target_compile_definitions(VantaTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(VantaTests PRIVATE juce::juce_audio_basics juce::juce_core)
    add_test(NAME VantaTests COMMAND VantaTests)
endif()
