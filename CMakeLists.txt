cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) when building on a Mac — must be set before project()
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(NoviTape VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if one is given (-DNOVITAPE_JUCE_DIR=...), otherwise fetch JUCE 8.
if(NOVITAPE_JUCE_DIR)
    add_subdirectory(${NOVITAPE_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats: VST3 everywhere, plus AU on macOS. Pass -DNOVITAPE_STANDALONE=ON for a test app.
set(NT_FORMATS VST3)
if(APPLE)
    list(APPEND NT_FORMATS AU)
endif()
if(NOVITAPE_STANDALONE)
    list(APPEND NT_FORMATS Standalone)
endif()

juce_add_plugin(NoviTape
    COMPANY_NAME "Amir Novin"
    BUNDLE_ID "com.amirnovin.novitape"
    PRODUCT_NAME "NoviTape"
    PLUGIN_MANUFACTURER_CODE Anov
    PLUGIN_CODE NovT
    FORMATS ${NT_FORMATS}
    # An audio effect for mixer inserts. It also listens to MIDI notes 60-67
    # (C5-G5 in FL Studio) to trigger the eight effects.
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

juce_add_binary_data(NoviTapeAssets SOURCES
    Resources/fonts/BarlowCondensed-Bold.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/ReenieBeanie.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(NoviTape PRIVATE
    Source/TapeEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(NoviTape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(NoviTape PRIVATE /utf-8)
endif()

target_link_libraries(NoviTape
    PRIVATE
        NoviTapeAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine unit test (no JUCE needed): cmake --build build --target EngineTest && ./build/EngineTest
add_executable(EngineTest Tests/EngineTest.cpp Source/TapeEngine.cpp)
