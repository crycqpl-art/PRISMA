#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class PrismaProcessor : public juce::AudioProcessor
{
public:
    PrismaProcessor();
    ~PrismaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Prisma"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- slots (message thread)
    bool loadSlot (int index, const juce::File& file, juce::String& error);
    void clearSlot (int index);
    prisma::SamplePtr getSlot (int index) const { return std::atomic_load (&slots[(size_t) index]); }
    void releaseRetired();

    // ---- helpers for the UI (message thread)
    void setParam (const juce::String& id, float realValue);
    float getParam (const juce::String& id) const;
    void dice();
    void applyPreset (int index);
    juce::String settingsToText() const;
    int settingsFromText (const juce::String& text);

    float getScan() const { return scanDisplay.load(); }

    juce::AudioProcessorValueTreeState apvts;
    prisma::ChopLog chopLog;
    std::atomic<int> activeNotes { 0 };
    std::atomic<float> outputLevel { 0.0f };

private:
    prisma::Params readParams() const;
    void storeSlotPath (int index, const juce::String& path);

    std::array<prisma::SamplePtr, prisma::numSlots> slots {};
    std::vector<prisma::SamplePtr> retired;

    prisma::Engine engine;
    prisma::ColorFX fx;

    double sr = 44100.0, internalPpq = 0.0;
    juce::int64 lastGridIndex = std::numeric_limits<juce::int64>::min();
    std::atomic<float> scanDisplay { 0.0f };
    std::array<int, 128> noteState {};
    juce::Random uiRandom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismaProcessor)
};
