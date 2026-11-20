// Headless tests for TSS-1: renders, trigger notes, every voice preset, melodic pitch, gate, FX,
// state save/restore; writes audio examples and editor snapshots.
//
//   TSS1Test <output folder>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static bool waitFor (std::function<bool()> cond, int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
    {
        if (cond()) return true;
        juce::Thread::sleep (20);
    }
    return cond();
}

static void writeWav (const juce::File& f, const std::vector<float>& data, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (out.get(), sr, 1, 16, {}, 0))
    {
        out.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        const float* ch[1] = { data.data() };
        writer->writeFromFloatArrays (ch, 1, (int) data.size());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    TSS1Processor proc;
    const double sr = 48000.0;
    const int block = 256;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    auto setParam = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };

    std::vector<float> rec, demo;
    auto run = [&] (int blocks, juce::MidiBuffer* midi = nullptr, std::vector<float>* also = nullptr)
    {
        rec.clear();
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer empty;
            auto& m = (midi != nullptr && b == 0) ? *midi : empty;
            proc.processBlock (buf, m);
            for (int i = 0; i < block; ++i) rec.push_back (buf.getSample (0, i));
        }
        if (also) also->insert (also->end(), rec.begin(), rec.end());
    };
    auto note = [] (int n, float v = 0.9f) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, n, v), 0); return m; };
    auto rms = [&] { double s = 0; for (float x : rec) s += (double) x * x; return std::sqrt (s / juce::jmax ((size_t) 1, rec.size())); };
    auto peak = [&] { float p = 0; for (float x : rec) p = juce::jmax (p, std::abs (x)); return p; };
    auto finite = [&] { for (float x : rec) if (! std::isfinite (x)) return false; return true; };
    auto allReady = [&] { for (int i = 0; i < kNumSlots; ++i) if (proc.getSlotStatus (i) != TSS1Processor::SlotStatus::ready) return false; return true; };

    // ---- the speech engine starts and renders all 20 default phrases
    check (waitFor ([&] { return EspeakService::get().isReady(); }, 10000), "speech engine ready (" + EspeakService::get().getError() + ")");
    const auto t0 = juce::Time::getMillisecondCounter();
    check (waitFor (allReady, 30000), "all 20 phrases rendered in " + juce::String ((int) (juce::Time::getMillisecondCounter() - t0)) + " ms");
    {
        auto c = proc.getClip (1);
        check (c != nullptr && c->numSamples() > 22050 / 2, "slot 2 clip has audio");
        check (c != nullptr && c->words.size() == 4, "slot 2 has 4 word timings (" + juce::String (c ? (int) c->words.size() : -1) + ")");
        check (c != nullptr && c->f0 > 50 && c->f0 < 200, "slot 2 pitch measured: " + juce::String (c ? c->f0 : 0.0, 1) + " Hz");
    }

    // ---- trigger mode: C1 + 1 = slot 2
    {
        auto m = note (37);
        run ((int) (sr * 2.5 / block), &m, &demo);
        check (rms() > 0.02 && finite() && peak() <= 1.0f, "note 37 speaks slot 2 (rms " + juce::String (rms(), 3) + ", peak " + juce::String (peak(), 3) + ")");
        writeWav (outDir.getChildFile ("trigger_slot2.wav"), rec, sr);
        auto ps = proc.getPlayState();
        check (ps.slot == 1, "play state shows slot 2");
        auto m2 = note (20);
        run ((int) (sr * 0.5 / block), &m2);
        check (rms() < 0.001, "a note below the slots is silent");
    }

    // ---- preview from the UI
    proc.previewSlot (5);
    run ((int) (sr * 1.0 / block));
    check (rms() > 0.02, "preview of slot 6 plays");

    // ---- every voice preset
    for (int v = 0; v < kNumVoices; ++v)
    {
        proc.loadVoicePreset (v);
        const bool ok = waitFor ([&] { return proc.getSlotStatus (0) == TSS1Processor::SlotStatus::ready; }, 10000);
        auto m = note (36);
        run ((int) (sr * 2.8 / block), &m, v % 4 == 0 ? &demo : nullptr);
        check (ok && rms() > 0.01 && finite() && peak() <= 1.0f,
               juce::String ("voice ") + kVoicePresets[v].name + " (rms " + juce::String (rms(), 3) + ", peak " + juce::String (peak(), 2) + ")");
    }
    proc.loadVoicePreset (kDefaultVoice);

    // ---- mono cuts the previous phrase, gate stops on note-off
    waitFor (allReady, 20000);
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 36, 0.9f), 0);
        m.addEvent (juce::MidiMessage::noteOn (1, 37, 0.9f), 100);
        run (40, &m);
        setParam ("gate", 1.0f);
        juce::MidiBuffer g1; g1.addEvent (juce::MidiMessage::noteOn (1, 36, 0.9f), 0);
        run (20, &g1);
        juce::MidiBuffer g2; g2.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        run (10, &g2);
        run (10);
        check (rms() < 0.0005, "gate: note-off stops the phrase");
        setParam ("gate", 0.0f);
    }

    // ---- FX on/off changes the sound; crush works
    {
        auto play = [&] { auto m = note (37); run ((int) (sr * 1.5 / block), &m); return rec; };
        setParam ("fx", 0.0f);  auto dry = play();
        setParam ("fx", 1.0f);  auto wet = play();
        double diff = 0; for (size_t i = 0; i < juce::jmin (dry.size(), wet.size()); ++i) diff += std::abs (dry[i] - wet[i]);
        check (diff > 1.0, "FX changes the sound");
        setParam ("crush", 0.8f); auto crushed = play();
        check (rms() > 0.01 && finite(), "crush plays");
        writeWav (outDir.getChildFile ("crush.wav"), crushed, sr);
        setParam ("crush", 0.0f);
    }

    // ---- melodic mode: keys set the pitch
    {
        proc.setSelectedSlot (6);   // "Electro"
        setParam ("mode", 1.0f);
        setParam ("range", 0.0f);
        waitFor ([&] { return ! proc.isRendering(); }, 2000);
        juce::Thread::sleep (300);
        check (waitFor ([&] { return ! proc.isRendering(); }, 60000), "melodic notes prepared");
        std::vector<float> mel;
        for (int n : { 36, 43, 48, 55, 60 })
        {
            auto m = note (n);
            run ((int) (sr * 0.9 / block), &m, &mel);
            const double hz = EspeakService::measureF0 (rec.data(), (int) rec.size(), sr);
            const double target = 440.0 * std::pow (2.0, (n - 69) / 12.0);
            const double cents = hz > 0 ? 1200.0 * std::log2 (hz / target) : 9999.0;
            check (std::abs (cents) < 60.0, "melodic " + TSS1Processor::noteName (n) + ": " + juce::String (hz, 1) + " Hz (target "
                                              + juce::String (target, 1) + ", " + juce::String ((int) cents) + " cents)");
        }
        writeWav (outDir.getChildFile ("melodic.wav"), mel, sr);
        demo.insert (demo.end(), mel.begin(), mel.end());
    }

    // ---- state save / restore
    {
        proc.setPhrase (3, "Underwater frequency");
        setParam ("speed", 222.0f);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        TSS1Processor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        check (other.getPhrase (3) == "Underwater frequency", "phrase restored");
        check (other.getSelectedSlot() == 6, "selected slot restored");
        check ((int) other.apvts.getParameter ("speed")->convertFrom0to1 (other.apvts.getParameter ("speed")->getValue()) == 222, "speed restored");
        check (other.isMelodic(), "mode restored");
    }
    writeWav (outDir.getChildFile ("demo.wav"), demo, sr);

    // ---- editor snapshots
    {
        setParam ("mode", 0.0f);
        setParam ("range", 50.0f);
        setParam ("speed", 140.0f);
        proc.setPhrase (3, "Signal detected");
        proc.setSelectedSlot (1);
        waitFor (allReady, 20000);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (TSS1Editor::kWidth, TSS1Editor::kHeight);
        auto snap = [&] (const char* name, float scale = 1.0f)
        {
            if (auto* e = dynamic_cast<TSS1Editor*> (ed.get())) e->refreshNow();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            juce::PNGImageFormat png;
            auto f = outDir.getChildFile (name);
            f.deleteFile();
            if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
            check (f.existsAsFile(), juce::String ("snapshot ") + name);
        };
        snap ("editor.png");
        // playing: the word and waveform on the screen, the key lit
        proc.previewSlot (1);
        run ((int) (sr * 0.75 / block));
        snap ("editor_playing.png");
        run ((int) (sr * 2.0 / block));
        setParam ("mode", 1.0f);
        snap ("editor_melodic.png");
        setParam ("mode", 0.0f);
        if (auto* e = dynamic_cast<TSS1Editor*> (ed.get()))
        {
            e->showAbout (true);
            snap ("editor_about.png");
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
