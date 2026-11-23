#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <vector>

// User divisions of a phrase for the Syllable and Sync modes.
//
// In the text: "Trans|is|tor speech" - every | is a cut and is not spoken. With dividers in a phrase,
// only the user's cuts are used. On the screen: cut markers can be added, moved and removed; those
// edits are kept per slot as a CutList and override the automatic or text divisions.

namespace cuts
{
    // removes the | dividers; positions get the index in the cleaned text where each one was
    inline juce::String stripDividers (const juce::String& text, std::vector<int>* positions = nullptr)
    {
        juce::String out;
        out.preallocateBytes ((size_t) text.getNumBytesAsUTF8());
        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            if (c == '|') { if (positions != nullptr) positions->push_back (out.length()); }
            else out += juce::String::charToString (c);
        }
        return out;
    }

    // the quietest point within +/- radius samples of pos (so a cut does not click)
    inline int snapToQuiet (const float* x, int n, int pos, int radius)
    {
        const int win = juce::jmax (8, radius / 6);
        int best = pos;
        double bestE = 1.0e30;
        for (int p = juce::jmax (0, pos - radius); p <= juce::jmin (n - 1, pos + radius); p += juce::jmax (1, win / 4))
        {
            double e = 0;
            for (int k = juce::jmax (0, p - win / 2); k < juce::jmin (n, p + win / 2); ++k) e += (double) x[k] * x[k];
            e *= 1.0 + 0.3 * std::abs (p - pos) / (double) juce::jmax (1, radius);   // prefer close points
            if (e < bestE) { bestE = e; best = p; }
        }
        return best;
    }

    // tidies a cut list: sorted, starting at 0, inside the clip, at least minGap apart
    inline std::vector<int> tidy (std::vector<int> c, int numSamples, int minGap)
    {
        c.push_back (0);
        std::sort (c.begin(), c.end());
        std::vector<int> out;
        for (int v : c)
        {
            v = juce::jlimit (0, juce::jmax (0, numSamples - 2), v);
            if (out.empty() || v - out.back() >= minGap) out.push_back (v);
        }
        if (out.empty() || out.front() != 0) out.insert (out.begin(), 0);
        return out;
    }

    // where the | dividers fall in the audio: between two words the cut is at the start of the next word;
    // inside a word it is the syllable boundary nearest to the divider's place in the spelling
    template <class Clip>
    std::vector<int> fromDividers (const Clip& clip, const std::vector<int>& positions)
    {
        std::vector<int> out { 0 };
        const int n = clip.numSamples();
        const auto& words = clip.words;
        if (words.empty()) return out;
        const float* x = clip.audio.getReadPointer (0);
        const int radius = (int) (clip.sampleRate * 0.025);
        for (int pos : positions)
        {
            for (size_t w = 0; w < words.size(); ++w)
            {
                const auto& wd = words[w];
                const int end = w + 1 < words.size() ? words[w + 1].startSample : n;
                if (pos <= wd.textStart)           // before this word: cut at its start
                {
                    if (w > 0 || pos == wd.textStart) out.push_back (wd.startSample);
                    break;
                }
                if (pos < wd.textStart + wd.textLength)
                {
                    const double f = (pos - wd.textStart) / (double) juce::jmax (1, wd.textLength);
                    const int target = wd.startSample + (int) (f * (end - wd.startSample));
                    int best = -1;
                    for (int s : clip.syllables)
                        if (s > wd.startSample && s < end && (best < 0 || std::abs (s - target) < std::abs (best - target))) best = s;
                    // a syllable boundary near the spelling position, otherwise the quietest point nearby
                    if (best < 0 || std::abs (best - target) > (end - wd.startSample) / 3) best = snapToQuiet (x, n, target, radius);
                    out.push_back (best);
                    break;
                }
            }
        }
        return tidy (out, n, (int) (clip.sampleRate * 0.02));
    }
}
