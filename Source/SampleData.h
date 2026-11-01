#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

namespace prisma
{

// One loaded sample: audio (always 2 channels), detected onsets and a
// downsampled overview used for drawing.
struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String path, name;
    std::vector<float> onsets;      // normalised 0..1, always starts with 0
    std::vector<float> overview;    // peak per bucket, 0..1
    std::vector<float> brightness;  // rough high-frequency share per bucket, 0..1

    int length() const { return audio.getNumSamples(); }
    double seconds() const { return audio.getNumSamples() / sampleRate; }
};

using SamplePtr = std::shared_ptr<const SampleData>;

// Loads and analyses a file. Returns nullptr (and sets error) on failure.
// Runs on the message thread.
SamplePtr loadSample (const juce::File& file, juce::String& error);

} // namespace prisma
