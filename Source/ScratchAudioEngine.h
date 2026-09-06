#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "MidiJogController.h"

// Continuously records the live audio input into a rolling ring buffer.
// While the jog wheel is touched, played-back position is driven directly
// by MIDI jog deltas (turntable-style scratch of the just-recorded audio).
// When released, playback free-runs forward from wherever the scratch left
// off, like a CDJ platter that keeps spinning.
class ScratchAudioEngine : public juce::AudioIODeviceCallback
{
public:
    explicit ScratchAudioEngine(MidiJogController& midiController);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

    void setRecording(bool shouldRecord) { recording.store(shouldRecord); }
    bool isRecording() const { return recording.load(); }

    void setMonitorGain(float g) { monitorGain.store(g); }
    void setScratchGain(float g) { scratchGain.store(g); }
    void setSensitivity(float ticksToSamplesPerBlockScale) { sensitivity.store(ticksToSamplesPerBlockScale); }

    // Export the current ring-buffer contents (what's been recorded so far).
    bool writeRecordingToWavFile(const juce::File& file);

    double getBufferLengthSeconds() const { return bufferLengthSeconds; }
    double getSampleRate() const { return currentSampleRate.load(); }

private:
    MidiJogController& midi;

    static constexpr double bufferLengthSeconds = 120.0;

    juce::AudioBuffer<float> ringBuffer;
    int ringBufferNumChannels = 2;
    std::atomic<int64_t> writePosition { 0 };   // absolute sample count written so far
    juce::CriticalSection ringBufferLock;

    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<bool> recording { false };

    std::atomic<double> scratchPosition { 0.0 }; // absolute sample index into the recorded stream
    std::atomic<float> monitorGain { 0.8f };
    std::atomic<float> scratchGain { 1.0f };
    std::atomic<float> sensitivity { 45.0f }; // samples of scrub per jog "tick"

    void ensureRingBufferAllocated(double sampleRate, int numChannels);
    float readInterpolatedSample(int channel, double absoluteSamplePos) const;
};
