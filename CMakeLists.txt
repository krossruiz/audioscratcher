cmake_minimum_required(VERSION 3.22)

project(AudioScratcher VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(AudioScratcher
    PRODUCT_NAME "AudioScratcher"
    COMPANY_NAME "AudioScratcher"
)

target_sources(AudioScratcher PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/ScratchAudioEngine.cpp
    Source/MidiJogController.cpp
)

target_compile_definitions(AudioScratcher PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AudioScratcher,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AudioScratcher,JUCE_VERSION>"
)

target_link_libraries(AudioScratcher PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_events
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
