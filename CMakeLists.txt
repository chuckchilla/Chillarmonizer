cmake_minimum_required(VERSION 3.22)
project(Chillarmonizer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Offline DSP tests (no JUCE): tools/selftest.cpp
add_executable(chillarmonizer_selftest tools/selftest.cpp)
target_include_directories(chillarmonizer_selftest PRIVATE Source)

option(CHILLARMONIZER_TESTS_ONLY "Only build the DSP self-test" OFF)
if(CHILLARMONIZER_TESTS_ONLY)
    return()
endif()

# Use a local JUCE checkout if given (-DJUCE_DIR=...), otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FORMATS VST3)
if(APPLE)
    list(APPEND FORMATS AU)
endif()

juce_add_plugin(Chillarmonizer
    COMPANY_NAME "CHILLplugins"
    PLUGIN_MANUFACTURER_CODE Chla
    PLUGIN_CODE Chrm
    FORMATS ${FORMATS}
    PRODUCT_NAME "Chillarmonizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Chillarmonizer)

target_sources(Chillarmonizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Chillarmonizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Chillarmonizer
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
