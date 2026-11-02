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

// Low-band core curve: unity at 0 % Amount, bending into asymmetric tanh saturation as it rises.
// Normalised so small signals pass at unity gain whatever the amount.
inline float ironCore (float low, float amount)
{
    const float drive = 1.0f + 7.0f * amount;
    const float sat = (std::tanh (drive * low + bias) - std::tanh (bias)) / (drive * (1.0f - juce::square (std::tanh (bias))));
    return low + amount * (sat - low);
}

class IronProcessor final : public Processor
{
public:
    IronProcessor() : Processor (makeLayout())
    {
        amount = param ("amount");
        output = param ("output");

        factoryPresets = {
            { "Default", {} },
            { "Light Touch", { { "amount", 20.0f } } },
            { "Subtle Weight", { { "amount", 35.0f } } },
            { "Warm Iron", { { "amount", 65.0f }, { "output", -0.5f } } },
            { "Heavy Iron", { { "amount", 85.0f }, { "output", -1.0f } } },
            { "Saturated Core", { { "amount", 100.0f }, { "output", -1.5f } } },
        };
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (percentParam ("amount", "Amount", 100.0f, 50.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::String tagline() const override { return "Output transformer"; }
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

        for (size_t n = 0; n < numUp; ++n)
        {
            const float a = amountSmooth.getNextValue();
            const float out = outGain.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* x = up.getChannelPointer ((size_t) ch);
                const float low = flux.process (ch, x[n]);
                const float rest = x[n] - low;
                const float colored = soften.process (ch, bump.process (ch, rest + ironCore (low, a)));
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

    std::unique_ptr<Display> createDisplay() override;

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

//==============================================================================
// Output oscilloscope beside the core's magnetisation curve. A dot rides the curve at the
// current input peak, and the panel warms up as the core is pushed into saturation.
class IronDisplay final : public ScopeDisplay
{
public:
    using ScopeDisplay::ScopeDisplay;

protected:
    void update() override
    {
        const auto peak = proc.inputPeak.load();
        level = peak > level ? peak : level * 0.9f;
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const auto amount = paramValue ("amount") / 100.0f;
        const auto heat = juce::jlimit (0.0f, 1.0f, amount * level * 3.0f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.25f * heat), area.getCentreX(), area.getBottom(),
                                                 accent.withAlpha (0.0f), area.getCentreX(), area.getY(), true));
        g.fillRect (area);

        auto curveArea = area.removeFromRight (area.getHeight() * 1.05f);
        area.removeFromRight (12.0f);

        g.setColour (gridColour());
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        g.drawVerticalLine ((int) (area.getRight() + 6.0f), area.getY(), area.getBottom());
        drawTrace (g, area, 1.6f);
        drawGridLabel (g, "OUTPUT", area.removeFromTop (12.0f));

        // Core curve with input and output both spanning +-1.5 so the knee is visible.
        drawGridLabel (g, "CORE", curveArea.removeFromTop (12.0f));
        const auto plot = curveArea.reduced (4.0f);
        const auto toPoint = [&plot] (float x, float y) {
            return juce::Point<float> (juce::jmap (x, -1.5f, 1.5f, plot.getX(), plot.getRight()),
                                       juce::jmap (y, -1.5f, 1.5f, plot.getBottom(), plot.getY()));
        };
        g.setColour (gridColour());
        g.drawLine ({ toPoint (-1.5f, 0.0f), toPoint (1.5f, 0.0f) }, 0.8f);
        g.drawLine ({ toPoint (0.0f, -1.5f), toPoint (0.0f, 1.5f) }, 0.8f);

        juce::Path curve;
        for (int i = 0; i <= 120; ++i)
        {
            const auto x = -1.5f + 3.0f * (float) i / 120.0f;
            const auto p = toPoint (x, ironCore (x, amount));
            i == 0 ? curve.startNewSubPath (p) : curve.lineTo (p);
        }
        draw::glow (g, curve, accent.brighter (0.4f), 1.8f);

        // The low band carries most of a mix's peak level, so the input peak is a fair stand-in.
        const auto x = juce::jmin (level * 1.5f, 1.5f);
        draw::led (g, toPoint (x, ironCore (x, amount)), 4.0f, accent.brighter (0.6f), level > 0.001f);
    }

private:
    float level = 0.0f;
};

std::unique_ptr<Display> IronProcessor::createDisplay() { return std::make_unique<IronDisplay> (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronProcessor(); }
