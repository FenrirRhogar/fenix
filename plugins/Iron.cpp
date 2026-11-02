// Fenix Iron: output-transformer colour.
// A transformer core saturates on flux, which grows as frequency falls, so distortion
// lives almost entirely in the low end. The signal is split with a complementary
// low-pass (low + rest == input exactly), only the low band is saturated (slightly
// asymmetric for 2nd harmonic), then a small low resonance bump and a gentle top-end
// softening are added. At 0 % Amount the plugin is a clean pass-through.

#include "Dsp.h"
#include "Editor.h"

using namespace fenix;

namespace
{
    constexpr double fluxCorner = 180.0;
    constexpr float bias = 0.12f;
}

class IronProcessor final : public Processor
{
public:
    IronProcessor() : Processor (makeLayout())
    {
        amount = param ("amount");
        output = param ("output");
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (percentParam ("amount", "Amount", 100.0f, 50.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::Colour accent() const override { return juce::Colour (0xffd9b44a); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        oversampling.reset();
        oversampling.initProcessing ((size_t) maxBlockSize);
        setLatencySamples ((int) oversampling.getLatencyInSamples());
        osRate = sampleRate * (double) oversampling.getOversamplingFactor();

        amountSmooth.reset (osRate, 0.05);
        outGain.reset (osRate, 0.03);
        amountSmooth.setCurrentAndTargetValue (amount->load() / 100.0f);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        for (auto* f : { &flux, &bump, &soften })
            f->reset();
        flux.lowpass (osRate, fluxCorner, 0.707);
        updateTone();
        dc.prepare (sampleRate);
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numChannels = getTotalNumInputChannels();
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                            (size_t) buffer.getNumSamples());

        amountSmooth.setTargetValue (amount->load() / 100.0f);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));

        auto up = oversampling.processSamplesUp (block);
        const auto numUp = up.getNumSamples();

        // Tone filters only need block-rate updates; the saturation drive is per-sample.
        updateTone();

        const float slopeAtZero = 1.0f - juce::square (std::tanh (bias));
        const float biasOffset = std::tanh (bias);

        for (size_t n = 0; n < numUp; ++n)
        {
            const float a = amountSmooth.getNextValue();
            const float drive = 1.0f + 7.0f * a;
            const float out = outGain.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* x = up.getChannelPointer ((size_t) ch);
                const float low = flux.process (ch, x[n]);
                const float rest = x[n] - low;
                const float sat = (std::tanh (drive * low + bias) - biasOffset) / (drive * slopeAtZero);
                const float colored = soften.process (ch, bump.process (ch, rest + low + a * (sat - low)));
                x[n] = colored * out;
            }
        }

        oversampling.processSamplesDown (block);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                x[n] = dc.process (ch, x[n]);
        }
    }

private:
    void updateTone()
    {
        const float a = amountSmooth.getCurrentValue();
        bump.peak (osRate, 55.0, 0.8, 1.5 * a);
        soften.highShelf (osRate, 14000.0, 0.5, -1.0 * a);
    }

    std::atomic<float>* amount = nullptr;
    std::atomic<float>* output = nullptr;

    juce::SmoothedValue<float> amountSmooth, outGain;
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    double osRate = 88200.0;
    Biquad flux, bump, soften;
    DcBlocker dc;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronProcessor(); }
