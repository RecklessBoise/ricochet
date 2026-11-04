cmake_minimum_required(VERSION 3.22)

project(Ricochet VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(RICOCHET_COPY_PLUGINS "Copy plugins to the system plug-in folders after build" ON)
option(RICOCHET_BUILD_TESTS "Build the unit test runner" ON)

add_subdirectory(JUCE)

set(RICOCHET_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RICOCHET_FORMATS AU)
endif()

juce_add_plugin(Ricochet
    COMPANY_NAME "RecklessBoise"
    COMPANY_WEBSITE "https://github.com/RecklessBoise/ricochet"
    BUNDLE_ID "com.recklessboise.ricochet"
    PLUGIN_MANUFACTURER_CODE Rckb
    PLUGIN_CODE Rcht
    FORMATS ${RICOCHET_FORMATS}
    PRODUCT_NAME "Reckless Ricochet"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${RICOCHET_COPY_PLUGINS})

set(RICOCHET_CORE_SOURCES
    Source/Core/Scene.cpp
    Source/Core/Physics.cpp
    Source/Core/Presets.cpp
    Source/Core/Randomizer.cpp
    Source/Core/RenderEngine.cpp
    Source/Core/OfflineRenderer.cpp
    Source/Synth/Synth.cpp
    Source/Synth/Effects.cpp)

set(RICOCHET_UI_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Recorder.cpp
    Source/UI/Theme.cpp
    Source/UI/SceneCanvas.cpp
    Source/UI/Inspector.cpp
    Source/UI/TopBar.cpp
    Source/UI/RecordingsPanel.cpp)

target_sources(Ricochet PRIVATE ${RICOCHET_CORE_SOURCES} ${RICOCHET_UI_SOURCES})
target_include_directories(Ricochet PRIVATE Source)

target_compile_definitions(Ricochet PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Ricochet
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(RICOCHET_BUILD_TESTS)
    juce_add_console_app(RicochetTests PRODUCT_NAME "RicochetTests")
    target_sources(RicochetTests PRIVATE Tests/TestMain.cpp ${RICOCHET_CORE_SOURCES})
    target_include_directories(RicochetTests PRIVATE Source)
    target_compile_definitions(RicochetTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_UNIT_TESTS=1)
    target_link_libraries(RicochetTests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_dsp
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME RicochetTests COMMAND RicochetTests)
endif()

option(RICOCHET_BUILD_SNAPSHOT "Build the offscreen UI snapshot tool" ON)
if(RICOCHET_BUILD_SNAPSHOT)
    juce_add_console_app(RicochetSnapshot PRODUCT_NAME "RicochetSnapshot")
    target_sources(RicochetSnapshot PRIVATE Tools/Snapshot.cpp ${RICOCHET_CORE_SOURCES} ${RICOCHET_UI_SOURCES})
    target_include_directories(RicochetSnapshot PRIVATE Source)
    target_compile_definitions(RicochetSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(RicochetSnapshot
        PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
