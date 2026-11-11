#pragma once

#include "Bypass.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

namespace fenix
{

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

class Display;

// Largest per-channel peak or RMS level of a buffer.
inline float bufferLevel (const juce::AudioBuffer<float>& buffer, int numChannels, bool peak)
{
    float level = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        level = juce::jmax (level, peak ? buffer.getMagnitude (ch, 0, buffer.getNumSamples())
                                        : buffer.getRMSLevel (ch, 0, buffer.getNumSamples()));
    return level;
}

// A factory preset: parameter values in plain units (choice params use their index).
// Parameters a preset leaves out go back to their defaults.
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
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
        markPresetClean();
    }

    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;
    virtual juce::Colour accent() const = 0;
    virtual bool allowMono() const { return true; }
    virtual juce::String tagline() const { return {}; }
    virtual bool reductionMeter() const { return false; } // left VU shows gain reduction instead of input
    virtual std::unique_ptr<Display> createDisplay();      // defaults to an oscilloscope; see Editor.h

    std::atomic<float>* param (const char* id) { return apvts.getRawParameterValue (id); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0.0f };
    std::atomic<float> inputRms { 0.0f }, inputPeak { 0.0f }, outputRms { 0.0f }, outputPeak { 0.0f };
    ScopeFifo scope;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maxBlockSize) override
    {
        maxBlock = maxBlockSize;
        prepare (sampleRate, maxBlockSize);

        fader.prepare (sampleRate, maxBlockSize, getTotalNumInputChannels(), getLatencySamples(), bypass->load() > 0.5f);
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

        // A host may send more samples than it promised in prepareToPlay. Work through them in pieces
        // no larger than that, so no buffer sized from the announced block has to grow (or overrun)
        // on the audio thread.
        const int total = buffer.getNumSamples();
        const int limit = maxBlock > 0 ? maxBlock : total;
        if (total <= limit)
        {
            processChunk (buffer);
            return;
        }

        for (int start = 0; start < total; start += limit)
        {
            juce::AudioBuffer<float> piece (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                            start, juce::jmin (limit, total - start));
            processChunk (piece);
        }
    }

    // One host block, no larger than the announced size.
    void processChunk (juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = getTotalNumInputChannels();
        const int numSamples = buffer.getNumSamples();
        for (auto ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);

        fader.capture (buffer, numChannels);
        inputRms.store (bufferLevel (buffer, numChannels, false));
        inputPeak.store (bufferLevel (buffer, numChannels, true));

        process (buffer);

        fader.apply (buffer, numChannels, bypass->load() > 0.5f);
        outputRms.store (bufferLevel (buffer, numChannels, false));
        outputPeak.store (bufferLevel (buffer, numChannels, true));
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

    //==============================================================================
    // Presets: factory presets double as host programs; user presets are state files on disk.

    std::vector<Preset> factoryPresets;

    int getNumPrograms() override { return juce::jmax (1, (int) factoryPresets.size()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override
    {
        if (juce::isPositiveAndBelow (index, (int) factoryPresets.size()))
            loadFactoryPreset (index);
    }
    const juce::String getProgramName (int index) override
    {
        return juce::isPositiveAndBelow (index, (int) factoryPresets.size()) ? factoryPresets[(size_t) index].name
                                                                             : juce::String ("Default");
    }
    void changeProgramName (int, const juce::String&) override {}

    void loadFactoryPreset (int index)
    {
        forEachPresetParameter ([] (juce::RangedAudioParameter& p) { setParameter (p, p.getDefaultValue()); });
        for (const auto& [id, value] : factoryPresets[(size_t) index].values)
            if (auto* p = apvts.getParameter (id))
                setParameter (*p, p->convertTo0to1 (value));

        currentProgram = index;
        setPresetName (factoryPresets[(size_t) index].name);
        markPresetClean();
    }

    juce::File userPresetDirectory() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("fenrikos")
            .getChildFile ("Fenix")
            .getChildFile (getName());
    }

    juce::Array<juce::File> userPresets() const
    {
        auto files = userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
        files.sort();
        return files;
    }

    bool saveUserPreset (const juce::String& name)
    {
        const auto file = userPresetDirectory().getChildFile (juce::File::createLegalFileName (name) + presetExtension);
        if (! file.getParentDirectory().createDirectory())
            return false;

        auto xml = apvts.copyState().createXml();
        if (xml == nullptr || ! xml->writeTo (file))
            return false;

        setPresetName (name);
        markPresetClean();
        return true;
    }

    bool loadUserPreset (const juce::File& file)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
            return false;

        // Set values through the parameters (not replaceState) so hosts record the change.
        const auto tree = juce::ValueTree::fromXml (*xml);
        forEachPresetParameter ([&tree] (juce::RangedAudioParameter& p) {
            const auto child = tree.getChildWithProperty ("id", p.getParameterID());
            if (child.isValid())
                setParameter (p, p.convertTo0to1 ((float) child.getProperty ("value")));
        });

        setPresetName (file.getFileNameWithoutExtension());
        markPresetClean();
        return true;
    }

    juce::String presetName() const { return apvts.state.getProperty ("presetName", "Default"); }

    // True when any knob or switch differs from how the preset was loaded or stored (moving a
    // knob back puts it right again). The editor shows an asterisk after the name.
    bool isPresetModified() const
    {
        const juce::ScopedLock lock (cleanLock);
        for (const auto& [parameter, value] : cleanValues)
            if (std::abs (parameter->getValue() - value) > 1.0e-5f)
                return true;
        return false;
    }

    inline static const juce::String presetExtension { ".fenixpreset" };

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

        // A switch keeps whatever raw value the host last sent (0.7 still reads as "on"), and
        // replaceState skips parameters whose on/off state is unchanged, so snap them to 0 or 1.
        for (auto* raw : getParameters())
            if (auto* toggle = dynamic_cast<juce::AudioParameterBool*> (raw))
                if (const auto exact = toggle->get() ? 1.0f : 0.0f; std::abs (raw->getValue() - exact) > 1.0e-6f)
                    toggle->setValueNotifyingHost (exact);

        markPresetClean();
    }

private:
    // Remembers the current values as "the preset as stored". Not for the audio thread.
    void markPresetClean()
    {
        std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
        forEachPresetParameter ([&values] (juce::RangedAudioParameter& p) { values.emplace_back (&p, p.getValue()); });
        const juce::ScopedLock lock (cleanLock);
        cleanValues = std::move (values);
    }

    mutable juce::CriticalSection cleanLock;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> cleanValues;

    template <typename Fn>
    void forEachPresetParameter (Fn&& fn)
    {
        for (auto* raw : getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw); p != nullptr && p->getParameterID() != "bypass")
                fn (*p);
    }

    static void setParameter (juce::RangedAudioParameter& p, float normalised)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost (normalised);
        p.endChangeGesture();
    }

    void setPresetName (const juce::String& name) { apvts.state.setProperty ("presetName", name, nullptr); }

    static Layout withBypass (Layout layout)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
        return layout;
    }

    std::atomic<float>* bypass = nullptr;
    std::atomic<int> currentProgram { 0 };   // hosts read it from their own threads
    int maxBlock = 0;
    BypassFader fader;
};

//==============================================================================
// Parameter helpers

inline std::unique_ptr<juce::AudioParameterFloat> dbParam (const juce::String& id, const juce::String& name,
                                                           float min, float max, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (min, max, 0.1f), def,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; }));
}

inline std::unique_ptr<juce::AudioParameterFloat> percentParam (const juce::String& id, const juce::String& name,
                                                                float max, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, max, 0.1f), def,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }));
}

inline std::unique_ptr<juce::AudioParameterFloat> hzParam (const juce::String& id, const juce::String& name,
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

inline std::unique_ptr<juce::AudioParameterChoice> choiceParam (const juce::String& id, const juce::String& name,
                                                                juce::StringArray choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def);
}

} // namespace fenix
