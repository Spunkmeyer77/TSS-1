#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// One rendered phrase: mono audio at eSpeak's own rate (22050 Hz) plus the word timings.
struct SpeechClip : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SpeechClip>;

    struct Word { int startSample, textStart, textLength; };

    juce::AudioBuffer<float> audio;   // 1 channel
    double sampleRate = 22050.0;
    std::vector<Word> words;
    std::vector<int> syllables;       // start sample of every syllable (for the beat-synced mode)
    double f0 = 0.0;                  // measured median pitch in Hz (0 if unvoiced)
    juce::String text;
    juce::uint64 key = 0;             // what it was rendered from (text + voice settings)
    int note = -1;                    // melodic renders: the MIDI note it was made for

    int numSamples() const noexcept   { return audio.getNumSamples(); }

    // index into words for a playback position, or -1
    int wordAt (int sample) const noexcept
    {
        int w = -1;
        for (int i = 0; i < (int) words.size(); ++i)
            if (words[(size_t) i].startSample <= sample) w = i; else break;
        return w;
    }
};

// Everything eSpeak-NG needs to say a phrase.
struct VoiceSettings
{
    int voice = 0;      // index into kVoicePresets
    int speed = 140;    // words per minute
    int pitch = 35;     // 0-99
    int range = 50;     // 0-99 (0 = monotone)
    int gap = 8;        // word gap, 10 ms units
    int amplitude = 100;

    juce::uint64 hash (const juce::String& text) const noexcept;
};

// eSpeak-NG is a single, process-wide engine that is not thread safe, so every plug-in
// instance goes through this one object. Its voice data is built into the plug-in and
// written to disk the first time it is needed.
class EspeakService
{
public:
    static EspeakService& get();

    // writes the voice data (if needed) and starts eSpeak-NG; safe to call from any thread
    bool ensureReady();
    bool isReady() const noexcept         { return ready.load(); }
    juce::String getError() const;

    // renders a phrase; pitchOverride >= 0 replaces settings.pitch. Never call from the audio thread.
    SpeechClip::Ptr render (const juce::String& text, const VoiceSettings& settings, int pitchOverride = -1);

    // the eSpeak pitch (0-99) that comes closest to the given frequency for a voice, and
    // the frequency that pitch is expected to give
    int pitchForFrequency (int voice, int range, double hz, double& expectedHz);

    static double measureF0 (const float* data, int numSamples, double sampleRate);

private:
    EspeakService() = default;
    juce::File dataFolder() const;
    bool extractData (const juce::File& root);
    void applyVoice (const VoiceSettings& s, int pitch);

    juce::CriticalSection lock;
    std::atomic<bool> ready { false };
    bool failed = false;
    juce::String error;

    struct Calibration { double f20 = 0, f80 = 0; int range = -1; };
    std::map<int, Calibration> calibrations;
};
