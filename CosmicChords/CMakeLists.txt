cmake_minimum_required(VERSION 3.22)

project(CosmicChords VERSION 1.1.0)

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

juce_add_binary_data(CosmicChordsData SOURCES Resources/library.json)

file(GLOB CHORD_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/*.cpp)
file(GLOB CHORD_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/*.cpp)
set(CHORD_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Plugin/PluginEditor.cpp)

juce_add_plugin(CosmicChords
    COMPANY_NAME            "MyPlugins"
    PLUGIN_MANUFACTURER_CODE Mypl
    PLUGIN_CODE             Cosc
    PRODUCT_NAME            "CosmicChords"
    PLUGIN_NAME             "Cosmic Chords"
    BUNDLE_ID               "com.myplugins.cosmicchords"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE            kAudioUnitType_MIDIProcessor
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS                 ${PLUGIN_FORMATS}
)

target_sources(CosmicChords PRIVATE
    ${CHORD_PLUGIN_SOURCES}
    ${CHORD_UI_SOURCES}
    ${CHORD_CORE_SOURCES}
)
target_include_directories(CosmicChords PRIVATE Source)

target_compile_definitions(CosmicChords PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(CosmicChords
    PRIVATE
        CosmicChordsData
        juce::juce_audio_utils
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Snapshot tool: renders the editor (and single widgets) to PNGs for headless visual checks.
juce_add_console_app(CosmicChordsSnapshot PRODUCT_NAME "CosmicChordsSnapshot")
target_sources(CosmicChordsSnapshot PRIVATE
    Tools/Snapshot.cpp
    ${CHORD_PLUGIN_SOURCES}
    ${CHORD_UI_SOURCES}
    ${CHORD_CORE_SOURCES}
)
target_include_directories(CosmicChordsSnapshot PRIVATE Source)
target_compile_definitions(CosmicChordsSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(CosmicChordsSnapshot
    PRIVATE
        CosmicChordsData
        juce::juce_audio_utils
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
if(APPLE)
    add_custom_command(TARGET CosmicChordsSnapshot POST_BUILD
        COMMAND codesign -s - -f $<TARGET_FILE:CosmicChordsSnapshot>)
endif()

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
        CosmicChordsData
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
