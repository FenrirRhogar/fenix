#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

namespace fenix
{

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

// What the editor's meter strip shows; an empty label hides it.
struct MeterSpec
{
    juce::String label;
    float min = 0.0f, max = 1.0f;
    juce::String unit;
};

// Lock-free single-producer/single-consumer stereo sample queue: the audio thread
// pushes processed output, the editor drains it for scopes and analysers.
class ScopeFifo
{
public:
    static constexpr int capacity = 8192;

    void push (const juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const int n = juce::jmin (buffer.getNumSamples(), fifo.getFreeSpace());
        const auto* l = buffer.getReadPointer (0);
        const auto* r = buffer.getReadPointer (numChannels > 1 ? 1 : 0);
        const auto scope = fifo.write (n);
        copy (l, r, 0, scope.startIndex1, scope.blockSize1);
        copy (l, r, scope.blockSize1, scope.startIndex2, scope.blockSize2);
    }

    int pull (float* l, float* r, int maxSamples)
    {
        const auto scope = fifo.read (juce::jmin (maxSamples, fifo.getNumReady()));
        std::copy_n (left + scope.startIndex1, scope.blockSize1, l);
        std::copy_n (right + scope.startIndex1, scope.blockSize1, r);
        std::copy_n (left + scope.startIndex2, scope.blockSize2, l + scope.blockSize1);
        std::copy_n (right + scope.startIndex2, scope.blockSize2, r + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    void copy (const float* l, const float* r, int from, int to, int count)
    {
        std::copy_n (l + from, count, left + to);
        std::copy_n (r + from, count, right + to);
    }

    juce::AbstractFifo fifo { capacity };
    float left[capacity] {}, right[capacity] {};
};

// Shared boilerplate: stereo (optionally mono) in == out, APVTS state, generic editor.
class Processor : public juce::AudioProcessor
{
public:
    explicit Processor (Layout layout)
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", withBypass (std::move (layout)))
    {
        bypass = apvts.getRawParameterValue ("bypass");
    }

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;
    virtual juce::Colour accent() const = 0;
    virtual bool allowMono() const { return true; }
    virtual MeterSpec meterSpec() const { return {}; }

    std::atomic<float>* param (const char* id) { return apvts.getRawParameterValue (id); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0.0f };
    std::atomic<float> inputRms { 0.0f }, inputPeak { 0.0f }, outputRms { 0.0f };
    ScopeFifo scope;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maxBlockSize) override
    {
        prepare (sampleRate, maxBlockSize);

        // Bypass crossfades to a dry path delayed by the plugin's latency, so it never clicks or shifts timing.
        const auto channels = juce::jmax (1, getTotalNumInputChannels());
        dryBuffer.setSize (channels, maxBlockSize);
        dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) channels });
        dryDelay.setDelay ((float) getLatencySamples());
        bypassMix.reset (sampleRate, 0.02);
        bypassMix.setCurrentAndTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto& out = layouts.getMainOutputChannelSet();
        if (out != layouts.getMainInputChannelSet())
            return false;
        return out == juce::AudioChannelSet::stereo()
            || (allowMono() && out == juce::AudioChannelSet::mono());
    }

    using AudioProcessor::processBlock;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int numChannels = getTotalNumInputChannels();
        const int numSamples = buffer.getNumSamples();
        for (auto ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);

        if (dryBuffer.getNumSamples() < numSamples)
            dryBuffer.setSize (dryBuffer.getNumChannels(), numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        inputRms.store (blockLevel (buffer, numChannels, false));
        inputPeak.store (blockLevel (buffer, numChannels, true));

        process (buffer);

        bypassMix.setTargetValue (bypass->load() > 0.5f ? 1.0f : 0.0f);
        for (int n = 0; n < numSamples; ++n)
        {
            const float b = bypassMix.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                dryDelay.pushSample (ch, dryBuffer.getSample (ch, n));
                const float dry = dryDelay.popSample (ch);
                const float wet = buffer.getSample (ch, n);
                buffer.setSample (ch, n, wet + b * (dry - wet));
            }
        }

        outputRms.store (blockLevel (buffer, numChannels, false));
        scope.push (buffer, numChannels);
    }

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    juce::AudioProcessorEditor* createEditor() override; // defined in Editor.h
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

private:
    static float blockLevel (const juce::AudioBuffer<float>& buffer, int numChannels, bool peak)
    {
        float level = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            level = juce::jmax (level, peak ? buffer.getMagnitude (ch, 0, buffer.getNumSamples())
                                            : buffer.getRMSLevel (ch, 0, buffer.getNumSamples()));
        return level;
    }

    static Layout withBypass (Layout layout)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
        return layout;
    }

    std::atomic<float>* bypass = nullptr;
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::SmoothedValue<float> bypassMix;
};

//==============================================================================
// Parameter helpers

inline std::unique_ptr<juce::AudioParameterFloat> dbParam (const char* id, const char* name,
                                                           float min, float max, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (min, max, 0.1f), def,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; }));
}

inline std::unique_ptr<juce::AudioParameterFloat> percentParam (const char* id, const char* name,
                                                                float max, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, max, 0.1f), def,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));
}

inline std::unique_ptr<juce::AudioParameterFloat> hzParam (const char* id, const char* name,
                                                           float min, float max, float def)
{
    juce::NormalisableRange<float> range (min, max);
    range.setSkewForCentre (std::sqrt (min * max));
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) {
                return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                    : juce::String (juce::roundToInt (v)) + " Hz";
            }));
}

inline std::unique_ptr<juce::AudioParameterChoice> choiceParam (const char* id, const char* name,
                                                                juce::StringArray choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def);
}

} // namespace fenix
