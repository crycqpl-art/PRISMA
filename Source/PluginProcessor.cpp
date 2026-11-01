#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace prisma;

juce::AudioProcessorValueTreeState::ParameterLayout PrismaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; });
    auto speed = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return std::abs (v) < 0.005f ? String ("stop") : (v > 0 ? "+" : "") + String (v, 2) + "x"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    auto perSec = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 0) + " /s"; });

    auto f = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
    auto c = [&] (const char* id, const char* name, StringArray items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    c ("playMode", "Play Mode", playModeNames(), 0);
    f ("position", "Position", { 0.0f, 1.0f }, 0.0f, pct);
    f ("scan", "Scan", { -2.0f, 2.0f, 0.0f, 1.0f, true }, 0.25f, speed);
    c ("rate", "Rate", rateNames(), 3);
    c ("shape", "Offset Shape", shapeNames(), 0);
    f ("range", "Range", { 0.0f, 1.0f }, 0.35f, pct);
    f ("snap", "Snap", { 0.0f, 1.0f }, 0.6f, pct);
    f ("length", "Length", { 0.05f, 1.0f }, 0.9f, pct);
    f ("probability", "Probability", { 0.0f, 1.0f }, 1.0f, pct);
    f ("reverse", "Reverse", { 0.0f, 1.0f }, 0.0f, pct);
    c ("pitchSet", "Pitch Set", pitchSetNames(), 0);
    f ("pitchAmount", "Pitch Random", { 0.0f, 1.0f }, 0.0f, pct);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "slotRandom", 1 }, "Slot Random", false));
    f ("morphX", "Morph X", { 0.0f, 1.0f }, 0.0f, pct);
    f ("morphY", "Morph Y", { 0.0f, 1.0f }, 0.0f, pct);
    f ("cloud", "Cloud", { 0.0f, 1.0f }, 0.0f, pct);
    f ("grainSize", "Grain Size", { 10.0f, 500.0f, 0.0f, 0.5f }, 90.0f, ms);
    f ("density", "Density", { 1.0f, 80.0f, 0.0f, 0.6f }, 22.0f, perSec);
    f ("spread", "Spread", { 0.0f, 1.0f }, 0.15f, pct);
    f ("attack", "Attack", { 1.0f, 2000.0f, 0.0f, 0.35f }, 5.0f, ms);
    f ("release", "Release", { 10.0f, 4000.0f, 0.0f, 0.35f }, 250.0f, ms);
    f ("crunch", "Crunch", { 0.0f, 1.0f }, 0.0f, pct);
    f ("wash", "Wash", { 0.0f, 1.0f }, 0.0f, pct);
    f ("pump", "Pump", { 0.0f, 1.0f }, 0.0f, pct);
    f ("gain", "Output", { -24.0f, 12.0f }, 0.0f, db);
    return l;
}

PrismaProcessor::PrismaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PRISMA", createLayout())
{
    engine.setChopLog (&chopLog);
}

bool PrismaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void PrismaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr);
    fx.prepare (sr, samplesPerBlock);
    lastGridIndex = std::numeric_limits<juce::int64>::min();
    noteState.fill (0);
}

Params PrismaProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Params p;
    p.playMode = (int) v ("playMode");
    p.position = v ("position");
    p.scan = v ("scan");
    p.rate = (int) v ("rate");
    p.shape = (int) v ("shape");
    p.range = v ("range");
    p.snap = v ("snap");
    p.length = v ("length");
    p.probability = v ("probability");
    p.reverse = v ("reverse");
    p.pitchSet = (int) v ("pitchSet");
    p.pitchAmount = v ("pitchAmount");
    p.slotRandom = v ("slotRandom") > 0.5f;
    p.morphX = v ("morphX");
    p.morphY = v ("morphY");
    p.cloud = v ("cloud");
    p.grainSize = v ("grainSize") * 0.001f;
    p.density = v ("density");
    p.spread = v ("spread");
    p.attack = v ("attack") * 0.001f;
    p.release = v ("release") * 0.001f;
    return p;
}

void PrismaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    const auto p = readParams();
    Engine::Slots s;
    for (int i = 0; i < numSlots; ++i) s[(size_t) i] = std::atomic_load (&slots[(size_t) i]);

    // ---- musical clock
    double bpm = 120.0, ppq = internalPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const double divs = rateDivisions (p.rate);
    const double samplesPerStep = (60.0 / bpm) * sr / divs;
    const double ppqPerSample = bpm / (60.0 * sr);

    engine.advanceScan (p, s, n);

    // ---- collect grid ticks inside this block
    std::array<int, 64> ticks {};
    int numTicks = 0;
    for (int i = 0; i < n; ++i)
    {
        const auto g = (juce::int64) std::floor ((ppq + i * ppqPerSample) * divs + 1.0e-9);
        if (g != lastGridIndex)
        {
            if (lastGridIndex != std::numeric_limits<juce::int64>::min() && numTicks < (int) ticks.size())
                ticks[(size_t) numTicks++] = i;
            lastGridIndex = g;
        }
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    int cursor = 0, tickIdx = 0;
    auto midiIt = midi.begin();

    auto renderTo = [&] (int end)
    {
        if (end > cursor) engine.render (L + cursor, R + cursor, end - cursor, p, s);
        cursor = juce::jmax (cursor, end);
    };

    while (true)
    {
        const int nextTick = tickIdx < numTicks ? ticks[(size_t) tickIdx] : n;
        const int nextMidi = midiIt != midi.end() ? juce::jlimit (0, n - 1, (*midiIt).samplePosition) : n;
        const int next = juce::jmin (nextTick, nextMidi);
        if (next >= n) break;

        renderTo (next);
        if (nextTick <= nextMidi)
        {
            engine.gridTick (p, s, samplesPerStep);
            ++tickIdx;
        }
        else
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
            {
                engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity(), p, s, samplesPerStep);
                noteState[(size_t) msg.getNoteNumber()] = 1;
            }
            else if (msg.isNoteOff())
            {
                engine.noteOff (msg.getNoteNumber());
                noteState[(size_t) msg.getNoteNumber()] = 0;
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                engine.allNotesOff();
                noteState.fill (0);
            }
            ++midiIt;
        }
    }
    renderTo (n);

    fx.process (L, R, n, apvts.getRawParameterValue ("crunch")->load(),
                apvts.getRawParameterValue ("wash")->load(),
                apvts.getRawParameterValue ("pump")->load(),
                apvts.getRawParameterValue ("gain")->load(), ppq, ppqPerSample);

    int held = 0;
    for (auto v : noteState) held += v;
    activeNotes.store (held);
    outputLevel.store (buffer.getMagnitude (0, n));
    scanDisplay.store (engine.getScan());
    internalPpq = ppq + n * ppqPerSample;
}

//==============================================================================
void PrismaProcessor::storeSlotPath (int index, const juce::String& path)
{
    apvts.state.setProperty ("slot" + juce::String (index), path, nullptr);
}

bool PrismaProcessor::loadSlot (int index, const juce::File& file, juce::String& error)
{
    auto data = loadSample (file, error);
    if (data == nullptr) return false;
    auto old = std::atomic_exchange (&slots[(size_t) index], data);
    if (old != nullptr) retired.push_back (old);
    storeSlotPath (index, file.getFullPathName());
    return true;
}

void PrismaProcessor::clearSlot (int index)
{
    auto old = std::atomic_exchange (&slots[(size_t) index], SamplePtr());
    if (old != nullptr) retired.push_back (old);
    storeSlotPath (index, {});
}

void PrismaProcessor::releaseRetired()
{
    // only drop samples the audio thread can no longer be holding
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const SamplePtr& p) { return p.use_count() == 1; }),
                   retired.end());
}

void PrismaProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    }
}

float PrismaProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void PrismaProcessor::dice()
{
    auto& r = uiRandom;
    setParam ("rate", (float) (1 + r.nextInt (5)));
    setParam ("shape", (float) r.nextInt (4));
    setParam ("range", 0.1f + 0.8f * r.nextFloat());
    setParam ("snap", r.nextFloat());
    setParam ("length", 0.3f + 0.7f * r.nextFloat());
    setParam ("probability", r.nextFloat() < 0.6f ? 1.0f : 0.6f + 0.4f * r.nextFloat());
    setParam ("reverse", r.nextFloat() < 0.5f ? 0.0f : 0.5f * r.nextFloat());
    setParam ("pitchSet", (float) r.nextInt (5));
    setParam ("pitchAmount", r.nextFloat() < 0.4f ? 0.0f : 0.6f * r.nextFloat());
    setParam ("cloud", r.nextFloat() < 0.5f ? 0.0f : 0.7f * r.nextFloat());
    setParam ("scan", r.nextFloat() < 0.5f ? 0.0f : (r.nextFloat() - 0.3f) * 1.2f);
}

juce::String PrismaProcessor::settingsToText() const
{
    juce::String t;
    t << "PRISMA " << PRISMA_VERSION << " settings\n";
    for (int i = 0; i < numSlots; ++i)
    {
        auto sd = getSlot (i);
        t << "# slot " << juce::String::charToString ((juce::juce_wchar) ('A' + i)) << ": "
          << (sd != nullptr ? sd->name : juce::String ("(empty)")) << "\n";
    }
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            t << rp->getParameterID() << "=" << juce::String (rp->convertFrom0to1 (rp->getValue()), 4)
              << "  # " << rp->getCurrentValueAsText() << "\n";
    return t;
}

int PrismaProcessor::settingsFromText (const juce::String& text)
{
    int applied = 0;
    for (auto line : juce::StringArray::fromLines (text))
    {
        line = line.upToFirstOccurrenceOf ("#", false, false).trim();
        if (! line.containsChar ('=')) continue;
        const auto id = line.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto val = line.fromFirstOccurrenceOf ("=", false, false).trim();
        if (apvts.getParameter (id) != nullptr && val.isNotEmpty())
        {
            setParam (id, val.getFloatValue());
            ++applied;
        }
    }
    return applied;
}

//==============================================================================
void PrismaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PrismaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            for (int i = 0; i < numSlots; ++i)
            {
                const auto path = apvts.state.getProperty ("slot" + juce::String (i)).toString();
                juce::String err;
                if (path.isNotEmpty() && juce::File (path).existsAsFile())
                    loadSlot (i, juce::File (path), err);
                else
                {
                    auto old = std::atomic_exchange (&slots[(size_t) i], SamplePtr());
                    if (old != nullptr) retired.push_back (old);
                    // keep the remembered path so the UI can say what is missing
                }
            }
        }
}

juce::AudioProcessorEditor* PrismaProcessor::createEditor()
{
    return new PrismaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismaProcessor();
}
