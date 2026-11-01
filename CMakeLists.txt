cmake_minimum_required(VERSION 3.22)
project(VantaBeta VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Pinned, statically linked decoders: no external DLLs in the Windows installer.
FetchContent_Declare(vanta_zlib
    URL https://codeload.github.com/madler/zlib/tar.gz/refs/tags/v1.3.1
    URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vanta_zlib)
FetchContent_Declare(vanta_zstd
    URL https://codeload.github.com/facebook/zstd/tar.gz/refs/tags/v1.5.6
    URL_HASH SHA256=30f35f71c1203369dc979ecde0400ffea93c27391bfd2ac5a9715d2173d92ff7
    SOURCE_SUBDIR build/cmake)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vanta_zstd)
# Static decoders are linked into the VST3 shared library on every platform.
set_target_properties(zlibstatic libzstd_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(VantaBeta
    COMPANY_NAME "Vanta"
    BUNDLE_ID "com.vanta.beta"
    PLUGIN_MANUFACTURER_CODE Vnta
    PLUGIN_CODE Vntb
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vanta Beta 0.10"
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
    Source/PresetImporter.cpp
    Source/SynthEngine.cpp)
target_include_directories(VantaBeta PRIVATE Source)
target_compile_definitions(VantaBeta PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VantaBeta PRIVATE juce::juce_audio_utils juce::juce_dsp zlibstatic libzstd_static
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(VANTA_BUILD_TESTS "Build VANTA's engine, preset and editor tests" ON)
if(VANTA_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(VantaTests PRODUCT_NAME "VantaTests")
    target_sources(VantaTests PRIVATE tests/VantaTests.cpp Source/SynthEngine.cpp Source/PresetCodec.cpp Source/PresetImporter.cpp)
    target_include_directories(VantaTests PRIVATE Source)
    target_compile_definitions(VantaTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(VantaTests PRIVATE juce::juce_audio_basics juce::juce_core zlibstatic libzstd_static)
    add_test(NAME VantaTests COMMAND VantaTests)

    juce_add_console_app(VantaImporterTests PRODUCT_NAME "VantaImporterTests")
    target_sources(VantaImporterTests PRIVATE tests/PresetImporterTests.cpp Source/SynthEngine.cpp Source/PresetCodec.cpp Source/PresetImporter.cpp)
    target_include_directories(VantaImporterTests PRIVATE Source)
    target_compile_definitions(VantaImporterTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(VantaImporterTests PRIVATE juce::juce_audio_basics juce::juce_core zlibstatic libzstd_static)
    add_test(NAME VantaImporterTests COMMAND VantaImporterTests)

    # Exercise the actual editor and processor in both wrapper modes, render
    # previews, and check that the controls remain connected to parameters.
    juce_add_console_app(VantaEditorTests PRODUCT_NAME "VantaEditorTests")
    juce_generate_juce_header(VantaEditorTests)
    target_sources(VantaEditorTests PRIVATE tests/VantaEditorTests.cpp
        Source/Plugin.cpp Source/PluginEditor.cpp Source/PresetCodec.cpp Source/PresetImporter.cpp Source/SynthEngine.cpp)
    target_include_directories(VantaEditorTests PRIVATE Source)
    target_compile_definitions(VantaEditorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(VantaEditorTests PRIVATE juce::juce_audio_utils juce::juce_dsp zlibstatic libzstd_static)
    add_test(NAME VantaEditorTests COMMAND VantaEditorTests "${CMAKE_BINARY_DIR}/ui-preview")

    # Exercise the actual VST3 view API with a small host/frame, not just the
    # editor component. Windows is the shipping platform. Other platforms can
    # opt in when their VST3 bundle and GUI dependencies are available.
    option(VANTA_TEST_VST3_WRAPPER "Test the built VST3 wrapper as a host" ${WIN32})
    if(VANTA_TEST_VST3_WRAPPER)
        add_executable(VantaVST3Tests tests/VantaVST3Tests.cpp)
        target_include_directories(VantaVST3Tests PRIVATE "${JUCE_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK")
        if(CMAKE_DL_LIBS)
            target_link_libraries(VantaVST3Tests PRIVATE ${CMAKE_DL_LIBS})
        endif()
        add_dependencies(VantaVST3Tests VantaBeta_VST3)
        add_test(NAME VantaVST3Tests COMMAND VantaVST3Tests "$<TARGET_FILE:VantaBeta_VST3>")
    endif()
endif()
