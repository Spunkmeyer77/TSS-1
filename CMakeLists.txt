cmake_minimum_required(VERSION 3.28)
# must come before project(): otherwise the macOS build targets the build machine's own macOS version
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(TSS1 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# ---- JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- eSpeak-NG (GPL-3.0): only the synthesis library is built; its voice data is prebuilt in
# assets/espeak-ng-data.zip (tools/build_espeak_data.sh) and embedded in the plug-in
set(USE_MBROLA OFF CACHE BOOL "" FORCE)
set(USE_LIBSONIC OFF CACHE BOOL "" FORCE)
set(USE_LIBPCAUDIO OFF CACHE BOOL "" FORCE)
set(USE_ASYNC OFF CACHE BOOL "" FORCE)
set(USE_SPEECHPLAYER OFF CACHE BOOL "" FORCE)
set(USE_KLATT ON CACHE BOOL "" FORCE)
set(ESPEAK_BUILD_MANPAGES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
if(DEFINED ESPEAK_DIR)
    add_subdirectory(${ESPEAK_DIR} espeak-ng EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(espeakng
        GIT_REPOSITORY https://github.com/espeak-ng/espeak-ng.git
        GIT_TAG 1.52.0
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(espeakng)
endif()
set_target_properties(espeak-ng ucd PROPERTIES POSITION_INDEPENDENT_CODE ON)
# link eSpeak without its include/compat folder, which replaces <wchar.h> and would break JUCE
get_target_property(ESPEAK_LIB_DIR espeak-ng SOURCE_DIR)
add_library(tss_espeak INTERFACE)
target_link_libraries(tss_espeak INTERFACE $<LINK_ONLY:espeak-ng>)
target_include_directories(tss_espeak INTERFACE "${ESPEAK_LIB_DIR}/../include")
target_compile_definitions(tss_espeak INTERFACE LIBESPEAK_NG_EXPORT=1)

# ---- the plug-in
if(APPLE)
    set(TSS_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(TSS_FORMATS VST3 Standalone)
else()
    set(TSS_FORMATS VST3 LV2 Standalone)
endif()

option(TSS_COPY_PLUGINS "Copy plug-ins to the plug-in folders after building" ON)

juce_add_plugin(TSS1
    COMPANY_NAME "Transistor Labs"
    COMPANY_WEBSITE "https://ticcoross.gumroad.com/"
    COMPANY_EMAIL "ticcoross@gmail.com"
    PLUGIN_MANUFACTURER_CODE Trlb
    PLUGIN_CODE Tss1
    FORMATS ${TSS_FORMATS}
    LV2URI "urn:transistorlabs:tss1"
    PRODUCT_NAME "TSS-1"
    BUNDLE_ID com.transistorlabs.tss1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${TSS_COPY_PLUGINS})

file(GLOB TSS_UI CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.jpg")
juce_add_binary_data(TSS1Data SOURCES ${TSS_UI} assets/espeak-ng-data.zip "docs/TSS-1 Manual.pdf")
set_target_properties(TSS1Data PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(TSS_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SpeechEngine.cpp)
target_sources(TSS1 PRIVATE ${TSS_SOURCES})

set(TSS_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 _USE_MATH_DEFINES)
target_compile_definitions(TSS1 PUBLIC ${TSS_DEFS})

target_link_libraries(TSS1
    PRIVATE
        TSS1Data
        tss_espeak
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- optional headless test app (rendering, notes, melodic pitch, state, editor snapshots)
option(TSS_BUILD_TESTS "Build the plug-in test app" OFF)
if(TSS_BUILD_TESTS)
    juce_add_console_app(TSS1Test PRODUCT_NAME "TSS1Test")
    target_sources(TSS1Test PRIVATE tools/plugin_test.cpp ${TSS_SOURCES})
    target_compile_definitions(TSS1Test PRIVATE ${TSS_DEFS} ${TSS_TEST_DEFS}
        JucePlugin_Name="TSS-1" JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(TSS1Test PRIVATE TSS1Data tss_espeak juce::juce_audio_utils juce::juce_gui_extra
                                           juce::juce_recommended_config_flags)
endif()
