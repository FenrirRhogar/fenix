#pragma once

// Drive: level-dependent harmonic saturation.
// Drive pushes the signal into a variable-knee sigmoid; Density hardens the knee and
// adds DC bias for even-order harmonics. Output is level-compensated so Drive changes
// colour rather than loudness. Runs at 4x oversampling.

#include "../Dsp.h"
#include "../Widgets.h"

namespace fenix::drive
{

inline juce::Colour accent() { return juce::Colour (0xffe8743b); }

// The static saturation curve for a given drive gain and density (0..1), level-compensated
// so a -12 dBFS sine leaves at the same level it came in.
struct Curve
{
    Curve (float driveGain, float density)
        : gain (driveGain), knee (1.6f + 4.4f * density), bias (0.3f * density), offset (shape (bias, knee))
    {
        constexpr float ref = 0.25f;
        const float refOut = 0.5f * (shape (gain * ref + bias, knee) - shape (-gain * ref + bias, knee));
        makeup = ref / juce::jmax (refOut, 1.0e-6f);
    }

    float operator() (float x) const { return (shape (gain * x + bias, knee) - offset) * makeup; }

    // Algebraic sigmoid x / (1 + |x|^k)^(1/k): k sets how abruptly it bends toward +-1.
    // Density raises k (harder knee) and the bias (even harmonics).
    static float shape (float x, float k)
    {
        return x / std::pow (1.0f + std::pow (std::abs (x), k), 1.0f / k);
    }

    float gain, knee, bias, offset, makeup = 1.0f;
};

inline void addParameters (Layout& layout, const juce::String& prefix = {}, const juce::String& names = {})
{
    layout.add (dbParam (prefix + "drive", names + "Drive", 0.0f, 24.0f, 6.0f),
                percentParam (prefix + "density", names + "Density", 100.0f, 40.0f),
                percentParam (prefix + "mix", names + "Mix", 100.0f, 100.0f),
                dbParam (prefix + "output", names + "Output", -12.0f, 12.0f, 0.0f));
}

inline std::vector<Preset> presets()
{
    return {
        { "Default", {} },
        { "Gentle Warmth", { { "drive", 4.0f }, { "density", 20.0f } } },
        { "Bus Glue", { { "drive", 8.0f }, { "density", 35.0f }, { "output", -0.5f } } },
        { "Tape Push", { { "drive", 12.0f }, { "density", 25.0f }, { "output", -1.0f } } },
        { "Thick Bass", { { "drive", 10.0f }, { "density", 70.0f } } },
        { "Vocal Edge", { { "drive", 7.0f }, { "density", 55.0f }, { "mix", 70.0f } } },
        { "Drum Crunch", { { "drive", 18.0f }, { "density", 80.0f }, { "mix", 60.0f } } },
        { "Parallel Grit", { { "drive", 24.0f }, { "density", 100.0f }, { "mix", 30.0f } } },
    };
}

class Module
{
public:
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix = {})
    {
        drive = apvts.getRawParameterValue (prefix + "drive");
        density = apvts.getRawParameterValue (prefix + "density");
        mix = apvts.getRawParameterValue (prefix + "mix");
        output = apvts.getRawParameterValue (prefix + "output");
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        oversampling.reset();
        oversampling.initProcessing ((size_t) maxBlockSize);

        const auto osRate = sampleRate * (double) oversampling.getOversamplingFactor();
        for (auto* s : { &driveGain, &densityAmt, &mixAmt, &outGain })
            s->reset (osRate, 0.03);
        driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (drive->load()));
        densityAmt.setCurrentAndTargetValue (density->load() / 100.0f);
        mixAmt.setCurrentAndTargetValue (mix->load() / 100.0f);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        dc.prepare (sampleRate);
    }

    int latency() const { return (int) oversampling.getLatencyInSamples(); }

    void process (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        inputPeak.store (bufferLevel (buffer, numChannels, true));
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                            (size_t) buffer.getNumSamples());

        driveGain.setTargetValue (juce::Decibels::decibelsToGain (drive->load()));
        densityAmt.setTargetValue (density->load() / 100.0f);
        mixAmt.setTargetValue (mix->load() / 100.0f);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));

        auto up = oversampling.processSamplesUp (block);

        for (size_t n = 0; n < up.getNumSamples(); ++n)
        {
            const float wet = mixAmt.getNextValue();
            const float out = outGain.getNextValue();
            const Curve curve (driveGain.getNextValue(), densityAmt.getNextValue());

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* x = up.getChannelPointer ((size_t) ch);
                const float dry = x[n];
                x[n] = (dry + wet * (curve (dry) - dry)) * out;
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

    std::atomic<float> inputPeak { 0.0f };

private:
    std::atomic<float>* drive = nullptr;
    std::atomic<float>* density = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;

    juce::SmoothedValue<float> driveGain, densityAmt, mixAmt, outGain;
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    DcBlocker dc;
};

//==============================================================================
// Transfer curve (input level -> output level) against the straight unity line. The shaded gap
// between them is the saturation; glowing dots ride the curve at the current input peak.
class View final : public Display
{
public:
    View (Processor& p, Module& m, const juce::String& paramPrefix = {}) : Display (p, drive::accent(), paramPrefix), module (m) {}

protected:
    void update() override
    {
        const auto peak = module.inputPeak.load();
        level = peak > level ? peak : level * 0.9f;
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const auto plot = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight() * 1.9f), area.getHeight());
        const auto toPoint = [&plot] (float x, float y) {
            return juce::Point<float> (juce::jmap (x, -1.0f, 1.0f, plot.getX(), plot.getRight()),
                                       juce::jmap (juce::jlimit (-1.15f, 1.15f, y), -1.15f, 1.15f, plot.getBottom(), plot.getY()));
        };

        g.setColour (gridColour());
        for (int i = -1; i <= 1; ++i)
        {
            const auto v = 0.5f * (float) i;
            const auto thickness = i == 0 ? 1.2f : 0.8f;
            g.drawLine ({ toPoint (v, -1.15f), toPoint (v, 1.15f) }, thickness);
            g.drawLine ({ toPoint (-1.0f, v), toPoint (1.0f, v) }, thickness);
        }

        const Curve curve (juce::Decibels::decibelsToGain (paramValue ("drive")), paramValue ("density") / 100.0f);
        const auto mix = paramValue ("mix") / 100.0f;
        const auto transfer = [&] (float x) { return x + mix * (curve (x) - x); };

        juce::Path line, gap;
        for (int i = 0; i <= 160; ++i)
        {
            const auto x = -1.0f + 2.0f * (float) i / 160.0f;
            const auto p = toPoint (x, transfer (x));
            i == 0 ? line.startNewSubPath (p) : line.lineTo (p);
        }
        gap = line;
        gap.lineTo (toPoint (1.0f, 1.0f));
        gap.lineTo (toPoint (-1.0f, -1.0f));
        gap.closeSubPath();

        g.setColour (accent.withAlpha (0.16f));
        g.fillPath (gap);
        const float dashes[] { 4.0f, 4.0f };
        juce::Path unity, dashed;
        unity.startNewSubPath (toPoint (-1.0f, -1.0f));
        unity.lineTo (toPoint (1.0f, 1.0f));
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, dashes, 2);
        g.setColour (accent.withAlpha (0.35f));
        g.fillPath (dashed);
        draw::glow (g, line, accent.brighter (0.35f), 2.0f);

        const auto x = juce::jmin (level, 1.0f);
        for (auto sign : { -1.0f, 1.0f })
            draw::led (g, toPoint (sign * x, transfer (sign * x)), 4.0f, accent.brighter (0.6f), level > 0.001f);

        drawGridLabel (g, "TRANSFER", area.removeFromTop (12.0f));
        drawGridLabel (g, "IN " + juce::String (juce::Decibels::gainToDecibels (level, -60.0f), 1) + " dBFS", area.removeFromBottom (12.0f),
                       juce::Justification::centredRight);
    }

private:
    Module& module;
    float level = 0.0f;
};

} // namespace fenix::drive
