cmake_minimum_required(VERSION 3.22)

project(BomBapColor VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Descarga JUCE automaticamente (no hay que instalar nada a mano)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(JUCE)

# Formatos: VST3 en Windows y Mac, AU solo en Mac (Logic / GarageBand / Ableton Mac)
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(BomBapColor
    COMPANY_NAME "BomBapLab"
    PLUGIN_MANUFACTURER_CODE Bbmf
    PLUGIN_CODE Bbc1
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "BomBap Color"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(BomBapColor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/TapeEngine.h
)

target_compile_definitions(BomBapColor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(BomBapColor
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
