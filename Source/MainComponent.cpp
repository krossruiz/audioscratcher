#include "MainComponent.h"

MainComponent::MainComponent()
    : audioSettings(deviceManager,
                     1, 2,   // min/max input channels
                     0, 2,   // min/max output channels
                     false,  // no MIDI input list from this component (we handle MIDI ourselves)
                     false,
                     false,
                     false),
      scratchEngine(midiController)
{
    setSize(760, 640);

    // --- Audio device settings panel ---
    addAndMakeVisible(audioSettings);

    deviceManager.initialiseWithDefaultDevices(2, 2);
    deviceManager.addAudioCallback(&scratchEngine);

    // --- MIDI device selection ---
    addAndMakeVisible(midiDeviceBox);
    midiDeviceBox.onChange = [this]
    {
        auto text = midiDeviceBox.getText();
        if (text.isNotEmpty())
            midiController.openDevice(text);
    };
    refreshMidiDeviceList();

    addAndMakeVisible(refreshMidiButton);
    refreshMidiButton.onClick = [this] { refreshMidiDeviceList(); };

    addAndMakeVisible(learnJogButton);
    learnJogButton.onClick = [this]
    {
        midiController.beginLearning(MidiJogController::LearnTarget::jogWheel);
        mappingLabel.setText("Move the jog wheel now...", juce::dontSendNotification);
    };

    addAndMakeVisible(learnTouchButton);
    learnTouchButton.onClick = [this]
    {
        midiController.beginLearning(MidiJogController::LearnTarget::touchButton);
        mappingLabel.setText("Touch/press the platter now...", juce::dontSendNotification);
    };

    midiController.onMappingLearned = [this](juce::String summary)
    {
        mappingLabel.setText(summary, juce::dontSendNotification);
    };

    addAndMakeVisible(mappingLabel);
    mappingLabel.setText(midiController.getMappingSummary(), juce::dontSendNotification);
    mappingLabel.setJustificationType(juce::Justification::centredLeft);

    // --- Record control ---
    addAndMakeVisible(recordButton);
    recordButton.onClick = [this]
    {
        const bool nowRecording = ! scratchEngine.isRecording();
        scratchEngine.setRecording(nowRecording);
        recordButton.setButtonText(nowRecording ? "Stop Recording" : "Start Recording");
        recordButton.setColour(juce::TextButton::buttonColourId,
                                nowRecording ? juce::Colours::red.darker() : juce::Colours::darkgrey);
    };

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centredLeft);

    // --- Mix / feel controls ---
    auto setupSlider = [this](juce::Slider& s, juce::Label& l, double min, double max, double def)
    {
        addAndMakeVisible(s);
        s.setRange(min, max);
        s.setValue(def, juce::dontSendNotification);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        addAndMakeVisible(l);
        l.attachToComponent(&s, true);
    };

    setupSlider(monitorGainSlider, monitorGainLabel, 0.0, 1.5, 0.8);
    monitorGainSlider.onValueChange = [this] { scratchEngine.setMonitorGain((float) monitorGainSlider.getValue()); };

    setupSlider(scratchGainSlider, scratchGainLabel, 0.0, 1.5, 1.0);
    scratchGainSlider.onValueChange = [this] { scratchEngine.setScratchGain((float) scratchGainSlider.getValue()); };

    setupSlider(sensitivitySlider, sensitivityLabel, 5.0, 200.0, 45.0);
    sensitivitySlider.onValueChange = [this] { scratchEngine.setSensitivity((float) sensitivitySlider.getValue()); };

    // --- Export ---
    addAndMakeVisible(exportButton);
    exportButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>("Export recording as WAV",
                                                            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                            "*.wav");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File{})
                {
                    const bool ok = scratchEngine.writeRecordingToWavFile(file);
                    statusLabel.setText(ok ? "Exported to " + file.getFullPathName() : "Export failed",
                                         juce::dontSendNotification);
                }
            });
    };

    startTimerHz(4);
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(&scratchEngine);
}

void MainComponent::refreshMidiDeviceList()
{
    const auto currentSelection = midiDeviceBox.getText();
    midiDeviceBox.clear(juce::dontSendNotification);
    auto devices = midiController.getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
        midiDeviceBox.addItem(devices[i], i + 1);

    if (devices.contains(currentSelection))
        midiDeviceBox.setText(currentSelection, juce::dontSendNotification);
    else if (devices.size() > 0)
        midiDeviceBox.setSelectedItemIndex(0);
}

void MainComponent::timerCallback()
{
    juce::String status;
    status << (scratchEngine.isRecording() ? "Recording. " : "Not recording. ")
           << (midiController.isTouched() ? "Jog TOUCHED" : "Jog released")
           << "   |  " << midiController.getMappingSummary();
    statusLabel.setText(status, juce::dontSendNotification);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto midiRow = area.removeFromTop(30);
    midiDeviceBox.setBounds(midiRow.removeFromLeft(260));
    midiRow.removeFromLeft(6);
    refreshMidiButton.setBounds(midiRow.removeFromLeft(160));

    area.removeFromTop(6);
    auto learnRow = area.removeFromTop(30);
    learnJogButton.setBounds(learnRow.removeFromLeft(160));
    learnRow.removeFromLeft(6);
    learnTouchButton.setBounds(learnRow.removeFromLeft(160));

    area.removeFromTop(6);
    mappingLabel.setBounds(area.removeFromTop(24));

    area.removeFromTop(10);
    recordButton.setBounds(area.removeFromTop(36).removeFromLeft(220));

    area.removeFromTop(6);
    statusLabel.setBounds(area.removeFromTop(24));

    area.removeFromTop(10);
    auto sliderArea = area.removeFromTop(110);
    sliderArea.removeFromLeft(140); // room for attached labels
    monitorGainSlider.setBounds(sliderArea.removeFromTop(30));
    sliderArea.removeFromTop(6);
    scratchGainSlider.setBounds(sliderArea.removeFromTop(30));
    sliderArea.removeFromTop(6);
    sensitivitySlider.setBounds(sliderArea.removeFromTop(30));

    area.removeFromTop(10);
    exportButton.setBounds(area.removeFromTop(30).removeFromLeft(240));

    area.removeFromTop(10);
    audioSettings.setBounds(area);
}
