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

    // ---- every voice preset: speaks, and sounds different from the others
    std::vector<std::pair<double, double>> prints;   // (pitch, brightness) per preset
    for (int v = 0; v < kNumVoices; ++v)
    {
        {
            VoiceSettings vs; const auto& pr = kVoicePresets[v];
            vs.voice = v; vs.speed = 140; vs.pitch = 40; vs.range = 30; vs.gap = 5; vs.amplitude = 100;   // same settings: only the voice differs
            auto c = EspeakService::get().render ("Hello human, systems online", vs);
            double zc = 0; const float* d = c->audio.getReadPointer (0);
            for (int i = 1; i < c->numSamples(); ++i) if ((d[i] >= 0) != (d[i - 1] >= 0)) zc += 1;
            prints.push_back ({ c->f0, zc / c->numSamples() * 22050.0 });
            std::printf ("      %-18s %-11s %-10s f0 %6.1f Hz  brightness %6.0f\n", pr.name, pr.language, pr.variant, c->f0, prints.back().second);
        }
        proc.loadVoicePreset (v);
        const bool ok = waitFor ([&] { return proc.getSlotStatus (0) == TSS1Processor::SlotStatus::ready; }, 10000);
        auto m = note (36);
        run ((int) (sr * 2.8 / block), &m, v % 4 == 0 ? &demo : nullptr);
        check (ok && rms() > 0.01 && finite() && peak() <= 1.0f,
               juce::String ("voice ") + kVoicePresets[v].name + " (rms " + juce::String (rms(), 3) + ", peak " + juce::String (peak(), 2) + ")");
    }
    {
        int similar = 0;
        for (size_t a = 0; a < prints.size(); ++a)
            for (size_t b = a + 1; b < prints.size(); ++b)
                if (std::abs (prints[a].first - prints[b].first) < 1.0 && std::abs (prints[a].second - prints[b].second) < 15.0) ++similar;
        check (similar == 0, "all 26 voices sound different (" + juce::String (similar) + " near-identical pairs)");
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

    // ---- sync: one syllable per 1/8 at 120 BPM (no host here, so the free-running clock is used)
    {
        auto c = proc.getClip (0);
        const int n = c != nullptr ? (int) c->syllables.size() : 0;
        check (n >= 6 && n <= 12, "slot 1 split into " + juce::String (n) + " syllables");
        setParam ("sync", 2.0f);
        auto m = note (36);
        run ((int) (sr * 6.0 / block), &m);
        int last = 0;
        for (int i = 0; i < (int) rec.size(); ++i) if (std::abs (rec[(size_t) i]) > 0.01f) last = i;
        const double T = last / sr, step = 0.25;
        check (T > (n - 1) * step - 0.01 && T < n * step + 0.06, "sync 1/8: phrase spread over " + juce::String (T, 2) + " s for " + juce::String (n) + " steps");
        writeWav (outDir.getChildFile ("sync_eighths.wav"), rec, sr);
        demo.insert (demo.end(), rec.begin(), rec.end());
        // gate: loops while the key is held
        setParam ("gate", 1.0f);
        auto m2 = note (36);
        run ((int) (sr * 5.0 / block), &m2);
        double e = 0; const size_t from = rec.size() - (size_t) sr;
        for (size_t i = from; i < rec.size(); ++i) e += (double) rec[i] * rec[i];
        check (std::sqrt (e / sr) > 0.005, "sync + gate keeps looping while held");
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        run (20, &off); run (10);
        check (rms() < 0.0005, "sync + gate stops on release");
        setParam ("gate", 0.0f);
        setParam ("sync", 0.0f);
    }

    // ---- syllable mode: each key says one syllable of the selected phrase
    {
        proc.setSelectedSlot (0);
        setParam ("mode", 2.0f);
        auto c = proc.getClip (0);
        const int n = c != nullptr ? (int) c->syllables.size() : 0;
        std::vector<float> chops;
        bool allShort = true, allSound = true;
        for (int k = 0; k < n; ++k)
        {
            auto m = note (36 + k);
            run ((int) (sr * 0.7 / block), &m, &chops);
            int first = -1, last = 0;
            for (int i = 0; i < (int) rec.size(); ++i) if (std::abs (rec[(size_t) i]) > 0.005f) { if (first < 0) first = i; last = i; }
            const int segLen = (k + 1 < n ? c->syllables[(size_t) k + 1] : c->numSamples()) - c->syllables[(size_t) k];
            const double expected = segLen / 22050.0;
            if (first < 0) allSound = false;
            else if ((last - first) / sr > expected + 0.03) allShort = false;
        }
        check (n > 0 && allSound, "syllable mode: all " + juce::String (n) + " keys speak");
        check (allShort, "syllable mode: each key stops at the end of its syllable");
        auto m = note (36 + n);
        run ((int) (sr * 0.5 / block), &m);
        check (rms() < 0.0005, "syllable mode: the key after the last syllable is silent");
        writeWav (outDir.getChildFile ("syllables.wav"), chops, sr);
        demo.insert (demo.end(), chops.begin(), chops.end());
        setParam ("mode", 0.0f);
    }

    // ---- the user's own cuts: | dividers in the text and markers on the screen
    {
        const auto old = proc.getPhrase (0);
        proc.setSelectedSlot (0);
        proc.setPhrase (0, "Trans|is|tor speech");
        juce::Thread::sleep (300);
        waitFor (allReady, 120000);
        auto c = proc.getClip (0);
        check (c != nullptr && c->syllables.size() == 3, "| dividers: 3 pieces (" + juce::String (c ? (int) c->syllables.size() : -1) + ")");
        check (c != nullptr && ! c->text.containsChar ('|'), "dividers are not spoken");
        const int n = c != nullptr ? c->numSamples() : 0;
        proc.setCuts (0, { 0, n / 4, n / 2, 3 * n / 4 });
        bool custom = false;
        check (proc.getCuts (0, &custom).size() == 4 && custom, "4 cut markers set");
        setParam ("mode", 2.0f);
        int sounding = 0;
        for (int k = 0; k < 4; ++k) { auto m = note (36 + k); run ((int) (sr * 0.5 / block), &m); if (rms() > 0.002) ++sounding; }
        auto m5 = note (40); run ((int) (sr * 0.4 / block), &m5);
        check (sounding == 4 && rms() < 0.0005, "syllable mode follows the markers (4 keys, the 5th silent)");
        proc.previewSyllable (0, 2);
        run ((int) (sr * 0.4 / block));
        check (rms() > 0.002, "a single piece can be previewed");
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        TSS1Processor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        waitFor ([&] { return other.getSlotStatus (0) == TSS1Processor::SlotStatus::ready; }, 120000);
        bool c2 = false;
        check (other.getCuts (0, &c2).size() == 4 && c2, "markers saved with the project");
        proc.resetCuts (0);
        check (proc.getCuts (0, &custom).size() == 3 && ! custom, "reset goes back to the | divisions");
        setParam ("mode", 0.0f);
        proc.setPhrase (0, old);
        juce::Thread::sleep (300);
        waitFor (allReady, 120000);
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
        {
            // nothing may take the keyboard when the window is activated (Live's computer MIDI keyboard must keep working)
            std::function<int (juce::Component&)> count = [&] (juce::Component& c)
            {
                int n = (c.getWantsKeyboardFocus() && c.isVisible()) ? 1 : 0;
                for (auto* ch : c.getChildren()) n += count (*ch);
                return n;
            };
            check (count (*ed) == 0, "no component wants the keyboard until the phrase box is clicked");
        }
        snap ("editor.png");
        snap ("editor@2x.png", 2.0f);
        // playing: the word and waveform on the screen, the key lit
        proc.previewSlot (1);
        run ((int) (sr * 0.75 / block));
        snap ("editor_playing.png");
        run ((int) (sr * 2.0 / block));
        setParam ("mode", 1.0f);
        snap ("editor_melodic.png");
        setParam ("mode", 2.0f);
        snap ("editor_syllable.png");
        snap ("editor_syllable@2x.png", 2.0f);
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
