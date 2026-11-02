cmake_minimum_required(VERSION 3.22)
# This directory can be built alone, or as a subproject of VANTA.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Laggify VERSION 0.1.0 LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(LAGGIFY_BUILD_TESTS "Build Laggify tests" ON)
if(LAGGIFY_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
endif()

juce_add_plugin(LaggifyFx
    COMPANY_NAME "Vanta"
    BUNDLE_ID "com.vanta.laggify"
    PLUGIN_MANUFACTURER_CODE Vnta
    PLUGIN_CODE Lggy
    FORMATS VST3
    PRODUCT_NAME "Laggify"
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(LaggifyFx)
target_sources(LaggifyFx PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/TimeVolumeEngine.cpp)
target_include_directories(LaggifyFx PRIVATE Source)
target_compile_definitions(LaggifyFx PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LaggifyFx PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(LAGGIFY_BUILD_TESTS)
    add_executable(LaggifyEngineTests tests/EngineTests.cpp Source/TimeVolumeEngine.cpp)
    target_include_directories(LaggifyEngineTests PRIVATE Source)
    add_test(NAME LaggifyEngineTests COMMAND LaggifyEngineTests)

    juce_add_console_app(LaggifyProcessorTests PRODUCT_NAME "LaggifyProcessorTests")
    juce_generate_juce_header(LaggifyProcessorTests)
    target_sources(LaggifyProcessorTests PRIVATE tests/ProcessorTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/TimeVolumeEngine.cpp)
    target_include_directories(LaggifyProcessorTests PRIVATE Source)
    target_compile_definitions(LaggifyProcessorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(LaggifyProcessorTests PRIVATE juce::juce_audio_utils)
    add_test(NAME LaggifyProcessorTests COMMAND LaggifyProcessorTests "${CMAKE_BINARY_DIR}/laggify-preview")

    # Load the actual compiled module in a minimal VST3 host, including real
    # incoming audio, automation, state, DPI and resize negotiation.
    add_executable(LaggifyVST3Tests tests/VST3Tests.cpp)
    target_include_directories(LaggifyVST3Tests PRIVATE "${juce_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK")
    if(UNIX AND NOT APPLE)
        target_link_libraries(LaggifyVST3Tests PRIVATE dl)
    endif()
    add_test(NAME LaggifyVST3Tests COMMAND LaggifyVST3Tests $<TARGET_FILE:LaggifyFx_VST3>)
endif()
