cmake_minimum_required(VERSION 3.22)
project(Fenix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "/LEVIATHAN/FROM-WINDOWS/Documents/JUCE" CACHE PATH "Path to a JUCE 8 checkout")
add_subdirectory(${JUCE_DIR} JUCE)

function(fenix_plugin target name code)
    juce_add_plugin(${target}
        COMPANY_NAME fenrikos
        PLUGIN_MANUFACTURER_CODE Fnrk
        PLUGIN_CODE ${code}
        PRODUCT_NAME "Fenix ${name}"
        FORMATS VST3 LV2
        LV2URI "https://github.com/FenrirRhogar/fenix/${target}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx
        COPY_PLUGIN_AFTER_BUILD TRUE)

    target_sources(${target} PRIVATE plugins/${target}.cpp)
    target_include_directories(${target} PRIVATE common)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags
               juce::juce_recommended_lto_flags
               juce::juce_recommended_warning_flags)
endfunction()

fenix_plugin(Drive     "Drive"     FxDr)  # harmonic saturation
