#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const char* const kDefaultPhrases[kNumSlots] =
    {
        "Transistor speech system online", "Dive into the deep", "Aquatic transmission", "Signal detected",
        "Systems nominal", "Warning", "Electro", "Initiate sequence",
        "Machine language", "Hello human", "Mutation", "Pressure rising",
        "Lost in the waves", "Frequency", "One two three four", "Bass",
        "Ready", "Error", "Transmission ends", "Shutdown"
    };

    constexpr int kMelodicLow = 24, kMelodicHigh = 96;     // notes the melodic mode prepares
    double noteHz (int note) { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TSS1Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray voiceNames;
    for (auto& p : kVoicePresets) voiceNames.add (p.name);
    const auto& d = kVoicePresets[kDefaultVoice];

    l.add (std::make_unique<AudioParameterChoice> (ParameterID ("voice", 1), "Voice", voiceNames, kDefaultVoice));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("speed", 1), "Speed", 80, 300, d.speed, AudioParameterIntAttributes().withLabel ("wpm")));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("pitch", 1), "Pitch", 0, 99, d.pitch));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("range", 1), "Range", 0, 99, 50));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("gap", 1), "Word Gap", 0, 30, d.gap, AudioParameterIntAttributes().withLabel ("x10 ms")));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("amp", 1), "Voice Volume", 10, 200, d.amplitude));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID ("mode", 1), "Mode", StringArray { "Trigger", "Melodic", "Syllable" }, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID ("root", 1), "Root Note", 0, 108, 36,
               AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return TSS1Processor::noteName (v); })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID ("poly", 1), "Voices", StringArray { "Mono", "Poly" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID ("gate", 1), "Gate", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID ("sync", 1), "Sync", StringArray { "Off", "1/4", "1/8", "1/16", "1/8T", "1/16T" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID ("fx", 1), "FX", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID ("intensity", 1), "FX Amount", NormalisableRange<float> (0.5f, 2.0f, 0.01f), 1.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID ("filter", 1), "Filter", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID ("harmonic", 1), "Harmonic Boost", NormalisableRange<float> (1.0f, 2.0f, 0.01f), 1.2f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID ("crush", 1), "Crush", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID ("level", 1), "Level", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
               AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

TSS1Processor::TSS1Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TSS1", createLayout())
{
    voice     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("voice"));
    speed     = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("speed"));
    pitch     = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("pitch"));
    range     = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("range"));
    gap       = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("gap"));
    amp       = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("amp"));
    root      = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("root"));
    mode      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode"));
    poly      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("poly"));
    gate      = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("gate"));
    sync      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("sync"));
    fxOn      = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("fx"));
    filterOn  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("filter"));
    intensity = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("intensity"));
    harmonic  = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("harmonic"));
    crush     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("crush"));
    level     = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("level"));

    for (int i = 0; i < kNumSlots; ++i) phrases[i] = kDefaultPhrases[i];
    for (auto* id : { "voice", "speed", "pitch", "range", "gap", "amp", "mode" })
        apvts.addParameterListener (id, this);

    renderer.startThread (juce::Thread::Priority::normal);
}

TSS1Processor::~TSS1Processor()
{
    renderer.stopThread (4000);
}

juce::String TSS1Processor::noteName (int note)
{
    // Ableton naming: MIDI 60 = C3, 36 = C1
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}

void TSS1Processor::parameterChanged (const juce::String&, float)
{
    ++changeCounter;
    renderer.notify();
}

VoiceSettings TSS1Processor::currentSettings() const
{
    VoiceSettings s;
    s.voice = voice->getIndex();
    s.speed = speed->get();
    s.pitch = pitch->get();
    s.range = range->get();
    s.gap = gap->get();
    s.amplitude = amp->get();
    return s;
}

juce::String TSS1Processor::getPhrase (int slot) const
{
    const juce::ScopedLock sl (phraseLock);
    return phrases[juce::jlimit (0, kNumSlots - 1, slot)];
}

void TSS1Processor::setPhrase (int slot, const juce::String& text)
{
    {
        const juce::ScopedLock sl (phraseLock);
        auto& p = phrases[juce::jlimit (0, kNumSlots - 1, slot)];
        if (p == text) return;
        p = text;
    }
    ++changeCounter;
    renderer.notify();
}

void TSS1Processor::setSelectedSlot (int slot)
{
    selectedSlot = juce::jlimit (0, kNumSlots - 1, slot);
    ++changeCounter;
    renderer.notify();
}

void TSS1Processor::loadVoicePreset (int index)
{
    index = juce::jlimit (0, kNumVoices - 1, index);
    const auto& p = kVoicePresets[index];
    auto set = [this] (const char* id, float v)
    {
        auto* prm = apvts.getParameter (id);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    };
    set ("voice", (float) index);
    set ("speed", (float) p.speed);
    set ("pitch", (float) p.pitch);
    set ("gap", (float) p.gap);
    set ("amp", (float) p.amplitude);
}

TSS1Processor::SlotStatus TSS1Processor::getSlotStatus (int slot) const
{
    const auto text = getPhrase (slot).trim();
    if (text.isEmpty()) return SlotStatus::empty;
    const auto key = currentSettings().hash (text);
    const juce::SpinLock::ScopedLockType sl (clipLock);
    auto& c = slotClips[slot];
    return (c != nullptr && c->key == key) ? SlotStatus::ready : SlotStatus::rendering;
}

SpeechClip::Ptr TSS1Processor::getClip (int slot) const
{
    const juce::SpinLock::ScopedLockType sl (clipLock);
    return slotClips[juce::jlimit (0, kNumSlots - 1, slot)];
}

TSS1Processor::PlayState TSS1Processor::getPlayState() const noexcept
{
    PlayState s;
    s.slot = uiSlot.load(); s.note = uiNote.load(); s.position = uiPos.load(); s.playing = uiPlaying.load();
    return s;
}

SpeechClip::Ptr TSS1Processor::getPlayingClip() const
{
    const juce::SpinLock::ScopedLockType sl (clipLock);
    return uiClip;
}

//==============================================================================
void RenderThread::requestNote (int note) noexcept
{
    const auto scope = noteFifo.write (1);
    if (scope.blockSize1 > 0) noteQueue[scope.startIndex1] = note;
}

void RenderThread::run()
{
    auto& es = EspeakService::get();
    if (! es.ensureReady())
    {
        owner.busy = false;
        return;
    }

    juce::uint32 seenChange = 0;
    juce::uint32 lastChangeTime = 0;
    juce::uint32 stableCounter = 0;

    while (! threadShouldExit())
    {
        // drop renders nothing plays any more (only 'retired' still refers to them)
        {
            juce::ReferenceCountedArray<SpeechClip> toFree;
            {
                const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                for (int i = owner.retired.size(); --i >= 0;)
                    if (owner.retired.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
                        toFree.add (owner.retired.removeAndReturn (i));
            }
        }   // freed here, outside the lock

        const auto change = owner.changeCounter.load();
        if (change != seenChange)
        {
            seenChange = change;
            lastChangeTime = juce::Time::getMillisecondCounter();
        }
        // wait until the controls stop moving before re-rendering (turning a knob sends many changes)
        if (juce::Time::getMillisecondCounter() - lastChangeTime < 120)
        {
            wait (30);
            continue;
        }
        stableCounter = change;

        const auto settings = owner.currentSettings();
        const int sel = owner.selectedSlot.load();
        bool didWork = false;

        // 1) melodic notes that were asked for but are missing
        if (owner.isMelodic())
        {
            const auto text = owner.getPhrase (sel).trim();
            const auto melKey = settings.hash (text) ^ (juce::uint64) (sel + 1);
            if (melKey != owner.melodicKey)
            {
                const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                for (auto& c : owner.melodicClips)
                    if (c != nullptr) { owner.retired.add (c); c = nullptr; }
                owner.melodicKey = melKey;
            }
            auto renderNote = [&] (int note)
            {
                if (note < 0 || note > 127 || text.isEmpty()) return;
                {
                    const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                    if (owner.melodicClips[note] != nullptr) return;
                }
                double expected = 0;
                const int p = es.pitchForFrequency (settings.voice, settings.range, noteHz (note), expected);
                auto clip = es.render (text, settings, p);
                if (clip == nullptr) return;
                // trust the measurement unless it jumped an octave
                if (clip->f0 <= 0 || clip->f0 > expected * 1.6 || clip->f0 < expected * 0.6) clip->f0 = expected;
                clip->note = note;
                const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                if (owner.melodicKey == melKey) owner.melodicClips[note] = clip;
                didWork = true;
            };

            int n;
            while (noteFifo.getNumReady() > 0)
            {
                const auto scope = noteFifo.read (1);
                n = noteQueue[scope.startIndex1];
                owner.busy = true;
                renderNote (n);
            }
            if (owner.changeCounter.load() != stableCounter) continue;
        }

        // 2) the phrase slots, the selected one first
        for (int k = 0; k < kNumSlots && ! threadShouldExit(); ++k)
        {
            const int slot = (sel + k) % kNumSlots;
            const auto text = owner.getPhrase (slot).trim();
            if (text.isEmpty()) continue;
            const auto key = settings.hash (text);
            {
                const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                if (owner.slotClips[slot] != nullptr && owner.slotClips[slot]->key == key) continue;
            }
            owner.busy = true;
            auto clip = es.render (text, settings);
            if (clip == nullptr) continue;
            {
                const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                if (owner.slotClips[slot] != nullptr) owner.retired.add (owner.slotClips[slot]);
                owner.slotClips[slot] = clip;
            }
            didWork = true;
            if (owner.changeCounter.load() != stableCounter || noteFifo.getNumReady() > 0) break;   // settings moved on
        }

        // 3) melodic mode: prepare every note of the selected phrase, from the middle outwards
        if (owner.isMelodic() && owner.changeCounter.load() == stableCounter && noteFifo.getNumReady() == 0)
        {
            const auto text = owner.getPhrase (sel).trim();
            if (text.isNotEmpty())
            {
                for (int k = 0; k <= kMelodicHigh - kMelodicLow && ! threadShouldExit(); ++k)
                {
                    const int note = 60 + ((k & 1) ? (k + 1) / 2 : -(k / 2));
                    if (note < kMelodicLow || note > kMelodicHigh) continue;
                    bool have;
                    {
                        const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                        have = owner.melodicClips[note] != nullptr;
                    }
                    if (have) continue;
                    owner.busy = true;
                    double expected = 0;
                    const int p = es.pitchForFrequency (settings.voice, settings.range, noteHz (note), expected);
                    auto clip = es.render (text, settings, p);
                    if (clip != nullptr)
                    {
                        if (clip->f0 <= 0 || clip->f0 > expected * 1.6 || clip->f0 < expected * 0.6) clip->f0 = expected;
                        clip->note = note;
                        const juce::SpinLock::ScopedLockType sl (owner.clipLock);
                        if (owner.melodicKey == (settings.hash (text) ^ (juce::uint64) (sel + 1)))
                            owner.melodicClips[note] = clip;
                    }
                    didWork = true;
                    break;   // one at a time, so new notes and edits get in quickly
                }
            }
        }

        if (! didWork)
        {
            owner.busy = false;
            wait (40);
        }
    }
}

//==============================================================================
void TSS1Processor::prepareToPlay (double sampleRate, int)
{
    hostRate = sampleRate;
    for (auto& v : voices) { v.active = false; v.clip = nullptr; }
    fxPrev = fxS1 = fxS2 = crushHold = 0;
    crushCount = 0;
    levelGain.reset (sampleRate, 0.02);
    levelGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (level->get()));
}

bool TSS1Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

double TSS1Processor::stepBeats() const noexcept
{
    static constexpr double steps[] = { 0.0, 1.0, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0 };
    return steps[juce::jlimit (0, 5, sync->getIndex())];
}

void TSS1Processor::startVoice (int slot, int note, float velocity, int syllable)
{
    SpeechClip::Ptr clip;
    double ratio = 1.0;
    const bool melodic = isMelodic();
    {
        const juce::SpinLock::ScopedLockType sl (clipLock);
        if (melodic)
        {
            const double target = noteHz (note);
            clip = melodicClips[note];
            if (clip == nullptr)
            {
                // not rendered yet: borrow the nearest note's render (or the plain phrase) and retune it
                for (int d = 1; d < 128 && clip == nullptr; ++d)
                {
                    if (note - d >= 0 && melodicClips[note - d] != nullptr) clip = melodicClips[note - d];
                    else if (note + d < 128 && melodicClips[note + d] != nullptr) clip = melodicClips[note + d];
                }
                if (clip == nullptr) clip = slotClips[slot];
                renderer.requestNote (note);
            }
            if (clip != nullptr && clip->f0 > 0) ratio = juce::jlimit (0.25, 4.0, target / clip->f0);
        }
        else
        {
            clip = slotClips[slot];
        }
    }
    if (clip == nullptr || clip->numSamples() < 2) return;
    if (syllable >= (int) clip->syllables.size()) return;   // syllable mode: past the last syllable
    if (melodic && renderer.isThreadRunning()) renderer.notify();

    // mono: the new note cuts the others with a short fade
    if (poly->getIndex() == 0)
        for (auto& v : voices)
            if (v.active) v.fadeStep = -1.0f / (float) (0.006 * hostRate);

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age > target->age) target = &v;
    }
    for (auto& v : voices) ++v.age;

    target->clip = clip;               // safe: the clip is still referenced by the store or 'retired'
    target->pos = 0;
    target->inc = clip->sampleRate / hostRate * ratio;
    target->gain = 0.3f + 0.7f * velocity;
    target->fade = 0.0f;
    target->fadeStep = 1.0f / (float) (0.002 * hostRate);
    target->slot = slot;
    target->note = note;
    target->age = 0;
    target->active = true;
    target->held = true;
    target->seq = false;
    target->chop = false;

    if (syllable >= 0)
    {
        // syllable mode: this key says one syllable of the phrase
        target->chop = true;
        target->pos = clip->syllables[(size_t) syllable];
        target->segEnd = syllable + 1 < (int) clip->syllables.size() ? clip->syllables[(size_t) syllable + 1] : clip->numSamples();
    }
    else

    if (const double step = stepBeats(); step > 0 && ! clip->syllables.empty())
    {
        // speak the first syllable now and each next one on the following steps of the host's grid
        const double notePpq = blockPpq + eventOffset / samplesPerBeat;
        target->seq = true;
        target->syl = 0;
        target->pos = clip->syllables[0];
        target->segEnd = clip->syllables.size() > 1 ? clip->syllables[1] : clip->numSamples();
        target->nextStep = (juce::int64) std::floor (notePpq / step + 1.0e-4) + 1;
        target->countdown = ((double) target->nextStep * step - notePpq) * samplesPerBeat;
    }

    uiSlot = slot; uiNote = note; uiPos = 0.0; uiPlaying = true;
    const juce::SpinLock::ScopedLockType sl (clipLock);
    uiClip = clip;
}

void TSS1Processor::stopNote (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note) v.held = false;
    if (! gate->get()) return;
    for (auto& v : voices)
        if (v.active && v.note == note && v.fadeStep >= 0)
            v.fadeStep = -1.0f / (float) (0.012 * hostRate);
}

void TSS1Processor::renderVoices (float* out, int numSamples)
{
    int newestAge = 1 << 30;
    const Voice* newest = nullptr;
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float* d = v.clip->audio.getReadPointer (0);
        const int len = v.clip->numSamples();
        const auto& syls = v.clip->syllables;
        const double step = stepBeats();
        const double fadeLen = 0.004 * hostRate;     // syllable edges
        for (int i = 0; i < numSamples; ++i)
        {
            if (v.seq)
            {
                if (step <= 0) v.seq = false;    // sync switched off: let the phrase run on
                else if ((v.countdown -= 1.0) <= 0.0)
                {
                    // next step: next syllable (gate mode loops the phrase while the key is held)
                    ++v.nextStep;
                    v.countdown += step * samplesPerBeat;
                    if (++v.syl >= (int) syls.size())
                    {
                        if (gate->get() && v.held) v.syl = 0;
                        else { v.active = false; break; }
                    }
                    v.pos = syls[(size_t) v.syl];
                    v.segEnd = v.syl + 1 < (int) syls.size() ? syls[(size_t) v.syl + 1] : len;
                    v.fade = 0.0f;
                    if (v.fadeStep >= 0) v.fadeStep = 1.0f / (float) (0.002 * hostRate);
                }
            }

            const int i1 = (int) v.pos;
            if (! v.seq && i1 >= len - 1) { v.active = false; break; }
            float edge = 1.0f;
            if (v.chop)
            {
                const int end = juce::jmin (v.segEnd, len - 1);
                if (i1 >= end) { v.active = false; break; }
                edge = (float) juce::jlimit (0.0, 1.0, (end - v.pos) / (v.inc * fadeLen));
            }
            else if (v.seq)
            {
                const int end = juce::jmin (v.segEnd, len - 1);
                if (i1 >= end) { v.fade += v.fadeStep; if (v.fade <= 0.0f && v.fadeStep < 0) { v.active = false; break; } continue; }
                edge = (float) juce::jlimit (0.0, 1.0, (end - v.pos) / (v.inc * fadeLen));
            }
            const float t = (float) (v.pos - i1);
            const float y0 = d[juce::jmax (0, i1 - 1)], y1 = d[i1], y2 = d[i1 + 1], y3 = d[juce::jmin (len - 1, i1 + 2)];
            // 4-point Hermite interpolation
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            const float s = ((c3 * t + c2) * t + c1) * t + y1;

            v.fade += v.fadeStep;
            if (v.fade >= 1.0f) { v.fade = 1.0f; if (v.fadeStep > 0) v.fadeStep = 0; }
            if (v.fade <= 0.0f && v.fadeStep < 0) { v.active = false; break; }
            out[i] += s * v.gain * v.fade * edge;
            v.pos += v.inc;
        }
        if (! v.active) { v.clip = nullptr; continue; }   // never the last reference (see 'retired')
        if (v.age < newestAge) { newestAge = v.age; newest = &v; }
    }
    if (newest != nullptr) { uiPos = newest->pos; uiPlaying = true; }
    else uiPlaying = false;
}

void TSS1Processor::processFx (float* x, int n)
{
    const float drive = intensity->get();
    const bool fx = fxOn->get();
    const float c = crush->get();
    if (fx)
    {
        const bool filt = filterOn->get();
        const float harm = harmonic->get() * drive;
        const int levels = juce::jlimit (16, 256, (int) (256.0f / (drive + 0.5f)));
        const float artifact = juce::jmin (0.3f * drive, 0.6f);
        const int bits = juce::roundToInt (16.0f - 12.0f * c);
        const float q = (float) (1 << (bits - 1));
        const int hold = juce::jmax (1, juce::roundToInt ((1.0f + 7.0f * c * c) * (float) hostRate / 22050.0f));

        for (int i = 0; i < n; ++i)
        {
            float s = x[i];
            if (filt)
            {
                // DJZ-Speak frequency filter: a touch of high-mid emphasis, then a gentle 3-tap smooth
                const float e = s + (s - fxPrev) * 0.3f * drive;
                fxPrev = s;
                s = 0.25f * e + 0.5f * fxS1 + 0.25f * fxS2;
                fxS2 = fxS1; fxS1 = e;
            }
            if (harm > 1.0f)
            {
                // harmonic enhancement: tanh saturation blended with the dry voice
                const float sat = std::tanh (s / 0.8f * harm) * 0.64f;
                s = s * 0.6f + sat * 0.4f;
            }
            // mechanical artifacts: partial quantisation
            const float qs = std::round (s * (float) levels) / (float) levels;
            s = s * (1.0f - artifact) + qs * artifact;

            if (c > 0.001f)
            {
                if (crushCount <= 0) { crushHold = std::round (s * q) / q; crushCount = hold; }
                --crushCount;
                s = crushHold;
            }
            x[i] = s;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        float s = x[i] * levelGain.getNextValue();
        if (std::abs (s) > 0.9f) s = (s > 0 ? 1.0f : -1.0f) * (0.9f + 0.1f * std::tanh ((std::abs (s) - 0.9f) * 10.0f));
        x[i] = s;
    }
}

void TSS1Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    levelGain.setTargetValue (juce::Decibels::decibelsToGain (level->get()));

    // the beat clock: the host's position while it plays, otherwise a free-running clock at its tempo
    {
        double bpm = 120.0;
        bool playing = false;
        double ppq = 0.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
            }
        samplesPerBeat = hostRate * 60.0 / bpm;
        blockPpq = playing ? ppq : freePpq;
        freePpq = blockPpq + numSamples / samplesPerBeat;
        if (const double step = stepBeats(); step > 0)
            for (auto& v : voices)
                if (v.active && v.seq)
                {
                    v.countdown = ((double) v.nextStep * step - blockPpq) * samplesPerBeat;
                    if (v.countdown < 0 || v.countdown > step * samplesPerBeat + 1.0)   // loop jump or tempo change
                    {
                        v.nextStep = (juce::int64) std::floor (blockPpq / step) + 1;
                        v.countdown = ((double) v.nextStep * step - blockPpq) * samplesPerBeat;
                    }
                }
    }
    eventOffset = 0;

    if (const int pv = pendingPreview.exchange (-1); pv >= 0)
    {
        if (isMelodic()) startVoice (pv, 60, 0.8f);
        else startVoice (pv, root->get() + pv, 0.8f);
    }

    float* out = buffer.getWritePointer (0);
    int pos = 0;
    const int rootNote = root->get();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        if (t > pos) { renderVoices (out + pos, t - pos); pos = t; }
        eventOffset = t;

        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            if (isMelodic())
                startVoice (selectedSlot.load(), note, m.getFloatVelocity());
            else if (isSyllableMode())
            {
                if (note >= rootNote && note < rootNote + 64)
                    startVoice (selectedSlot.load(), note, m.getFloatVelocity(), note - rootNote);
            }
            else if (note >= rootNote && note < rootNote + kNumSlots)
                startVoice (note - rootNote, note, m.getFloatVelocity());
        }
        else if (m.isNoteOff())
            stopNote (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.active) v.fadeStep = -1.0f / (float) (0.006 * hostRate);
    }
    if (pos < numSamples) renderVoices (out + pos, numSamples - pos);

    processFx (out, numSamples);
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

//==============================================================================
void TSS1Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto ph = juce::ValueTree ("PHRASES");
    {
        const juce::ScopedLock sl (phraseLock);
        for (int i = 0; i < kNumSlots; ++i)
            ph.appendChild (juce::ValueTree ("P").setProperty ("i", i, nullptr).setProperty ("t", phrases[i], nullptr), nullptr);
    }
    ph.setProperty ("selected", selectedSlot.load(), nullptr);
    state.removeChild (state.getChildWithName ("PHRASES"), nullptr);
    state.appendChild (ph, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TSS1Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;
    auto ph = state.getChildWithName ("PHRASES");
    if (ph.isValid())
    {
        {
            const juce::ScopedLock sl (phraseLock);
            for (auto c : ph)
            {
                const int i = c.getProperty ("i", -1);
                if (i >= 0 && i < kNumSlots) phrases[i] = c.getProperty ("t").toString();
            }
        }
        selectedSlot = juce::jlimit (0, kNumSlots - 1, (int) ph.getProperty ("selected", 0));
        state.removeChild (ph, nullptr);
    }
    apvts.replaceState (state);
    ++changeCounter;
    renderer.notify();
}

juce::AudioProcessorEditor* TSS1Processor::createEditor()
{
    return new TSS1Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TSS1Processor();
}
