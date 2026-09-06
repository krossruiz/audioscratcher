#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <atomic>

// Listens to a MIDI input device, learns which CC drives the jog wheel
// (most DJ controllers send it as a relative encoder: values centred on 64,
// >64 = forward, <64 = backward) and which Note number is the jog "touch"
// sensor, then exposes a continuous scratch delta + touch state that the
// audio engine polls each block.
class MidiJogController : public juce::MidiInputCallback
{
public:
    enum class LearnTarget { none, jogWheel, touchButton };

    MidiJogController();
    ~MidiJogController() override;

    juce::StringArray getAvailableDevices() const;
    bool openDevice(const juce::String& deviceIdentifier);
    void closeDevice();
    juce::String getOpenDeviceName() const { return openDeviceName; }

    // Call, then move the jog wheel / press its touch sensor on the controller.
    // The next matching MIDI messages are captured as the mapping.
    void beginLearning(LearnTarget target);
    void cancelLearning();
    bool isLearning() const { return learnTarget != LearnTarget::none; }
    juce::String getMappingSummary() const;

    // Polled by the audio thread (lock-free): consumes accumulated relative
    // motion since the last call. Positive = forward, negative = backward.
    double consumeJogDelta();
    bool isTouched() const { return touched.load(std::memory_order_relaxed); }

    // Manual mapping for controllers whose CC numbers are already known.
    void setJogWheelCC(int channel, int ccNumber);
    void setTouchNote(int channel, int noteNumber);

    std::function<void(juce::String)> onMappingLearned;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void processLearning(const juce::MidiMessage& message);
    void processMapped(const juce::MidiMessage& message);

    std::unique_ptr<juce::MidiInput> midiInput;
    juce::String openDeviceName;

    std::atomic<LearnTarget> learnTarget { LearnTarget::none };

    std::atomic<int> jogChannel { -1 };
    std::atomic<int> jogCC { -1 };
    std::atomic<int> touchChannel { -1 };
    std::atomic<int> touchNote { -1 };

    std::atomic<double> pendingJogDelta { 0.0 };
    std::atomic<bool> touched { false };

    // Fallback: if the jog CC turns out to send absolute (not relative)
    // values, we track the last value and diff it ourselves.
    std::atomic<int> lastAbsoluteJogValue { -1 };
    std::atomic<bool> jogIsRelativeEncoder { true };
};
