#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiJogController.h"
#include "ScratchAudioEngine.h"

class MainComponent : public juce::Component,
                       private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshMidiDeviceList();
    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceSelectorComponent audioSettings;

    MidiJogController midiController;
    ScratchAudioEngine scratchEngine;

    juce::ComboBox midiDeviceBox;
    juce::TextButton refreshMidiButton { "Refresh MIDI Devices" };
    juce::TextButton learnJogButton { "Learn Jog Wheel" };
    juce::TextButton learnTouchButton { "Learn Touch Button" };
    juce::Label mappingLabel;

    juce::TextButton recordButton { "Start Recording" };
    juce::Label statusLabel;

    juce::Slider monitorGainSlider;
    juce::Label monitorGainLabel { {}, "Live Monitor" };
    juce::Slider scratchGainSlider;
    juce::Label scratchGainLabel { {}, "Scratch Output" };
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel { {}, "Scratch Sensitivity" };

    juce::TextButton exportButton { "Export Recording As WAV..." };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
