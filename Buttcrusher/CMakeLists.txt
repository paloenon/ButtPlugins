cmake_minimum_required(VERSION 3.22)

project(Buttcrusher VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build a universal binary (Apple Silicon + Intel) on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# Download JUCE automatically the first time you configure
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS AU VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Buttcrusher
    COMPANY_NAME            "MyPlugins"
    PLUGIN_MANUFACTURER_CODE Mypl          # 4 chars, at least one uppercase
    PLUGIN_CODE             Bcrs           # 4 chars, exactly one uppercase
    PRODUCT_NAME            "Buttcrusher"
    BUNDLE_ID               "com.myplugins.buttcrusher"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE            kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE           # installs into ~/Library/Audio/Plug-Ins
    FORMATS                 ${PLUGIN_FORMATS}
)

target_sources(Buttcrusher PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Buttcrusher PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Buttcrusher
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

