// Offline test for Prisma: builds four synthetic samples, loads them into the
// slots, plays MIDI through every preset and checks the output.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"

static constexpr double SR = 48000.0;
static int failures = 0;

static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;
    if (! ok) ++failures;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr = SR)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

static double hz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }

static juce::AudioBuffer<float> makeVocal (double sr)
{
    const int n = (int) (4.0 * sr);
    juce::AudioBuffer<float> b (1, n);
    double ph = 0.0;
    const int notes[] = { 64, 67, 69, 67, 64, 62, 64, 60 };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const int syl = (int) (t / 0.5);
        const double ts = std::fmod (t, 0.5);
        const double f0 = hz (notes[syl % 8]) * (1.0 + 0.006 * std::sin (t * 2 * juce::MathConstants<double>::pi * 5.5));
        ph += f0 / sr;
        double s = 0.0;
        for (int h = 1; h <= 12; ++h)
        {
            const double fh = f0 * h;
            const double formant = std::exp (-std::pow ((fh - 700.0) / 300.0, 2)) + 0.6 * std::exp (-std::pow ((fh - 1200.0) / 400.0, 2)) + 0.15;
            s += std::sin (2 * juce::MathConstants<double>::pi * ph * h) * formant / h;
        }
        const double env = std::min (1.0, ts * 30.0) * std::exp (-ts * 1.5) * (ts < 0.42 ? 1.0 : std::max (0.0, 1.0 - (ts - 0.42) * 12.5));
        b.setSample (0, i, (float) (s * env * 0.5));
    }
    return b;
}

static juce::AudioBuffer<float> makeKeys (double sr)
{
    const int n = (int) (4.0 * sr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 48, 52, 55 }, { 55, 59, 62 } };
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const int c = (int) (t / 1.0) % 4;
        const double tc = std::fmod (t, 1.0);
        double s = 0.0;
        for (int v = 0; v < 3; ++v)
        {
            const double f = hz (chords[c][v]);
            s += (std::sin (2 * juce::MathConstants<double>::pi * f * t) + 0.3 * std::sin (4 * juce::MathConstants<double>::pi * f * t)) * std::exp (-tc * 2.5);
        }
        b.setSample (0, i, (float) (s * 0.25));
        b.setSample (1, i, (float) (s * 0.23));
    }
    return b;
}

static juce::AudioBuffer<float> makeDrums (double sr)
{
    const int n = (int) (4.0 * sr);
    juce::AudioBuffer<float> b (2, n);
    juce::Random r (5);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double beat = 0.5, tb = std::fmod (t, beat);
        const int bi = (int) (t / beat);
        double s = 0.0;
        if (bi % 2 == 0) s += std::sin (2 * juce::MathConstants<double>::pi * (50.0 * tb + 60.0 * (1 - std::exp (-tb * 30)) / 30.0)) * std::exp (-tb * 9.0);
        if (bi % 2 == 1) s += (r.nextFloat() * 2 - 1) * 0.6 * std::exp (-tb * 14.0);
        const double th = std::fmod (t, beat / 2);
        s += (r.nextFloat() * 2 - 1) * 0.15 * std::exp (-th * 60.0);
        b.setSample (0, i, (float) s);
        b.setSample (1, i, (float) s);
    }
    return b;
}

static juce::AudioBuffer<float> makePad (double sr)
{
    const int n = (int) (6.0 * sr);
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        double l = 0.0, rr = 0.0;
        const int notes[] = { 50, 57, 62, 65, 69 };
        for (int k = 0; k < 5; ++k)
        {
            const double f = hz (notes[k]);
            l += std::sin (2 * juce::MathConstants<double>::pi * f * 1.003 * t + k);
            rr += std::sin (2 * juce::MathConstants<double>::pi * f * 0.997 * t + 2 * k);
        }
        const double env = 0.5 + 0.5 * std::sin (t * 1.3);
        b.setSample (0, i, (float) (l * 0.15 * env));
        b.setSample (1, i, (float) (rr * 0.15 * env));
    }
    return b;
}

static juce::AudioBuffer<float> makeSine (double sr)
{
    const int n = (int) (4.0 * sr);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i) b.setSample (0, i, (float) (0.8 * std::sin (2 * juce::MathConstants<double>::pi * 220.0 * i / sr)));
    return b;
}

struct Stats { float peak = 0, rms = 0; bool finite = true; double cpuMsPerSec = 0; };

// Plays a list of (time, note, on/off) events and renders `seconds`.
static Stats render (PrismaProcessor& p, std::vector<std::tuple<double, int, bool>> events, double seconds,
                     juce::AudioBuffer<float>& out, bool randomBlocks = false)
{
    p.setPlayConfigDetails (0, 2, SR, 1024);
    p.prepareToPlay (SR, 1024);
    const int total = (int) (seconds * SR);
    out.setSize (2, total);
    out.clear();
    juce::Random r (3);
    int pos = 0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    while (pos < total)
    {
        const int bs = juce::jmin (randomBlocks ? 1 + r.nextInt (1024) : 512, total - pos);
        juce::AudioBuffer<float> block (2, bs);
        juce::MidiBuffer midi;
        for (auto& [t, note, on] : events)
        {
            const int s = (int) (t * SR);
            if (s >= pos && s < pos + bs)
                midi.addEvent (on ? juce::MidiMessage::noteOn (1, note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, note), s - pos);
        }
        p.processBlock (block, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, block, c, 0, bs);
        pos += bs;
    }
    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    Stats st;
    st.cpuMsPerSec = (t1 - t0) / seconds;
    double sum = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < total; ++i)
        {
            const float v = out.getSample (c, i);
            if (! std::isfinite (v)) st.finite = false;
            st.peak = std::max (st.peak, std::abs (v));
            sum += v * v;
        }
    st.rms = (float) std::sqrt (sum / (2.0 * total));
    return st;
}

static double maxSecondDiff (const juce::AudioBuffer<float>& b, int from, int to)
{
    double m = 0;
    for (int i = from + 1; i < to - 1; ++i)
        m = std::max (m, (double) std::abs (b.getSample (0, i + 1) - 2 * b.getSample (0, i) + b.getSample (0, i - 1)));
    return m;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    auto sampleDir = outDir.getChildFile ("samples");
    sampleDir.createDirectory();

    writeWav (sampleDir.getChildFile ("vocal.wav"), makeVocal (44100.0), 44100.0);   // different rate on purpose
    writeWav (sampleDir.getChildFile ("keys.wav"), makeKeys (SR));
    writeWav (sampleDir.getChildFile ("drums.wav"), makeDrums (SR));
    writeWav (sampleDir.getChildFile ("pad.wav"), makePad (SR));
    writeWav (sampleDir.getChildFile ("sine.wav"), makeSine (SR));
    const char* names[] = { "vocal.wav", "keys.wav", "drums.wav", "pad.wav" };

    auto loadAll = [&] (PrismaProcessor& p)
    {
        for (int i = 0; i < 4; ++i)
        {
            juce::String err;
            const bool ok = p.loadSlot (i, sampleDir.getChildFile (names[i]), err);
            if (! ok) { std::cout << "load failed: " << err << std::endl; ++failures; }
        }
    };

    // ---- snapshot of the UI
    if (std::getenv ("PRISMA_SNAPSHOT") != nullptr)
    {
        PrismaProcessor p;
        loadAll (p);
        p.applyPreset (1);
        p.setParam ("morphX", 0.3f); p.setParam ("morphY", 0.2f);
        juce::AudioBuffer<float> out;
        render (p, { { 0.0, 60, true } }, 1.0, out);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("ui.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::cout << "snapshot written" << std::endl;
        return 0;
    }

    // ---- analysis
    {
        juce::String err;
        auto d = prisma::loadSample (sampleDir.getChildFile ("drums.wav"), err);
        std::cout << "Drum onsets found: " << d->onsets.size() << " (expect ~16-24)" << std::endl;
        check (d->onsets.size() >= 8 && d->onsets.size() <= 40, "onset detection on a drum loop");
        auto v = prisma::loadSample (sampleDir.getChildFile ("vocal.wav"), err);
        std::cout << "Vocal onsets found: " << v->onsets.size() << " (expect ~8)" << std::endl;
        check (v->onsets.size() >= 4, "onset detection on a sung phrase");
    }

    // ---- silence with no samples
    {
        PrismaProcessor p;
        juce::AudioBuffer<float> out;
        const auto s = render (p, { { 0.0, 60, true } }, 1.0, out);
        check (s.finite && s.peak < 1.0e-4f, "empty slots stay silent");
    }

    const std::vector<std::tuple<double, int, bool>> phrase = {
        { 0.0, 60, true }, { 2.0, 60, false }, { 2.0, 63, true }, { 2.0, 67, true },
        { 4.0, 63, false }, { 4.0, 67, false }, { 4.2, 55, true }, { 6.0, 55, false } };

    // ---- every preset, with different morph positions
    const auto presets = prisma::presetNames();
    const float morphs[][2] = { { 0.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 0.0f }, { 0.3f, 0.8f } };
    for (int pi = 0; pi < presets.size(); ++pi)
    {
        PrismaProcessor p;
        loadAll (p);
        p.applyPreset (pi);
        p.setParam ("morphX", morphs[pi % 4][0]);
        p.setParam ("morphY", morphs[pi % 4][1]);
        std::vector<std::tuple<double, int, bool>> ev = phrase;
        if (presets[pi] == "Slice Keys")
        {
            ev.clear();
            for (int k = 0; k < 16; ++k) { ev.push_back ({ k * 0.375, 36 + k, true }); ev.push_back ({ k * 0.375 + 0.3, 36 + k, false }); }
        }
        juce::AudioBuffer<float> out;
        const auto s = render (p, ev, 8.0, out);
        std::cout << "Preset '" << presets[pi] << "': peak " << s.peak << ", rms " << s.rms
                  << ", CPU " << juce::String (s.cpuMsPerSec, 1) << " ms/s" << std::endl;
        check (s.finite, presets[pi] + ": no NaN");
        check (s.peak <= 1.0f, presets[pi] + ": below 0 dBFS");
        check (s.rms > 0.01f, presets[pi] + ": makes sound");
        // tail must die out after the last note (2 s after release plus reverb)
        writeWav (outDir.getChildFile (juce::String (pi + 1).paddedLeft ('0', 2) + "_" + presets[pi].replaceCharacters (" ", "_") + ".wav"), out);
    }

    // ---- click check with a pure sine: chops must be faded
    {
        PrismaProcessor p;
        juce::String err;
        p.loadSlot (0, sampleDir.getChildFile ("sine.wav"), err);
        p.applyPreset (0);
        p.setParam ("range", 0.8f); p.setParam ("snap", 0.0f); p.setParam ("rate", 5); p.setParam ("length", 0.6f);
        juce::AudioBuffer<float> out;
        render (p, { { 0.0, 60, true }, { 3.0, 60, false } }, 3.5, out);
        const double d2 = maxSecondDiff (out, 2000, (int) (2.9 * SR));
        std::cout << "Max 2nd difference on chopped sine: " << d2 << " (natural ~0.0015)" << std::endl;
        check (d2 < 0.02, "no clicks between chops");
        writeWav (outDir.getChildFile ("zz_sine_chop.wav"), out);
    }

    // ---- dice + random block sizes + all slot-random combos
    {
        PrismaProcessor p;
        loadAll (p);
        juce::AudioBuffer<float> out;
        for (int k = 0; k < 8; ++k)
        {
            p.dice();
            p.setParam ("slotRandom", (float) (k % 2));
            p.setParam ("morphX", (float) (k % 3) / 2.0f);
            p.setParam ("crunch", (float) (k % 4) / 3.0f);
            p.setParam ("wash", (float) ((k + 1) % 4) / 3.0f);
            p.setParam ("pump", (float) ((k + 2) % 4) / 3.0f);
            const auto s = render (p, phrase, 6.5, out, true);
            check (s.finite && s.peak <= 1.0f && s.rms > 0.005f, "dice run " + juce::String (k) + " (peak " + juce::String (s.peak, 2)
                                                                 + ", CPU " + juce::String (s.cpuMsPerSec, 1) + " ms/s)");
            if (k == 0) writeWav (outDir.getChildFile ("zz_dice.wav"), out);
        }
    }

    // ---- 8-note chord stress (polyphony + cloud)
    {
        PrismaProcessor p;
        loadAll (p);
        p.applyPreset (2);
        p.setParam ("density", 80.0f);
        p.setParam ("morphX", 0.5f); p.setParam ("morphY", 0.5f);
        std::vector<std::tuple<double, int, bool>> ev;
        for (int k = 0; k < 10; ++k) ev.push_back ({ 0.05 * k, 48 + k * 3, true });
        juce::AudioBuffer<float> out;
        const auto s = render (p, ev, 4.0, out);
        std::cout << "Stress: CPU " << s.cpuMsPerSec << " ms per second of audio" << std::endl;
        check (s.finite && s.peak <= 1.0f, "10 held notes with dense cloud");
        check (s.cpuMsPerSec < 400.0, "stress CPU acceptable");
    }

    // ---- state save / load including slot files, and settings text
    {
        PrismaProcessor a, b;
        loadAll (a);
        a.applyPreset (3);
        a.setParam ("morphX", 0.7f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        for (auto* prm : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (std::abs (rp->getValue() - b.apvts.getParameter (rp->getParameterID())->getValue()) > 1e-5f) same = false;
        check (same, "parameters restored");
        bool slotsOk = true;
        for (int i = 0; i < 4; ++i) slotsOk &= b.getSlot (i) != nullptr && b.getSlot (i)->name == a.getSlot (i)->name;
        check (slotsOk, "sample slots restored from saved paths");

        PrismaProcessor c;
        const auto text = a.settingsToText();
        const int applied = c.settingsFromText (text);
        bool same2 = true;
        for (auto* prm : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (std::abs (rp->getValue() - c.apvts.getParameter (rp->getParameterID())->getValue()) > 1e-3f) same2 = false;
        check (applied >= 20 && same2, "copy/paste settings round trip (" + juce::String (applied) + " values)");
        if (std::getenv ("PRISMA_PRINT_SETTINGS")) std::cout << text << std::endl;
    }

    // ---- slot swap while playing (thread-safety smoke test)
    {
        PrismaProcessor p;
        loadAll (p);
        p.setPlayConfigDetails (0, 2, SR, 512);
        p.prepareToPlay (SR, 512);
        juce::AudioBuffer<float> block (2, 512);
        bool ok = true;
        for (int k = 0; k < 400; ++k)
        {
            juce::MidiBuffer m;
            if (k == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            p.processBlock (block, m);
            if (k % 50 == 25) { juce::String e; p.loadSlot (k % 4, sampleDir.getChildFile (names[(k / 50) % 4]), e); }
            if (k % 50 == 40) p.clearSlot ((k / 50) % 4);
            p.releaseRetired();
            for (int i = 0; i < 512; ++i) if (! std::isfinite (block.getSample (0, i))) ok = false;
        }
        check (ok, "loading and clearing slots while playing");
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
