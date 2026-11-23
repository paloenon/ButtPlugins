cmake_minimum_required(VERSION 3.22)

project(ChordButt VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

enable_testing()
include(Catch)

if(APPLE)
    set(PLUGIN_FORMATS AU Standalone)
else()
    set(PLUGIN_FORMATS Standalone)
endif()

juce_add_binary_data(ChordButtData SOURCES Resources/library.json)

file(GLOB CHORD_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.cpp)

juce_add_plugin(ChordButt
    COMPANY_NAME            "MyPlugins"
    PLUGIN_MANUFACTURER_CODE Mypl
    PLUGIN_CODE             Chbt
    PRODUCT_NAME            "ChordButt"
    BUNDLE_ID               "com.myplugins.chordbutt"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE            kAudioUnitType_MIDIProcessor
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS                 ${PLUGIN_FORMATS}
)

target_sources(ChordButt PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    ${CHORD_CORE_SOURCES}
)
target_include_directories(ChordButt PRIVATE Source)

target_compile_definitions(ChordButt PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ChordButt
    PRIVATE
        ChordButtData
        juce::juce_audio_utils
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Unit tests (core logic only, no GUI)
file(GLOB CHORD_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp)

juce_add_console_app(ChordCoreTests PRODUCT_NAME "ChordCoreTests")
target_sources(ChordCoreTests PRIVATE ${CHORD_CORE_SOURCES} ${CHORD_TEST_SOURCES})
target_include_directories(ChordCoreTests PRIVATE Source)
target_compile_definitions(ChordCoreTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(ChordCoreTests
    PRIVATE
        ChordButtData
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_data_structures
        Catch2::Catch2WithMain
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
if(APPLE)
    # Unsigned arm64 binaries are killed by macOS; ad-hoc sign before test discovery runs.
    add_custom_command(TARGET ChordCoreTests POST_BUILD
        COMMAND codesign -s - -f $<TARGET_FILE:ChordCoreTests>)
endif()
catch_discover_tests(ChordCoreTests)
