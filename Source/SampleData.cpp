#include "SampleData.h"
#include <cmath>

namespace prisma
{

static void analyse (SampleData& s)
{
    const int n = s.audio.getNumSamples();
    const auto* L = s.audio.getReadPointer (0);
    const auto* R = s.audio.getReadPointer (1);

    // ---- overview + brightness (zero-crossing rate as a cheap brightness proxy)
    const int buckets = 600;
    s.overview.assign ((size_t) buckets, 0.0f);
    s.brightness.assign ((size_t) buckets, 0.0f);
    for (int b = 0; b < buckets; ++b)
    {
        const int a = (int) ((juce::int64) n * b / buckets);
        const int e = juce::jmax (a + 1, (int) ((juce::int64) n * (b + 1) / buckets));
        float peak = 0.0f; int zc = 0; float prev = 0.0f;
        for (int i = a; i < e && i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            peak = juce::jmax (peak, std::abs (m));
            if ((m > 0.0f) != (prev > 0.0f)) ++zc;
            prev = m;
        }
        s.overview[(size_t) b] = peak;
        const float zcr = (float) zc / (float) juce::jmax (1, e - a);
        s.brightness[(size_t) b] = juce::jlimit (0.0f, 1.0f, zcr * 6.0f);
    }

    // ---- onset detection on log energy in ~10 ms frames
    const int hop = juce::jmax (64, (int) (s.sampleRate * 0.01));
    const int frames = n / hop;
    std::vector<float> env ((size_t) juce::jmax (1, frames), 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        double sum = 0.0;
        for (int i = f * hop; i < (f + 1) * hop; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            // emphasise attacks a little with a first difference
            const float d = i > 0 ? m - 0.5f * (L[i - 1] + R[i - 1]) : 0.0f;
            sum += (double) m * m + 4.0 * (double) d * d;
        }
        env[(size_t) f] = (float) std::log10 (1.0e-7 + sum / hop);
    }

    s.onsets.clear();
    s.onsets.push_back (0.0f);
    const int minGap = juce::jmax (1, (int) (0.07 * s.sampleRate / hop));
    int last = -minGap;
    float globalMax = -10.0f;
    for (auto v : env) globalMax = juce::jmax (globalMax, v);

    for (int f = 2; f < frames - 1; ++f)
    {
        const float rise = env[(size_t) f] - juce::jmin (env[(size_t) f - 1], env[(size_t) f - 2]);
        const bool loudEnough = env[(size_t) f] > globalMax - 3.0f; // within 30 dB of the peak
        const bool localMax = env[(size_t) f] >= env[(size_t) f + 1];
        if (rise > 0.45f && loudEnough && localMax && f - last >= minGap)
        {
            // step back to the start of the rise
            int start = f - 1;
            s.onsets.push_back ((float) ((double) start * hop / n));
            last = f;
        }
    }

    // Fallback for very smooth material: evenly spaced slices
    if (s.onsets.size() < 4)
    {
        s.onsets.clear();
        for (int i = 0; i < 16; ++i) s.onsets.push_back ((float) i / 16.0f);
    }
}

SamplePtr loadSample (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable file";
        return nullptr;
    }

    const double sr = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
    const auto maxSamples = (juce::int64) (sr * 90.0); // cap at 90 s
    const int len = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    if (len < 256)
    {
        error = "File is too short";
        return nullptr;
    }

    auto s = std::make_shared<SampleData>();
    s->sampleRate = sr;
    s->path = file.getFullPathName();
    s->name = file.getFileNameWithoutExtension();
    s->audio.setSize (2, len);
    s->audio.clear();

    juce::AudioBuffer<float> tmp ((int) juce::jmax (1u, reader->numChannels), len);
    reader->read (&tmp, 0, len, 0, true, true);
    s->audio.copyFrom (0, 0, tmp, 0, 0, len);
    s->audio.copyFrom (1, 0, tmp, tmp.getNumChannels() > 1 ? 1 : 0, 0, len);

    // Normalise so slots blend at similar levels
    const float peak = s->audio.getMagnitude (0, len);
    if (peak > 1.0e-5f)
        s->audio.applyGain (0.89f / peak);

    analyse (*s);
    return s;
}

} // namespace prisma
