#include "PluginProcessor.h"
#include "Presets.h"
#include <initializer_list>
#include <utility>

namespace
{
using Setting = std::pair<const char*, float>;

struct Preset
{
    const char* name;
    std::vector<Setting> values;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Init", {} },
        { "Future Chop", { { "rate", 3 }, { "shape", 0 }, { "range", 0.45f }, { "snap", 0.8f }, { "length", 0.7f },
                           { "probability", 0.9f }, { "reverse", 0.15f }, { "pitchSet", 1 }, { "pitchAmount", 0.25f },
                           { "scan", 0.25f }, { "crunch", 0.25f }, { "wash", 0.1f }, { "pump", 0.45f }, { "gain", 3 } } },
        { "Cloud Haze", { { "rate", 1 }, { "shape", 3 }, { "range", 0.2f }, { "snap", 0.3f }, { "length", 1.0f },
                          { "reverse", 0.1f }, { "pitchSet", 0 }, { "pitchAmount", 0.2f }, { "cloud", 0.7f },
                          { "grainSize", 220 }, { "density", 26 }, { "spread", 0.25f }, { "attack", 300 },
                          { "release", 1500 }, { "wash", 0.75f }, { "crunch", 0.1f }, { "scan", 0.15f }, { "gain", 5 } } },
        { "Glitch Rush", { { "rate", 5 }, { "shape", 0 }, { "range", 0.8f }, { "snap", 0.5f }, { "length", 0.5f },
                           { "probability", 0.75f }, { "reverse", 0.35f }, { "pitchSet", 5 }, { "pitchAmount", 0.3f },
                           { "crunch", 0.55f }, { "pump", 0.2f } } },
        { "Slow Ramp", { { "rate", 4 }, { "shape", 1 }, { "range", 0.3f }, { "snap", 0.0f }, { "length", 1.0f },
                         { "scan", 0.5f }, { "cloud", 0.25f }, { "wash", 0.35f } } },
        { "Vocal Choir", { { "rate", 1 }, { "shape", 2 }, { "range", 0.1f }, { "cloud", 0.85f }, { "grainSize", 140 },
                           { "density", 40 }, { "spread", 0.1f }, { "pitchSet", 3 }, { "pitchAmount", 0.6f },
                           { "wash", 0.5f }, { "attack", 150 }, { "release", 900 }, { "gain", 2 } } },
        { "Slice Keys", { { "playMode", 1 }, { "attack", 1 }, { "release", 120 }, { "wash", 0.15f }, { "gain", 5 } } },
    };
    return p;
}
} // namespace

juce::StringArray prisma::presetNames()
{
    juce::StringArray n;
    for (auto& p : presets()) n.add (p.name);
    return n;
}

void PrismaProcessor::applyPreset (int index)
{
    const auto& all = presets();
    if (index < 0 || index >= (int) all.size()) return;

    // reset sound parameters (not the morph pad) to defaults first
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto id = rp->getParameterID();
            if (id == "morphX" || id == "morphY" || id == "slotRandom") continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    for (auto& [id, v] : all[(size_t) index].values)
        setParam (id, v);
}
