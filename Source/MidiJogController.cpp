#include "MidiJogController.h"

namespace
{
    void atomicAddDouble(std::atomic<double>& target, double amount)
    {
        double expected = target.load(std::memory_order_relaxed);
        while (! target.compare_exchange_weak(expected, expected + amount,
                                               std::memory_order_relaxed, std::memory_order_relaxed))
        {
        }
    }
}

MidiJogController::MidiJogController() = default;

MidiJogController::~MidiJogController()
{
    closeDevice();
}

juce::StringArray MidiJogController::getAvailableDevices() const
{
    juce::StringArray names;
    for (auto& dev : juce::MidiInput::getAvailableDevices())
        names.add(dev.name);
    return names;
}

bool MidiJogController::openDevice(const juce::String& deviceIdentifier)
{
    closeDevice();

    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        if (dev.name == deviceIdentifier || dev.identifier == deviceIdentifier)
        {
            midiInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (midiInput != nullptr)
            {
                midiInput->start();
                openDeviceName = dev.name;
                return true;
            }
        }
    }
    return false;
}

void MidiJogController::closeDevice()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
    openDeviceName.clear();
}

void MidiJogController::beginLearning(LearnTarget target)
{
    learnTarget.store(target);
}

void MidiJogController::cancelLearning()
{
    learnTarget.store(LearnTarget::none);
}

void MidiJogController::setJogWheelCC(int channel, int ccNumber)
{
    jogChannel.store(channel);
    jogCC.store(ccNumber);
    lastAbsoluteJogValue.store(-1);
    jogIsRelativeEncoder.store(true);
}

void MidiJogController::setTouchNote(int channel, int noteNumber)
{
    touchChannel.store(channel);
    touchNote.store(noteNumber);
}

juce::String MidiJogController::getMappingSummary() const
{
    juce::String s;
    if (jogCC.load() >= 0)
        s << "Jog: ch " << jogChannel.load() << " CC " << jogCC.load() << "  ";
    else
        s << "Jog: not mapped  ";

    if (touchNote.load() >= 0)
        s << "Touch: ch " << touchChannel.load() << " note " << touchNote.load();
    else
        s << "Touch: not mapped";

    return s;
}

void MidiJogController::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (isLearning())
        processLearning(message);
    else
        processMapped(message);
}

void MidiJogController::processLearning(const juce::MidiMessage& message)
{
    auto target = learnTarget.load();

    if (target == LearnTarget::jogWheel && message.isController())
    {
        setJogWheelCC(message.getChannel(), message.getControllerNumber());
        learnTarget.store(LearnTarget::none);
        if (onMappingLearned)
            juce::MessageManager::callAsync([this]
            {
                if (onMappingLearned) onMappingLearned("Learned jog wheel: " + getMappingSummary());
            });
    }
    else if (target == LearnTarget::touchButton && (message.isNoteOn() || message.isNoteOff()))
    {
        setTouchNote(message.getChannel(), message.getNoteNumber());
        learnTarget.store(LearnTarget::none);
        if (onMappingLearned)
            juce::MessageManager::callAsync([this]
            {
                if (onMappingLearned) onMappingLearned("Learned touch button: " + getMappingSummary());
            });
    }
}

void MidiJogController::processMapped(const juce::MidiMessage& message)
{
    if (message.isController()
        && message.getChannel() == jogChannel.load()
        && message.getControllerNumber() == jogCC.load())
    {
        const int value = message.getControllerValue(); // 0-127

        // Relative encoders on most DJ controllers centre around 64:
        // 65..~72 = small forward steps, 63..~56 = small backward steps.
        // Treat anything reasonably close to 64 as relative; a value that
        // sits far from centre repeatedly suggests an absolute knob instead.
        if (jogIsRelativeEncoder.load())
        {
            const int centred = value - 64;
            if (std::abs(centred) <= 32)
            {
                atomicAddDouble(pendingJogDelta, (double) centred);
                return;
            }
            // Didn't look relative this time - fall back to absolute tracking.
            jogIsRelativeEncoder.store(false);
        }

        const int last = lastAbsoluteJogValue.exchange(value);
        if (last >= 0)
        {
            int diff = value - last;
            // handle 0..127 wraparound
            if (diff > 64) diff -= 128;
            if (diff < -64) diff += 128;
            atomicAddDouble(pendingJogDelta, (double) diff);
        }
    }
    else if ((message.isNoteOn() || message.isNoteOff())
        && message.getChannel() == touchChannel.load()
        && message.getNoteNumber() == touchNote.load())
    {
        touched.store(message.isNoteOn() && message.getVelocity() > 0);
    }
}

double MidiJogController::consumeJogDelta()
{
    return pendingJogDelta.exchange(0.0, std::memory_order_relaxed);
}
