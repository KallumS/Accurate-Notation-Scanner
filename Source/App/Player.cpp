#include "Player.h"

#include <cmath>
#include <cstring>

namespace ans
{

namespace
{
// The stand-in: a soft organ-like tone, enough to hear a score by.
struct Tone : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class ToneVoice : public juce::SynthesiserVoice
{
public:
    using juce::SynthesiserVoice::renderNextBlock;
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<Tone*> (s) != nullptr; }
    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0;
        step = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / getSampleRate();
        level = velocity * 0.12;
        envelope = 0;
        releasing = false;
    }
    void stopNote (float, bool allowTail) override
    {
        if (allowTail) releasing = true;
        else { clearCurrentNote(); envelope = 0; }
    }
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int count) override
    {
        if (! isVoiceActive()) return;
        for (int i = 0; i < count; ++i)
        {
            envelope = releasing ? envelope * 0.9993 : std::min (1.0, envelope + 0.002);
            if (releasing && envelope < 0.001) { clearCurrentNote(); break; }
            const double s = std::sin (phase) + 0.3 * std::sin (2 * phase) + 0.12 * std::sin (3 * phase);
            const float v = static_cast<float> (s * level * envelope);
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample (ch, start + i, v);
            phase += step;
        }
    }

private:
    double phase = 0, step = 0, level = 0, envelope = 0;
    bool releasing = false;
};
} // namespace

Player::Player()
{
    for (int i = 0; i < 32; ++i) builtIn.addVoice (new ToneVoice());
    builtIn.addSound (new Tone());
    sound = "the built-in tone";

   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    juce::AudioUnitPluginFormat au;
    juce::OwnedArray<juce::PluginDescription> found;
    au.findAllTypesForFile (found, "AudioUnit:Synths/aumu,dls ,appl");
    if (! found.isEmpty())
    {
        juce::String error;
        appleSynth = au.createInstanceFromDescription (*found.getFirst(), 44100.0, 512, error);
        if (appleSynth != nullptr) sound = "the Mac's General MIDI instruments";
    }
   #endif

    devices.initialiseWithDefaultDevices (0, 2);
    devices.addAudioCallback (this);
}

Player::~Player()
{
    devices.removeAudioCallback (this);
    devices.closeAudioDevice();
}

void Player::play (const Performance& perf, double fromSeconds)
{
    auto seq = std::make_shared<Sequence>();
    for (size_t p = 0; p < perf.channelOfPart.size(); ++p)
        seq->events.push_back ({ 0.0, juce::MidiMessage::programChange (perf.channelOfPart[p] + 1, perf.programOfPart[p]) });
    for (const auto& n : perf.notes)
    {
        const double on = perf.seconds (n.start), off = perf.seconds (n.start + n.length);
        if (off <= fromSeconds) continue;
        seq->events.push_back ({ std::max (on, fromSeconds), juce::MidiMessage::noteOn (n.channel + 1, n.pitch, static_cast<juce::uint8> (n.velocity)) });
        seq->events.push_back ({ off, juce::MidiMessage::noteOff (n.channel + 1, n.pitch) });
    }
    // AutoCC's expression and volume; CC1 and CC21 are left out: on General
    // MIDI sounds CC1 is vibrato, not loudness (decision 0012)
    for (const auto& c : perf.controls)
    {
        if (c.controller != 11 && c.controller != 7) continue;
        seq->events.push_back ({ std::max (perf.seconds (c.start), fromSeconds), juce::MidiMessage::controllerEvent (c.channel + 1, c.controller, c.value) });
    }
    std::stable_sort (seq->events.begin(), seq->events.end(), [] (const auto& a, const auto& b) {
        if (a.seconds < b.seconds || a.seconds > b.seconds) return a.seconds < b.seconds;
        return a.message.isNoteOff() && ! b.message.isNoteOff();
    });
    seq->length = perf.seconds (perf.length()) + 1.5;
    playing = false;
    position = fromSeconds;
    std::atomic_store (&sequence, std::shared_ptr<const Sequence> (seq));
    next = static_cast<size_t> (-1);   // the audio thread finds its place
    playing = true;
}

void Player::stop()
{
    if (playing.exchange (false)) silence = true;
}

void Player::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    rate = device->getCurrentSampleRate();
    const int block = device->getCurrentBufferSizeSamples();
    scratch.setSize (2, block);
    builtIn.setCurrentPlaybackSampleRate (rate);
    if (appleSynth != nullptr)
    {
        appleSynth->setPlayConfigDetails (0, 2, rate, block);
        appleSynth->prepareToPlay (rate, block);
    }
}

void Player::audioDeviceStopped()
{
    if (appleSynth != nullptr) appleSynth->releaseResources();
}

void Player::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* outputs, int outs, int samples,
                                               const juce::AudioIODeviceCallbackContext&)
{
    if (scratch.getNumSamples() < samples) scratch.setSize (2, samples, false, false, true);
    juce::AudioBuffer<float> buffer (scratch.getArrayOfWritePointers(), 2, samples);
    buffer.clear();
    juce::MidiBuffer midi;

    if (silence.exchange (false))
        for (int ch = 1; ch <= 16; ++ch) midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);

    if (playing.load())
    {
        const auto seq = std::atomic_load (&sequence);
        const double from = position.load();
        const double to = from + samples / rate;
        if (seq != nullptr)
        {
            if (next == static_cast<size_t> (-1))
            {
                next = 0;
                while (next < seq->events.size() && seq->events[next].seconds < from && seq->events[next].seconds > 0.0) ++next;
            }
            while (next < seq->events.size() && seq->events[next].seconds < to)
            {
                const int at = juce::jlimit (0, samples - 1, static_cast<int> ((seq->events[next].seconds - from) * rate));
                midi.addEvent (seq->events[next].message, at);
                ++next;
            }
            position = to;
            if (to >= seq->length)
            {
                playing = false;
                for (int ch = 1; ch <= 16; ++ch) midi.addEvent (juce::MidiMessage::allNotesOff (ch), samples - 1);
            }
        }
    }

    if (appleSynth != nullptr) appleSynth->processBlock (buffer, midi);
    else builtIn.renderNextBlock (buffer, midi, 0, samples);

    for (int ch = 0; ch < outs; ++ch)
        if (outputs[ch] != nullptr) std::memcpy (outputs[ch], buffer.getReadPointer (std::min (ch, 1)), sizeof (float) * static_cast<size_t> (samples));
}

} // namespace ans
