#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SampleData.h"
#include <array>
#include <atomic>
#include <cmath>

namespace prisma
{

constexpr int numSlots = 4;
constexpr int numVoices = 8;

enum PlayMode    { ModeOffset = 0, ModeSlices };
enum OffsetShape { ShapeRandom = 0, ShapeRamp, ShapeSine, ShapeDrift };

inline juce::StringArray rateNames()      { return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
inline double rateDivisions (int r)       { static const double d[] = { 1, 2, 3, 4, 6, 8 }; return d[juce::jlimit (0, 5, r)]; }
inline juce::StringArray shapeNames()     { return { "Random", "Ramp", "Sine", "Drift" }; }
inline juce::StringArray pitchSetNames()  { return { "Octaves", "Fifths", "Minor", "Major", "Pentatonic", "Chaos" }; }
inline juce::StringArray playModeNames()  { return { "Offset", "Slices" }; }

inline const std::vector<int>& pitchSet (int i)
{
    static const std::vector<int> sets[] = {
        { 12, -12 },
        { 7, 12, -5, -12 },
        { 3, 7, 10, 12, -2, -5 },
        { 4, 7, 11, 12, -1, -5 },
        { 2, 4, 7, 9, 12, -3, -5 },
        { 1, 6, 11, -7, 13, -13 }
    };
    return sets[juce::jlimit (0, 5, i)];
}

// Everything the engine needs for one block (read from parameters once).
struct Params
{
    int playMode = ModeOffset;
    float position = 0.0f, scan = 0.0f;
    int rate = 3, shape = ShapeRandom;
    float range = 0.5f, snap = 0.5f, length = 0.8f, probability = 1.0f, reverse = 0.0f;
    int pitchSet = 0; float pitchAmount = 0.0f; bool slotRandom = false;
    float morphX = 0.0f, morphY = 0.0f;
    float cloud = 0.0f, grainSize = 0.12f, density = 20.0f, spread = 0.2f;
    float attack = 0.005f, release = 0.3f;
};

// Lock-free event log so the editor can draw where chops land.
struct ChopLog
{
    static constexpr int size = 64;
    std::array<std::atomic<float>, size> pos {};
    std::array<std::atomic<int>, size> slot {};
    std::atomic<int> write { 0 };

    void push (float p, int s)
    {
        const int w = write.load (std::memory_order_relaxed);
        pos[(size_t) (w % size)].store (p, std::memory_order_relaxed);
        slot[(size_t) (w % size)].store (s, std::memory_order_relaxed);
        write.store (w + 1, std::memory_order_release);
    }
};

//==============================================================================
class Engine
{
public:
    using Slots = std::array<SamplePtr, numSlots>;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v = Voice();
        scanPos = 0.0;
    }

    void setChopLog (ChopLog* l) { log = l; }

    // weights for the four slots from the morph pad; empty slots get 0
    static std::array<float, numSlots> weights (const Params& p, const Slots& s)
    {
        const float x = p.morphX, y = p.morphY;
        std::array<float, numSlots> w { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
        float sum = 0.0f;
        for (int i = 0; i < numSlots; ++i) { if (s[(size_t) i] == nullptr) w[(size_t) i] = 0.0f; sum += w[(size_t) i]; }
        if (sum < 1.0e-6f)
        {
            // pad is pointing at empty corners: fall back to whatever is loaded
            sum = 0.0f;
            for (int i = 0; i < numSlots; ++i) { w[(size_t) i] = s[(size_t) i] != nullptr ? 1.0f : 0.0f; sum += w[(size_t) i]; }
        }
        if (sum > 0.0f) for (auto& v : w) v /= sum;
        return w;
    }

    void noteOn (int note, float velocity, const Params& p, const Slots& s, double samplesPerStep)
    {
        bool anyActive = false;
        for (auto& v : voices) anyActive |= v.active;
        if (! anyActive) scanPos = 0.0;

        Voice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age > target->age) target = &v;
        }
        auto& v = *target;
        const bool steal = v.active;
        v.active = true; v.held = true; v.note = note;
        v.velocity = 0.25f + 0.75f * velocity;
        v.envStage = 0; if (! steal) v.env = 0.0f;
        v.age = 0; v.triggerCount = 0;
        v.rng.setSeed ((juce::int64) note * 7919 + (juce::int64) juce::Time::getMillisecondCounter());
        v.drift = p.position;
        for (auto& g : v.grains) g.active = false;
        for (auto& c : v.chops) c.active = false;

        if (p.playMode == ModeSlices) startSlice (v, p, s);
        else trigger (v, p, s, samplesPerStep, true);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.held && v.note == note) { v.held = false; v.envStage = 2; }
    }

    void allNotesOff()
    {
        for (auto& v : voices) if (v.active) { v.held = false; v.envStage = 2; }
    }

    // Called at every grid tick (e.g. each 1/16) for retriggering.
    void gridTick (const Params& p, const Slots& s, double samplesPerStep)
    {
        if (p.playMode != ModeOffset) return;
        for (auto& v : voices)
            if (v.active && v.envStage != 3)
                trigger (v, p, s, samplesPerStep, false);
    }

    void advanceScan (const Params& p, const Slots& s, int numSamples)
    {
        // scan moves the centre through the sample; speed 1 = real time
        double refLen = 0.0;
        const auto w = weights (p, s);
        for (int i = 0; i < numSlots; ++i)
            if (s[(size_t) i] != nullptr) refLen += w[(size_t) i] * s[(size_t) i]->seconds();
        if (refLen > 0.01)
            scanPos += (double) p.scan * numSamples / sr / refLen;
        scanPos -= std::floor (scanPos);
    }

    void render (float* outL, float* outR, int numSamples, const Params& p, const Slots& s)
    {
        const auto w = weights (p, s);
        const double attackStep = 1.0 / juce::jmax (1.0, (double) p.attack * sr);
        const double releaseCoef = std::exp (-1.0 / juce::jmax (1.0, (double) p.release * sr * 0.25));
        const float cloudAmt = p.cloud;
        const float chopAmt = 1.0f - cloudAmt;
        const double grainLen = juce::jmax (0.01, (double) p.grainSize) * sr;
        const float grainNorm = 1.4f / std::sqrt (juce::jmax (1.0f, p.density * p.grainSize));

        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int n = 0; n < numSamples; ++n)
            {
                // ---- amp envelope
                if (v.envStage == 0) { v.env += (float) attackStep; if (v.env >= 1.0f) { v.env = 1.0f; v.envStage = 1; } }
                else if (v.envStage == 2) { v.env *= (float) releaseCoef; if (v.env < 1.0e-4f) { v.envStage = 3; } }
                if (v.envStage == 3) { v.active = false; break; }

                float l = 0.0f, r = 0.0f;

                // ---- chops
                if (chopAmt > 0.0f)
                {
                    for (auto& c : v.chops)
                    {
                        if (! c.active) continue;
                        const float g = chopGain (c);
                        if (g > 0.0f)
                        {
                            float cl = 0.0f, cr = 0.0f;
                            if (c.slot >= 0) readSlot (*s[(size_t) c.slot], c, cl, cr);
                            else
                                for (int i = 0; i < numSlots; ++i)
                                    if (w[(size_t) i] > 0.001f && s[(size_t) i] != nullptr)
                                    {
                                        float a = 0.0f, b = 0.0f;
                                        readSlotAt (*s[(size_t) i], c.startNorm, c.age, c.ratio, c.reverse, a, b);
                                        cl += a * w[(size_t) i]; cr += b * w[(size_t) i];
                                    }
                            l += cl * g; r += cr * g;
                        }
                        c.age += 1.0;
                        if (c.age >= c.length) c.active = false;
                    }
                    l *= chopAmt; r *= chopAmt;
                }

                // ---- grain cloud
                if (cloudAmt > 0.0f)
                {
                    v.grainAcc += p.density / sr;
                    while (v.grainAcc >= 1.0)
                    {
                        v.grainAcc -= 1.0;
                        spawnGrain (v, p, s, w, grainLen);
                    }
                    float gl = 0.0f, gr = 0.0f;
                    for (auto& g : v.grains)
                    {
                        if (! g.active) continue;
                        const float ph = (float) (g.age / g.length);
                        const float win = std::sin (juce::MathConstants<float>::pi * ph);
                        float a = 0.0f, b = 0.0f;
                        readSlotAt (*s[(size_t) g.slot], g.startNorm, g.age, g.ratio, false, a, b);
                        gl += a * win * win * g.panL;
                        gr += b * win * win * g.panR;
                        g.age += 1.0;
                        if (g.age >= g.length) g.active = false;
                    }
                    l += gl * grainNorm * cloudAmt;
                    r += gr * grainNorm * cloudAmt;
                }

                const float amp = v.env * v.velocity * 0.5f; // headroom for chords
                outL[n] += l * amp;
                outR[n] += r * amp;
                ++v.age;
            }
        }
    }

    float getScan() const { return (float) scanPos; }

private:
    struct Chop
    {
        bool active = false;
        int slot = -1;           // -1 = blend all slots by morph weights
        double startNorm = 0.0, ratio = 1.0, age = 0.0, length = 1.0;
        double fadeIn = 64.0, fadeOut = 256.0;
        bool reverse = false;
    };

    struct Grain
    {
        bool active = false;
        int slot = 0;
        double startNorm = 0.0, ratio = 1.0, age = 0.0, length = 1.0;
        float panL = 1.0f, panR = 1.0f;
    };

    struct Voice
    {
        bool active = false, held = false;
        int note = 60, envStage = 0, triggerCount = 0;
        float velocity = 1.0f, env = 0.0f;
        double centre = 0.0, drift = 0.0, grainAcc = 0.0;
        juce::int64 age = 0;
        std::array<Chop, 3> chops {};
        std::array<Grain, 32> grains {};
        juce::Random rng;
    };

    static float chopGain (const Chop& c)
    {
        const double a = c.age;
        double g = 1.0;
        if (a < c.fadeIn) g = a / c.fadeIn;
        const double toEnd = c.length - a;
        if (toEnd < c.fadeOut) g = juce::jmin (g, toEnd / c.fadeOut);
        return (float) juce::jlimit (0.0, 1.0, g);
    }

    void readSlotAt (const SampleData& sd, double startNorm, double age, double ratio, bool rev, float& l, float& r) const
    {
        const double step = ratio * sd.sampleRate / sr;
        const double start = startNorm * (sd.length() - 1);
        const double pos = rev ? start - age * step : start + age * step;
        const int len = sd.length();
        if (pos < 0.0 || pos >= len - 1) { l = r = 0.0f; return; }
        const int i = (int) pos;
        const float f = (float) (pos - i);
        const auto* L = sd.audio.getReadPointer (0);
        const auto* R = sd.audio.getReadPointer (1);
        l = L[i] + f * (L[i + 1] - L[i]);
        r = R[i] + f * (R[i + 1] - R[i]);
    }

    void readSlot (const SampleData& sd, const Chop& c, float& l, float& r) const
    {
        readSlotAt (sd, c.startNorm, c.age, c.ratio, c.reverse, l, r);
    }

    int pickSlot (const std::array<float, numSlots>& w, juce::Random& rng) const
    {
        float x = rng.nextFloat();
        int last = 0;
        for (int i = 0; i < numSlots; ++i)
        {
            if (w[(size_t) i] <= 0.0f) continue;
            last = i;
            x -= w[(size_t) i];
            if (x <= 0.0f) return i;
        }
        return last;
    }

    int dominantSlot (const std::array<float, numSlots>& w) const
    {
        int best = 0;
        for (int i = 1; i < numSlots; ++i) if (w[(size_t) i] > w[(size_t) best]) best = i;
        return best;
    }

    double noteRatio (int note, int extraSemis) const
    {
        return std::pow (2.0, (note - 60 + extraSemis) / 12.0);
    }

    // Shortest time (in host samples) the chop can play before running out of audio
    double available (const Slots& s, const std::array<float, numSlots>& w, int slot,
                      double startNorm, double ratio, bool rev) const
    {
        double best = 1.0e12;
        for (int i = 0; i < numSlots; ++i)
        {
            if (s[(size_t) i] == nullptr) continue;
            if (slot >= 0 ? i != slot : w[(size_t) i] <= 0.001f) continue;
            const auto& sd = *s[(size_t) i];
            const double step = ratio * sd.sampleRate / sr;
            const double start = startNorm * (sd.length() - 1);
            const double room = rev ? start : (sd.length() - 2) - start;
            best = juce::jmin (best, room / step);
        }
        return best;
    }

    void startChop (Voice& v, const Slots& s, const std::array<float, numSlots>& w, int slot,
                    double startNorm, double ratio, bool rev, double wantLen)
    {
        // fade out whatever is still sounding
        for (auto& c : v.chops)
            if (c.active)
            {
                const double fade = juce::jmin (c.length - c.age, 0.006 * sr);
                c.fadeOut = juce::jmax (1.0, fade);
                c.length = c.age + c.fadeOut;
            }

        Chop* slotPtr = &v.chops[0];
        for (auto& c : v.chops) if (! c.active) { slotPtr = &c; break; }
        auto& c = *slotPtr;

        const double avail = available (s, w, slot, startNorm, ratio, rev);
        c.active = true;
        c.slot = slot;
        c.startNorm = startNorm;
        c.ratio = ratio;
        c.reverse = rev;
        c.age = 0.0;
        c.length = juce::jmax (32.0, juce::jmin (wantLen, avail));
        c.fadeIn = juce::jmin (0.003 * sr, c.length * 0.25);
        c.fadeOut = juce::jmin (0.012 * sr, c.length * 0.4);
        v.centre = startNorm;

        if (log != nullptr) log->push ((float) startNorm, slot >= 0 ? slot : dominantSlot (w));
    }

    double snapToOnset (const SampleData& sd, double pos, float amount, juce::Random& rng) const
    {
        if (amount <= 0.0f || rng.nextFloat() > amount) return pos;
        double best = pos, bd = 1.0;
        for (auto o : sd.onsets)
        {
            const double d = std::abs ((double) o - pos);
            if (d < bd) { bd = d; best = o; }
        }
        return best;
    }

    void trigger (Voice& v, const Params& p, const Slots& s, double samplesPerStep, bool first)
    {
        const auto w = weights (p, s);
        bool any = false;
        for (auto x : w) any |= x > 0.0f;
        if (! any) return;

        auto& rng = v.rng;
        const int k = v.triggerCount++;

        if (! first && rng.nextFloat() > p.probability)
        {
            // skip this hit: let the previous chop fade quickly
            for (auto& c : v.chops)
                if (c.active) { c.fadeOut = juce::jmax (1.0, juce::jmin (c.length - c.age, 0.01 * sr)); c.length = c.age + c.fadeOut; }
            return;
        }

        double centre = p.position + scanPos;
        centre -= std::floor (centre);

        double pos = centre;
        const double range = p.range;
        switch (p.shape)
        {
            case ShapeRamp:  pos = centre + ((k % 8) / 8.0 - 0.4375) * range; break;
            case ShapeSine:  pos = centre + 0.5 * range * std::sin (juce::MathConstants<double>::twoPi * (k % 16) / 16.0); break;
            case ShapeDrift: v.drift += (rng.nextDouble() - 0.5) * range * 0.35;
                             v.drift = juce::jlimit (centre - range * 0.5, centre + range * 0.5, v.drift);
                             pos = v.drift; break;
            default:         pos = centre + (rng.nextDouble() - 0.5) * range; break;
        }
        pos -= std::floor (pos);

        const int slot = p.slotRandom ? pickSlot (w, rng) : -1;
        const int snapSlot = slot >= 0 ? slot : dominantSlot (w);
        if (s[(size_t) snapSlot] != nullptr)
            pos = snapToOnset (*s[(size_t) snapSlot], pos, p.snap, rng);

        int semis = 0;
        if (p.pitchAmount > 0.0f && rng.nextFloat() < p.pitchAmount)
        {
            const auto& set = pitchSet (p.pitchSet);
            semis = set[(size_t) rng.nextInt ((int) set.size())];
        }
        const bool rev = p.reverse > 0.0f && rng.nextFloat() < p.reverse;
        const double ratio = noteRatio (v.note, semis);
        const double len = juce::jmax (0.01 * sr, samplesPerStep * p.length);
        startChop (v, s, w, slot, pos, ratio, rev, len);
    }

    void startSlice (Voice& v, const Params& p, const Slots& s)
    {
        const auto w = weights (p, s);
        const int d = dominantSlot (w);
        if (s[(size_t) d] == nullptr || w[(size_t) d] <= 0.0f) return;
        const auto& sd = *s[(size_t) d];
        const int count = (int) sd.onsets.size();
        int idx = (v.note - 36) % count;
        if (idx < 0) idx += count;
        const double start = sd.onsets[(size_t) idx];
        const double end = idx + 1 < count ? (double) sd.onsets[(size_t) idx + 1] : 1.0;
        const double len = (end - start) * sd.length() * sr / sd.sampleRate;
        const bool rev = p.reverse > 0.0f && v.rng.nextFloat() < p.reverse;
        startChop (v, s, w, d, rev ? end : start, 1.0, rev, len);
    }

    void spawnGrain (Voice& v, const Params& p, const Slots& s, const std::array<float, numSlots>& w, double grainLen)
    {
        Grain* g = nullptr;
        for (auto& x : v.grains) if (! x.active) { g = &x; break; }
        if (g == nullptr) return;
        auto& rng = v.rng;
        const int slot = pickSlot (w, rng);
        if (s[(size_t) slot] == nullptr) return;

        int semis = 0;
        if (p.pitchAmount > 0.0f && rng.nextFloat() < p.pitchAmount)
        {
            const auto& set = pitchSet (p.pitchSet);
            semis = set[(size_t) rng.nextInt ((int) set.size())];
        }
        const int slicesNote = p.playMode == ModeSlices ? 60 : v.note;
        const double ratio = noteRatio (slicesNote, semis);
        double pos = v.centre + (rng.nextDouble() - 0.5) * p.spread;
        pos = juce::jlimit (0.0, 0.999, pos);
        const double len = grainLen * (0.7 + 0.6 * rng.nextDouble());
        const double avail = available (s, w, slot, pos, ratio, false);
        if (avail < 64.0) return;

        const float pan = 0.5f + (rng.nextFloat() - 0.5f) * juce::jmin (1.0f, p.spread * 2.0f + 0.3f);
        g->active = true;
        g->slot = slot;
        g->startNorm = pos;
        g->ratio = ratio;
        g->age = 0.0;
        g->length = juce::jmin (len, avail);
        g->panL = std::cos (pan * juce::MathConstants<float>::halfPi) * 1.41f;
        g->panR = std::sin (pan * juce::MathConstants<float>::halfPi) * 1.41f;
    }

    std::array<Voice, numVoices> voices {};
    double sr = 44100.0, scanPos = 0.0;
    ChopLog* log = nullptr;
};

//==============================================================================
// Crunch, Pump, Wash on the summed signal.
class ColorFX
{
public:
    void prepare (double sampleRate, int blockSize)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };
        lp.prepare (spec); lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass); lp.setResonance (0.6f);
        hp.prepare (spec); hp.setType (juce::dsp::StateVariableTPTFilterType::highpass); hp.setResonance (0.5f);
        lp.reset(); hp.reset();
        reverb.setSampleRate (sr); reverb.reset();
        crunchSm.reset (sr, 0.05); washSm.reset (sr, 0.05); pumpSm.reset (sr, 0.05); gainSm.reset (sr, 0.05);
        holdL = holdR = 0.0f; holdCount = 0;
    }

    void process (float* L, float* R, int n, float crunch, float wash, float pump, float gainDb,
                  double ppqStart, double ppqPerSample)
    {
        crunchSm.setTargetValue (crunch); washSm.setTargetValue (wash);
        pumpSm.setTargetValue (pump); gainSm.setTargetValue (juce::Decibels::decibelsToGain (gainDb));

        const float relCoef = (float) std::exp (-1.0 / (0.4 * sr));
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            const float lvl = juce::jmax (std::abs (l), std::abs (r));
            follower = lvl > follower ? lvl : follower * relCoef;
            const float noiseGate = juce::jmin (1.0f, follower * 6.0f);

            // ---- Crunch: saturation, then sample-rate reduction at high settings
            const float c = crunchSm.getNextValue();
            if (c > 0.001f)
            {
                const float drive = 1.0f + c * 14.0f;
                const float norm = 1.0f / std::tanh (drive);
                const float sl = std::tanh (l * drive) * norm, sr2 = std::tanh (r * drive) * norm;
                l = l + (sl * 0.8f - l) * juce::jmin (1.0f, c * 1.6f);
                r = r + (sr2 * 0.8f - r) * juce::jmin (1.0f, c * 1.6f);
                const int hold = 1 + (int) (juce::jmax (0.0f, c - 0.5f) * 10.0f);
                if (holdCount <= 0) { holdL = l; holdR = r; holdCount = hold; }
                --holdCount;
                if (hold > 1) { l = holdL; r = holdR; }
            }

            // ---- Pump: ducking synced to quarter notes
            const float pmp = pumpSm.getNextValue();
            if (pmp > 0.001f)
            {
                double ph = ppqStart + i * ppqPerSample;
                ph -= std::floor (ph);
                const float duck = (float) std::exp (-ph * 5.0);
                const float g = 1.0f - pmp * 0.92f * duck * juce::jmin (1.0f, (float) (ph * 60.0) + 0.2f);
                l *= g; r *= g;
            }

            // ---- Wash, part 1: darken + thin out the lows a touch
            const float w = washSm.getNextValue();
            if ((i & 15) == 0)
            {
                const float cutoff = 20000.0f * std::pow (1.0f - w * 0.93f, 2.2f) + 400.0f;
                lp.setCutoffFrequency (juce::jmin (cutoff, (float) (sr * 0.45)));
                hp.setCutoffFrequency (20.0f + w * 120.0f);
            }
            if (w > 0.001f)
            {
                const float fl = hp.processSample (0, lp.processSample (0, l));
                const float fr = hp.processSample (1, lp.processSample (1, r));
                l = fl; r = fr;
                // vinyl crackle
                if (rng.nextFloat() < w * 0.0012f * noiseGate)
                {
                    const float pop = (rng.nextFloat() - 0.5f) * 0.25f * w;
                    l += pop; r += pop * 0.8f;
                }
                const float hiss = (rng.nextFloat() - 0.5f) * 0.004f * w * noiseGate;
                l += hiss; r += hiss;
            }
            L[i] = l; R[i] = r;
        }
        lp.snapToZero(); hp.snapToZero();

        // ---- Wash, part 2: big reverb
        const float wv = washSm.getCurrentValue();
        if (wv > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.6f + 0.38f * wv;
            rp.damping = 0.55f;
            rp.wetLevel = 0.32f * wv;               // juce::Reverb scales wet by 3
            rp.dryLevel = 0.5f * (1.0f - 0.35f * wv); // and dry by 2
            rp.width = 1.0f;
            reverb.setParameters (rp);
            reverb.processStereo (L, R, n);
        }

        // ---- output gain + soft safety limiter
        for (int i = 0; i < n; ++i)
        {
            const float g = gainSm.getNextValue();
            L[i] = safety (L[i] * g);
            R[i] = safety (R[i] * g);
        }
    }

private:
    static float safety (float x)
    {
        if (! std::isfinite (x)) return 0.0f;
        const float a = std::abs (x);
        if (a < 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0.0f ? -y : y;
    }

    double sr = 44100.0;
    juce::dsp::StateVariableTPTFilter<float> lp, hp;
    juce::Reverb reverb;
    juce::SmoothedValue<float> crunchSm, washSm, pumpSm, gainSm;
    float holdL = 0.0f, holdR = 0.0f, follower = 0.0f;
    int holdCount = 0;
    juce::Random rng { 99 };
};

} // namespace prisma
