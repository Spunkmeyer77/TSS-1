#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpeechEngine.h"
#include "Presets.h"

class TSS1Processor;

// the user's own syllable cuts for one slot (start samples), valid for clips with the same cutKey
struct CutList : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<CutList>;
    juce::uint64 key = 0;
    std::vector<int> cuts;
};

// Renders phrases in the background so that notes play instantly.
class RenderThread : public juce::Thread
{
public:
    explicit RenderThread (TSS1Processor& p) : juce::Thread ("TSS-1 speech"), owner (p) {}
    void run() override;
    void requestNote (int note) noexcept;   // audio thread: a melodic note that has no render yet
private:
    TSS1Processor& owner;
    juce::AbstractFifo noteFifo { 64 };
    int noteQueue[64] {};
    friend class TSS1Processor;
};

class TSS1Processor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    TSS1Processor();
    ~TSS1Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }
    const juce::String getName() const override           { return "TSS-1"; }
    bool acceptsMidi() const override                     { return true; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 0.0; }
    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- phrases (message thread)
    juce::String getPhrase (int slot) const;
    void setPhrase (int slot, const juce::String& text);
    int getSelectedSlot() const noexcept                  { return selectedSlot.load(); }
    void setSelectedSlot (int slot);

    // loads a voice preset into the VOICE, SPEED, PITCH, GAP and VOL controls
    void loadVoicePreset (int index);
    void previewSlot (int slot) noexcept                  { pendingPreview.store (slot); }

    // syllable cuts of a slot: the user's markers if there are any, otherwise the automatic (or | divider) ones
    std::vector<int> getCuts (int slot, bool* custom = nullptr) const;
    void setCuts (int slot, std::vector<int> cuts);      // message thread
    void resetCuts (int slot);
    void previewSyllable (int slot, int index) noexcept   { pendingSyllable.store (slot * 1024 + index); }

    enum class SlotStatus { empty, rendering, ready };
    SlotStatus getSlotStatus (int slot) const;
    SpeechClip::Ptr getClip (int slot) const;            // the current render of a slot (any thread but audio)

    // what is playing now, for the screen
    struct PlayState { int slot = -1; int note = -1; double position = 0; bool playing = false; };
    PlayState getPlayState() const noexcept;
    SpeechClip::Ptr getPlayingClip() const;
    bool isMelodic() const noexcept                       { return mode->getIndex() == 1; }
    bool isSyllableMode() const noexcept                  { return mode->getIndex() == 2; }
    bool isRendering() const noexcept                     { return busy.load(); }

    static juce::String noteName (int note);

private:
    friend class RenderThread;
    void parameterChanged (const juce::String&, float) override;
    VoiceSettings currentSettings() const;

    struct Voice
    {
        SpeechClip::Ptr clip;
        double pos = 0, inc = 1;
        float gain = 1, fade = 1, fadeStep = 0;
        int slot = -1, note = -1, age = 0;
        bool active = false;
        // beat-synced mode: one syllable per step
        bool seq = false, held = false, chop = false;   // chop: plays one syllable only
        CutList::Ptr cuts;                              // user cuts in use (else the clip's syllables)
        int syl = 0, segEnd = 0;
        juce::int64 nextStep = 0;
        double countdown = 0;
    };
    void startVoice (int slot, int note, float velocity, int syllable = -1);
    void stopNote (int note);
    void renderVoices (float* out, int numSamples);
    void processFx (float* mono, int numSamples);

    // parameters
    juce::AudioParameterChoice* voice = nullptr;
    juce::AudioParameterInt *speed = nullptr, *pitch = nullptr, *range = nullptr, *gap = nullptr, *amp = nullptr, *root = nullptr;
    juce::AudioParameterChoice *mode = nullptr, *poly = nullptr, *sync = nullptr;
    juce::AudioParameterBool *gate = nullptr, *fxOn = nullptr, *filterOn = nullptr;
    juce::AudioParameterFloat *intensity = nullptr, *harmonic = nullptr, *crush = nullptr, *level = nullptr;

    // phrases and renders
    mutable juce::CriticalSection phraseLock;
    juce::String phrases[kNumSlots];
    std::atomic<int> selectedSlot { 0 };
    std::atomic<int> pendingPreview { -1 }, pendingSyllable { -1 };
    std::atomic<juce::uint32> changeCounter { 1 };
    std::atomic<bool> busy { false };

    mutable juce::SpinLock clipLock;                     // guards the clip pointers below
    SpeechClip::Ptr slotClips[kNumSlots];
    SpeechClip::Ptr melodicClips[128];                   // renders of the selected slot per MIDI note
    juce::uint64 melodicKey = 0;
    juce::ReferenceCountedArray<SpeechClip> retired;     // freed on the render thread, never the audio thread
    CutList::Ptr cutLists[kNumSlots];                    // guarded by clipLock
    juce::ReferenceCountedArray<CutList> retiredCuts;

    RenderThread renderer { *this };

    // audio
    static constexpr int kMaxVoices = 8;
    Voice voices[kMaxVoices];
    double hostRate = 44100.0;
    std::atomic<int> uiSlot { -1 }, uiNote { -1 };
    std::atomic<double> uiPos { 0 };
    std::atomic<bool> uiPlaying { false };
    SpeechClip::Ptr uiClip;                              // guarded by clipLock
    double stepBeats() const noexcept;        // length of one sync step in beats (0 = sync off)
    double blockPpq = 0, freePpq = 0, samplesPerBeat = 22050, eventOffset = 0;
    float fxPrev = 0, fxS1 = 0, fxS2 = 0, crushHold = 0;
    int crushCount = 0;
    juce::LinearSmoothedValue<float> levelGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSS1Processor)
};
