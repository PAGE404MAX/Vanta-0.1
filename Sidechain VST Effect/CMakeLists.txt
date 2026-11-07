cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(VantaSidechain VERSION 0.1.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(SIDECHAIN_BUILD_TESTS "Build VANTA Sidechain tests" ON)
if(SIDECHAIN_BUILD_TESTS)
    enable_testing()
endif()

juce_add_plugin(VantaSidechainFx
    VERSION 0.1.0
    COMPANY_NAME "Vanta"
    BUNDLE_ID "com.vanta.sidechain"
    PLUGIN_MANUFACTURER_CODE Vnta
    PLUGIN_CODE Vsdc
    FORMATS VST3
    PRODUCT_NAME "VANTA Sidechain"
    VST3_CATEGORIES Fx Dynamics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(VantaSidechainFx)
target_sources(VantaSidechainFx PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SidechainEngine.cpp)
target_include_directories(VantaSidechainFx PRIVATE Source)
target_compile_definitions(VantaSidechainFx PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VantaSidechainFx PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(SIDECHAIN_BUILD_TESTS)
    add_executable(SidechainEngineTests tests/EngineTests.cpp Source/SidechainEngine.cpp)
    target_include_directories(SidechainEngineTests PRIVATE Source)
    add_test(NAME SidechainEngineTests COMMAND SidechainEngineTests)

    juce_add_console_app(SidechainProcessorTests PRODUCT_NAME "SidechainProcessorTests")
    juce_generate_juce_header(SidechainProcessorTests)
    target_sources(SidechainProcessorTests PRIVATE tests/ProcessorTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SidechainEngine.cpp)
    target_include_directories(SidechainProcessorTests PRIVATE Source)
    target_compile_definitions(SidechainProcessorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(SidechainProcessorTests PRIVATE juce::juce_audio_utils)
    add_test(NAME SidechainProcessorTests COMMAND SidechainProcessorTests "${CMAKE_BINARY_DIR}/sidechain-preview")
    add_executable(SidechainVST3Tests tests/VST3Tests.cpp)
    target_include_directories(SidechainVST3Tests PRIVATE "${juce_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK")
    if(UNIX AND NOT APPLE)
        target_link_libraries(SidechainVST3Tests PRIVATE dl)
    endif()
    add_dependencies(SidechainVST3Tests VantaSidechainFx_VST3)
    add_test(NAME SidechainVST3Tests COMMAND SidechainVST3Tests $<TARGET_FILE:VantaSidechainFx_VST3>)
endif()
