cmake_minimum_required(VERSION 3.22)
project(Fenix VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FENIX_LTO "Link-time optimisation (slow links; turn off for development)" ON)
option(FENIX_COPY_AFTER_BUILD "Install built plugins into ~/.vst3 and ~/.lv2" ON)

set(JUCE_DIR "/LEVIATHAN/FROM-WINDOWS/Documents/JUCE" CACHE PATH "Path to a JUCE 8 checkout")
add_subdirectory(${JUCE_DIR} JUCE)

# VST3 everywhere; AU for Logic/GarageBand on macOS; LV2 on Linux.
if(APPLE)
    set(FENIX_FORMATS VST3 AU)
elseif(WIN32)
    set(FENIX_FORMATS VST3)
else()
    set(FENIX_FORMATS VST3 LV2)
endif()

# Panel fonts, compiled into every plugin so legends look the same on every platform.
juce_add_binary_data(RackFonts
    HEADER_NAME "RackFonts.h"
    NAMESPACE RackFonts
    SOURCES common/fonts/BarlowCondensed-SemiBold.ttf common/fonts/BarlowCondensed-Bold.ttf
            common/fonts/Barlow-Regular.ttf common/fonts/Barlow-Medium.ttf
            common/fonts/JetBrainsMono-Medium.ttf common/fonts/JetBrainsMono-Bold.ttf)
set_target_properties(RackFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(fenix_plugin target name code)
    juce_add_plugin(${target}
        COMPANY_NAME fenrikos
        PLUGIN_MANUFACTURER_CODE Fnrk
        PLUGIN_CODE ${code}
        PRODUCT_NAME "Fenix ${name}"
        FORMATS ${FENIX_FORMATS}
        BUNDLE_ID "com.fenrikos.fenix.${target}"
        HARDENED_RUNTIME_ENABLED TRUE
        LV2URI "https://github.com/FenrirRhogar/fenix/${target}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx
        COPY_PLUGIN_AFTER_BUILD ${FENIX_COPY_AFTER_BUILD})

    target_sources(${target} PRIVATE plugins/${target}.cpp)
    target_include_directories(${target} PRIVATE common)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1                       # update check (UpdateCheck.h); on Linux libcurl is
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1       # loaded at run time, so it is not a hard dependency
        FENRIKOS_UPDATE_REPO="fenix"
        FENRIKOS_UPDATE_NAME="Fenix"
        FENRIKOS_PRODUCT_URL="https://fenrikos.gumroad.com/l/fenix"
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC RackFonts
               juce::juce_recommended_config_flags
               juce::juce_recommended_warning_flags)
    if(FENIX_LTO)
        target_link_libraries(${target} PUBLIC juce::juce_recommended_lto_flags)
    endif()
endfunction()

fenix_plugin(Drive     "Drive"     FxDr)  # harmonic saturation
fenix_plugin(Shelf     "Shelf EQ"  FxSh)  # broad two-band shelving EQ
fenix_plugin(HFComp    "HF Comp"   FxHc)  # high-band-only compressor
fenix_plugin(Width     "Width"     FxWd)  # M/S width + low-band spread
fenix_plugin(Iron      "Iron"      FxIr)  # output transformer colour
fenix_plugin(Bus       "Bus"       FxBs)  # all five stages in one chain

# Optional: one <Plugin>_snapshot executable per plugin that renders its editor to PNG.
option(FENIX_SNAPSHOTS "Build editor snapshot tools" OFF)
if(FENIX_SNAPSHOTS)
    foreach(plugin Drive Shelf HFComp Width Iron Bus)
        add_executable(${plugin}_snapshot tools/Snapshot.cpp)
        target_include_directories(${plugin}_snapshot PRIVATE $<TARGET_PROPERTY:${plugin},INCLUDE_DIRECTORIES>)
        target_compile_definitions(${plugin}_snapshot PRIVATE $<TARGET_PROPERTY:${plugin},COMPILE_DEFINITIONS>)
        target_link_libraries(${plugin}_snapshot PRIVATE ${plugin})
    endforeach()
endif()
