#include "SpeechEngine.h"
#include "Presets.h"
#include "Resources.h"
#include <algorithm>
#include <cstring>
#include <espeak-ng/speak_lib.h>

namespace
{
    // the folder name changes whenever the built-in voice data changes, so updates re-extract it
    constexpr const char* kDataVersion = "espeak-ng-1.52.0-r1";

    // eSpeak calls back with audio while espeak_Synth runs; we collect into this
    struct Phoneme { int startSample; bool vowel, pause; };
    struct Collector
    {
        std::vector<short> samples;
        std::vector<SpeechClip::Word> words;
        std::vector<Phoneme> phonemes;
        int sampleRate = 22050;
    };

    // eSpeak phoneme mnemonics: vowels and diphthongs start with one of these (after any stress mark)
    bool isVowelPhoneme (const char* name)
    {
        while (*name == '\'' || *name == ',' || *name == '%' || *name == '=') ++name;
        return *name != 0 && std::strchr ("aeiouAEIOUV03@", *name) != nullptr;
    }

    // Splits the phrase into syllables: every word starts one, and inside a word each vowel after
    // the first starts one at the consonant just before it ("tran-zis-t3", "hju:-m@n").
    std::vector<int> findSyllables (const std::vector<Phoneme>& ph, const std::vector<SpeechClip::Word>& words)
    {
        std::vector<int> out;
        size_t w = 0;
        bool vowelInSyllable = false;
        int lastConsonant = -1;
        for (size_t i = 0; i < ph.size(); ++i)
        {
            const auto& p = ph[i];
            const bool newWord = w < words.size() && p.startSample >= words[w].startSample;
            if (newWord)
            {
                while (w < words.size() && p.startSample >= words[w].startSample) ++w;
                if (p.pause) { vowelInSyllable = false; lastConsonant = -1; continue; }
                out.push_back (p.startSample);
                vowelInSyllable = p.vowel;
                lastConsonant = p.vowel ? -1 : p.startSample;
                continue;
            }
            if (p.pause) continue;
            if (p.vowel)
            {
                if (vowelInSyllable)
                    out.push_back (lastConsonant >= 0 ? lastConsonant : p.startSample);
                vowelInSyllable = true;
                lastConsonant = -1;
            }
            else lastConsonant = p.startSample;
        }
        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end()), out.end());
        return out;
    }
    thread_local Collector* gCollector = nullptr;

    int synthCallback (short* wav, int numSamples, espeak_EVENT* events)
    {
        auto* c = gCollector;
        if (c == nullptr) return 1;
        if (wav != nullptr && numSamples > 0)
            c->samples.insert (c->samples.end(), wav, wav + numSamples);
        for (auto* e = events; e != nullptr && e->type != espeakEVENT_LIST_TERMINATED; ++e)
        {
            const int pos = (int) ((juce::int64) e->audio_position * c->sampleRate / 1000);
            if (e->type == espeakEVENT_WORD)
                c->words.push_back ({ pos, juce::jmax (0, e->text_position - 1), e->length });
            else if (e->type == espeakEVENT_PHONEME)
            {
                char name[9] = {};
                std::memcpy (name, e->id.string, 8);
                c->phonemes.push_back ({ pos, isVowelPhoneme (name), name[0] == '_' });
            }
        }
        return 0;
    }
}

juce::uint64 VoiceSettings::hash (const juce::String& text) const noexcept
{
    auto h = (juce::uint64) text.hashCode64();
    for (int v : { voice, speed, pitch, range, gap, amplitude })
        h = (h ^ ((juce::uint64) (juce::int64) v + 0x9e3779b9ull)) * 1099511628211ull;
    return h;
}

EspeakService& EspeakService::get()
{
    static EspeakService instance;
    return instance;
}

juce::String EspeakService::getError() const
{
    const juce::ScopedLock sl (lock);
    return error;
}

juce::File EspeakService::dataFolder() const
{
   #if JUCE_WINDOWS
    // eSpeak opens files with plain fopen(), which fails on paths with non-ASCII characters, and user
    // names often have them. ProgramData is ASCII and writable by every user.
    auto base = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return base.getChildFile ("Transistor Labs").getChildFile ("TSS-1").getChildFile (kDataVersion);
}

bool EspeakService::extractData (const juce::File& root)
{
    auto marker = root.getChildFile ("installed.txt");
    if (marker.existsAsFile() && root.getChildFile ("espeak-ng-data").getChildFile ("phondata").existsAsFile())
        return true;

    int zipSize = 0;
    const char* zipData = getResource ("espeak-ng-data.zip", zipSize);
    if (zipData == nullptr) { error = "voice data missing from the plug-in"; return false; }
    juce::MemoryInputStream zipStream (zipData, (size_t) zipSize, false);
    juce::ZipFile zip (zipStream);
    if (zip.getNumEntries() == 0) { error = "voice data missing from the plug-in"; return false; }

    // unzip next to the final folder, then move it into place (another instance may be doing the same)
    auto tmp = root.getSiblingFile (root.getFileName() + "-tmp-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
    if (! tmp.createDirectory()) { error = "can't write " + tmp.getFullPathName(); return false; }
    auto result = zip.uncompressTo (tmp, true);
    if (result.failed()) { tmp.deleteRecursively(); error = "can't unpack voice data: " + result.getErrorMessage(); return false; }
    tmp.getChildFile ("installed.txt").replaceWithText (kDataVersion);

    if (! root.isDirectory())
        tmp.moveFileTo (root);
    tmp.deleteRecursively();
    return root.getChildFile ("espeak-ng-data").getChildFile ("phondata").existsAsFile();
}

bool EspeakService::ensureReady()
{
    if (ready.load()) return true;
    const juce::ScopedLock sl (lock);
    if (ready.load()) return true;
    if (failed) return false;

    auto root = dataFolder();
    root.getParentDirectory().createDirectory();
    if (! extractData (root))
    {
        // last resort: the temp folder
        auto alt = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TSS-1").getChildFile (kDataVersion);
        alt.getParentDirectory().createDirectory();
        if (! extractData (alt)) { failed = true; return false; }
        root = alt;
    }

    const auto path = root.getFullPathName();
    const int rate = espeak_Initialize (AUDIO_OUTPUT_SYNCHRONOUS, 0, path.toRawUTF8(), espeakINITIALIZE_DONT_EXIT | espeakINITIALIZE_PHONEME_EVENTS);
    if (rate <= 0) { failed = true; error = "speech engine failed to start"; return false; }
    espeak_SetSynthCallback (synthCallback);
    error.clear();
    ready = true;
    return true;
}

void EspeakService::applyVoice (const VoiceSettings& s, int pitch)
{
    const auto& p = kVoicePresets[juce::jlimit (0, kNumVoices - 1, s.voice)];
    const auto name = juce::String (p.language) + "+" + p.variant;
    if (espeak_SetVoiceByName (name.toRawUTF8()) != EE_OK)
        espeak_SetVoiceByName (p.language);   // variant missing: plain language voice
    espeak_SetParameter (espeakRATE, juce::jlimit (80, 450, s.speed), 0);
    espeak_SetParameter (espeakPITCH, juce::jlimit (0, 99, pitch), 0);
    espeak_SetParameter (espeakRANGE, juce::jlimit (0, 99, s.range), 0);
    espeak_SetParameter (espeakVOLUME, juce::jlimit (0, 200, s.amplitude), 0);
    espeak_SetParameter (espeakWORDGAP, juce::jlimit (0, 100, s.gap), 0);
    espeak_SetParameter (espeakPUNCTUATION, espeakPUNCT_NONE, 0);
}

SpeechClip::Ptr EspeakService::render (const juce::String& text, const VoiceSettings& s, int pitchOverride)
{
    if (! ensureReady()) return nullptr;

    Collector c;
    {
        const juce::ScopedLock sl (lock);
        applyVoice (s, pitchOverride >= 0 ? pitchOverride : s.pitch);
        gCollector = &c;
        const auto utf8 = text.toUTF8();
        espeak_Synth (utf8.getAddress(), std::strlen (utf8.getAddress()) + 1, 0, POS_CHARACTER, 0,
                      espeakCHARS_UTF8, nullptr, nullptr);
        espeak_Synchronize();
        gCollector = nullptr;
    }

    SpeechClip::Ptr clip = new SpeechClip();
    clip->text = text;
    clip->sampleRate = c.sampleRate;
    clip->key = s.hash (text);

    // trim the silence eSpeak leaves at the end (keep a short tail)
    int n = (int) c.samples.size();
    while (n > 0 && std::abs (c.samples[(size_t) n - 1]) < 8) --n;
    n = juce::jmin ((int) c.samples.size(), n + c.sampleRate / 50);

    clip->audio.setSize (1, juce::jmax (1, n));
    clip->audio.clear();
    auto* d = clip->audio.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        d[i] = (float) c.samples[(size_t) i] * (1.0f / 32768.0f);
    clip->syllables = findSyllables (c.phonemes, c.words);
    clip->syllables.erase (std::remove_if (clip->syllables.begin(), clip->syllables.end(), [n] (int x) { return x >= n - 1; }),
                           clip->syllables.end());
    if (clip->syllables.empty()) clip->syllables.push_back (0);
    clip->syllables.front() = 0;   // the first syllable starts with the clip
    clip->words = std::move (c.words);
    clip->f0 = measureF0 (d, n, clip->sampleRate);
    return clip;
}

int EspeakService::pitchForFrequency (int voice, int range, double hz, double& expectedHz)
{
    juce::ignoreUnused (range);
    Calibration cal;
    {
        const juce::ScopedLock sl (lock);
        auto it = calibrations.find (voice);
        if (it != calibrations.end()) cal = it->second;
    }
    if (cal.f20 <= 0)
    {
        // measure the voice once: a flat vowel at pitch 20 and at pitch 80
        VoiceSettings s;
        s.voice = voice; s.speed = 120; s.range = 0; s.gap = 0; s.amplitude = 100;
        auto a = render ("aaa mama aaa", s, 20);
        auto b = render ("aaa mama aaa", s, 80);
        cal.f20 = (a != nullptr && a->f0 > 0) ? a->f0 : 80.0;
        cal.f80 = (b != nullptr && b->f0 > cal.f20) ? b->f0 : cal.f20 * 1.9;
        const juce::ScopedLock sl (lock);
        calibrations[voice] = cal;
    }
    // eSpeak's pitch scale is close to exponential in Hz
    const double perStep = std::log (cal.f80 / cal.f20) / 60.0;
    const int p = juce::jlimit (0, 99, juce::roundToInt (20.0 + std::log (hz / cal.f20) / perStep));
    expectedHz = cal.f20 * std::exp ((p - 20) * perStep);
    return p;
}

double EspeakService::measureF0 (const float* data, int n, double sr)
{
    // median of YIN pitch estimates over the voiced frames (55 - 500 Hz); YIN picks the first
    // clear period, which avoids the octave errors plain autocorrelation makes on buzzy robot voices
    const int win = (int) (sr * 0.03);
    const int minLag = (int) (sr / 500.0), maxLag = (int) (sr / 55.0);
    std::vector<double> est, diff ((size_t) maxLag + 2);
    for (int s = 0; s + win + maxLag + 2 < n; s += win / 2)
    {
        double e = 0;
        for (int i = 0; i < win; ++i) e += (double) data[s + i] * data[s + i];
        if (e / win < 1.0e-4) continue;
        double running = 0;
        int found = -1;
        for (int lag = 1; lag <= maxLag + 1; ++lag)
        {
            double d = 0;
            for (int i = 0; i < win; ++i) { const double x = (double) data[s + i] - data[s + i + lag]; d += x * x; }
            running += d;
            diff[(size_t) lag] = running > 0 ? d * lag / running : 1.0;   // cumulative mean normalised difference
        }
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (diff[(size_t) lag] < 0.2)
            {
                while (lag + 1 <= maxLag && diff[(size_t) lag + 1] < diff[(size_t) lag]) ++lag;
                found = lag;
                break;
            }
        if (found > 0)
        {
            // parabolic interpolation for sub-sample accuracy
            const double a = diff[(size_t) found - 1], b = diff[(size_t) found], c = diff[(size_t) found + 1];
            const double den = a - 2 * b + c;
            const double lag = found + (std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
            est.push_back (sr / lag);
        }
    }
    if (est.empty()) return 0.0;
    std::nth_element (est.begin(), est.begin() + (long) est.size() / 2, est.end());
    return est[est.size() / 2];
}
