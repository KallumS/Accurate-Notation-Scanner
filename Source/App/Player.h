/*
    Player - plays a read score.

    On a Mac it plays through Apple's own General MIDI instruments (the
    DLSMusicDevice Audio Unit every Mac has, as Noterator does), so a score
    sounds with real instruments and nothing has to be installed. Where that
    cannot be loaded, a small synth built in here stands in.

    The audio thread does not allocate or lock: the sequence is handed over
    whole through an atomic shared pointer.
*/

#pragma once

#include "Midi.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

namespace ans
{

class Player : private juce::AudioIODeviceCallback
{
public:
    Player();
    ~Player() override;

    // From `fromSeconds` to the end.
    void play (const Performance& performance, double fromSeconds = 0);
    void stop();
    bool isPlaying() const { return playing.load(); }
    double positionSeconds() const { return position.load(); }
    const juce::String& soundName() const { return sound; }

private:
    struct Sequence
    {
        struct Event { double seconds; juce::MidiMessage message; };
        std::vector<Event> events;
        double length = 0;
    };

    juce::AudioDeviceManager devices;
    std::unique_ptr<juce::AudioPluginInstance> appleSynth;
    juce::Synthesiser builtIn;
    juce::String sound;
    std::shared_ptr<const Sequence> sequence;
    std::atomic<bool> playing { false }, silence { false };
    std::atomic<double> position { 0 };
    std::atomic<size_t> next { 0 };
    double rate = 44100;
    juce::AudioBuffer<float> scratch;

    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int outs, int samples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
};

} // namespace ans
