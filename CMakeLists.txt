cmake_minimum_required(VERSION 3.22)
project(niebla VERSION 1.0.0)

# JUCE is expected in ./juce (the GitHub workflow clones it there)
add_subdirectory(juce)

juce_add_plugin(niebla
    COMPANY_NAME "ZOONIDO"
    PRODUCT_NAME "NIEBLA"
    BUNDLE_ID com.zoonido.niebla
    PLUGIN_MANUFACTURER_CODE Zoon
    PLUGIN_CODE Nbla
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    FORMATS VST3 AU
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(niebla)

target_sources(niebla PRIVATE
    source/pluginprocessor.cpp
    source/plugineditor.cpp)

target_compile_definitions(niebla PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(niebla
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
