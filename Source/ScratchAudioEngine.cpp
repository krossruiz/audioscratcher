#include "ScratchAudioEngine.h"

ScratchAudioEngine::ScratchAudioEngine(MidiJogController& midiController)
    : midi(midiController)
{
}

void ScratchAudioEngine::ensureRingBufferAllocated(double sampleRate, int numChannels)
{
    const juce::ScopedLock sl(ringBufferLock);

    const int neededSamples = (int) std::ceil(bufferLengthSeconds * sampleRate);
    if (ringBuffer.getNumSamples() != neededSamples || ringBuffer.getNumChannels() != numChannels)
    {
        ringBuffer.setSize(numChannels, neededSamples, false, true, true);
        ringBuffer.clear();
        ringBufferNumChannels = numChannels;
        writePosition.store(0);
        scratchPosition.store(0.0);
    }
}

void ScratchAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate.store(device->getCurrentSampleRate());
    auto activeIn = device->getActiveInputChannels();
    int numChannels = juce::jmax(1, activeIn.countNumberOfSetBits());
    numChannels = juce::jmin(numChannels, 2);
    ensureRingBufferAllocated(device->getCurrentSampleRate(), numChannels);
}

void ScratchAudioEngine::audioDeviceStopped()
{
}

float ScratchAudioEngine::readInterpolatedSample(int channel, double absoluteSamplePos) const
{
    const int64_t ringLen = ringBuffer.getNumSamples();
    if (ringLen <= 0)
        return 0.0f;

    // Wrap absolute position into ring-buffer index space.
    auto wrap = [ringLen](int64_t v) -> int64_t
    {
        int64_t m = v % ringLen;
        if (m < 0) m += ringLen;
        return m;
    };

    const int64_t base = (int64_t) std::floor(absoluteSamplePos);
    const float frac = (float) (absoluteSamplePos - (double) base);

    const int idx0 = (int) wrap(base);
    const int idx1 = (int) wrap(base + 1);

    const float s0 = ringBuffer.getSample(channel, idx0);
    const float s1 = ringBuffer.getSample(channel, idx1);
    return s0 + frac * (s1 - s0);
}

void ScratchAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                            int numInputChannels,
                                                            float* const* outputChannelData,
                                                            int numOutputChannels,
                                                            int numSamples,
                                                            const juce::AudioIODeviceCallbackContext&)
{
    const juce::ScopedLock sl(ringBufferLock);

    const int ringLen = ringBuffer.getNumSamples();
    if (ringLen <= 0)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    // 1) Write live input into the ring buffer if recording.
    int64_t wp = writePosition.load();
    if (recording.load() && numInputChannels > 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const int idx = (int) ((wp + i) % ringLen);
            for (int ch = 0; ch < ringBufferNumChannels; ++ch)
            {
                const float* in = (ch < numInputChannels && inputChannelData[ch] != nullptr) ? inputChannelData[ch]
                                 : (numInputChannels > 0 && inputChannelData[0] != nullptr ? inputChannelData[0] : nullptr);
                const float sample = in != nullptr ? in[i] : 0.0f;
                ringBuffer.setSample(ch, idx, sample);
            }
        }
        wp += numSamples;
        writePosition.store(wp);
    }

    // 2) Update scratch playback position from jog wheel state.
    const double jogDelta = midi.consumeJogDelta();
    const bool touched = midi.isTouched();
    double pos = scratchPosition.load();

    if (wp == 0)
    {
        pos = 0.0;
    }
    else if (touched)
    {
        pos += jogDelta * (double) sensitivity.load();
    }
    else
    {
        pos += (double) numSamples; // free-run forward at normal speed
    }

    // Don't let playback run past what's actually been recorded, and don't
    // let it drift more than one buffer length behind (that data is gone).
    const double latest = (double) wp;
    const double earliest = latest - (double) ringLen + 4.0;
    pos = juce::jlimit(earliest > 0.0 ? earliest : 0.0, latest, pos);
    scratchPosition.store(pos);

    // 3) Render output: scratch playback mixed with live monitoring.
    const float mGain = monitorGain.load();
    const float sGain = scratchGain.load();

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        float* out = outputChannelData[ch];
        if (out == nullptr)
            continue;

        const int srcChannel = juce::jmin(ch, ringBufferNumChannels - 1);
        const float* liveIn = (ch < numInputChannels && inputChannelData[ch] != nullptr) ? inputChannelData[ch]
                             : (numInputChannels > 0 ? inputChannelData[0] : nullptr);

        for (int i = 0; i < numSamples; ++i)
        {
            const double samplePos = pos - (double) (numSamples - i);
            const float scratched = readInterpolatedSample(srcChannel, samplePos);
            const float live = liveIn != nullptr ? liveIn[i] : 0.0f;
            out[i] = scratched * sGain + live * mGain;
        }
    }
}

bool ScratchAudioEngine::writeRecordingToWavFile(const juce::File& file)
{
    const juce::ScopedLock sl(ringBufferLock);

    const int64_t wp = writePosition.load();
    const int ringLen = ringBuffer.getNumSamples();
    if (ringLen <= 0 || wp <= 0)
        return false;

    const int lengthToExport = (int) juce::jmin<int64_t>(wp, ringLen);

    juce::AudioBuffer<float> exportBuffer(ringBufferNumChannels, lengthToExport);
    const int64_t startAbs = wp - lengthToExport;

    for (int ch = 0; ch < ringBufferNumChannels; ++ch)
    {
        for (int i = 0; i < lengthToExport; ++i)
        {
            const int idx = (int) ((startAbs + i) % ringLen < 0
                                    ? (startAbs + i) % ringLen + ringLen
                                    : (startAbs + i) % ringLen);
            exportBuffer.setSample(ch, i, ringBuffer.getSample(ch, idx));
        }
    }

    file.deleteFile();
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(stream.get(), currentSampleRate.load(),
                                   (unsigned int) ringBufferNumChannels, 24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release(); // writer now owns the stream
    writer->writeFromAudioSampleBuffer(exportBuffer, 0, exportBuffer.getNumSamples());
    return true;
}
