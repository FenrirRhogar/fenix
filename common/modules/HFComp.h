#pragma once

// HF Comp: compresses only the band above the crossover, leaving the lows and mids
// untouched. For taming harsh cymbals, sibilance and brittle top end on a mix bus.
// Linkwitz-Riley 4th-order split (sums flat when idle), stereo-linked peak detector,
// fast attack / short release, 4:1 with a 6 dB soft knee.

#include "../Analyser.h"
#include "../Dsp.h"
#include "../Widgets.h"

namespace fenix::hfcomp
{

inline juce::Colour accent() { return juce::Colour (0xff4fc3d9); }

inline void addParameters (Layout& layout, const juce::String& prefix = {}, const juce::String& names = {})
{
    layout.add (dbParam (prefix + "threshold", names + "Threshold", -40.0f, 0.0f, -12.0f),
                hzParam (prefix + "crossover", names + "Crossover", 2000.0f, 16000.0f, 6000.0f),
                percentParam (prefix + "mix", names + "Mix", 100.0f, 100.0f),
                dbParam (prefix + "output", names + "Output", -12.0f, 12.0f, 0.0f));
}

inline std::vector<Preset> presets()
{
    return {
        { "Default", {} },
        { "Cymbal Tamer", { { "threshold", -18.0f }, { "crossover", 6000.0f } } },
        { "De-Harsh Mix", { { "threshold", -15.0f }, { "crossover", 3500.0f }, { "mix", 80.0f } } },
        { "Vocal De-Ess", { { "threshold", -24.0f }, { "crossover", 5000.0f } } },
        { "Bright Guitars", { { "threshold", -20.0f }, { "crossover", 2500.0f }, { "mix", 70.0f } } },
        { "Gentle Polish", { { "threshold", -8.0f }, { "crossover", 10000.0f } } },
        { "Smooth Master", { { "threshold", -12.0f }, { "crossover", 8000.0f }, { "mix", 60.0f } } },
    };
}

class Module
{
public:
    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix = {})
    {
        threshold = apvts.getRawParameterValue (prefix + "threshold");
        crossover = apvts.getRawParameterValue (prefix + "crossover");
        mix = apvts.getRawParameterValue (prefix + "mix");
        output = apvts.getRawParameterValue (prefix + "output");
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        split.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 2 });
        split.reset();

        crossoverHz.reset (sampleRate, 0.05);
        crossoverHz.setCurrentAndTargetValue (crossover->load());
        split.setCutoffFrequency (crossoverHz.getCurrentValue());

        for (auto* s : { &mixAmt, &outGain })
            s->reset (sampleRate, 0.03);
        mixAmt.setCurrentAndTargetValue (mix->load() / 100.0f);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        attackCoef = std::exp (-1.0f / (attackMs * 0.001f * (float) sampleRate));
        releaseCoef = std::exp (-1.0f / (releaseMs * 0.001f * (float) sampleRate));
        envelopeDb = 0.0f;
    }

    int latency() const { return 0; }

    void process (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const int numSamples = buffer.getNumSamples();
        const float thresholdDb = threshold->load();

        crossoverHz.setTargetValue (crossover->load());
        if (crossoverHz.isSmoothing())
        {
            crossoverHz.skip (numSamples);
            split.setCutoffFrequency (crossoverHz.getCurrentValue());
        }
        mixAmt.setTargetValue (mix->load() / 100.0f);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));

        float maxReduction = 0.0f;
        float lows[2] {}, highs[2] {};

        for (int n = 0; n < numSamples; ++n)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                split.processSample (ch, buffer.getSample (ch, n), lows[ch], highs[ch]);
                peak = juce::jmax (peak, std::abs (highs[ch]));
            }

            const float target = gainComputer (juce::Decibels::gainToDecibels (peak, -120.0f) - thresholdDb);
            const float coef = target < envelopeDb ? attackCoef : releaseCoef;
            envelopeDb = target + coef * (envelopeDb - target);
            maxReduction = juce::jmax (maxReduction, -envelopeDb);

            const float wet = mixAmt.getNextValue();
            const float highGain = 1.0f + wet * (juce::Decibels::decibelsToGain (envelopeDb) - 1.0f);
            const float out = outGain.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, n, (lows[ch] + highs[ch] * highGain) * out);
        }

        reduction.store (maxReduction);
    }

    std::atomic<float> reduction { 0.0f }; // dB, block maximum

private:
    static constexpr float ratio = 4.0f, kneeDb = 6.0f, attackMs = 0.5f, releaseMs = 60.0f;

    // Soft-knee static curve: dB above threshold -> gain change in dB (<= 0).
    static float gainComputer (float overDb)
    {
        const float slope = 1.0f / ratio - 1.0f;
        if (2.0f * overDb < -kneeDb)
            return 0.0f;
        if (2.0f * std::abs (overDb) <= kneeDb)
            return slope * juce::square (overDb + kneeDb / 2.0f) / (2.0f * kneeDb);
        return slope * overDb;
    }

    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* crossover = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;

    juce::dsp::LinkwitzRileyFilter<float> split;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> crossoverHz { 6000.0f };
    juce::SmoothedValue<float> mixAmt, outGain;
    float attackCoef = 0.0f, releaseCoef = 0.0f, envelopeDb = 0.0f;
};

//==============================================================================
// Live spectrum with the compressed band shaded above the crossover (brighter as it works
// harder), and a scrolling trace of gain reduction hanging from the top edge.
class View final : public Display
{
public:
    View (Processor& p, Module& m, const juce::String& paramPrefix = {}) : Display (p, hfcomp::accent(), paramPrefix), module (m) {}

protected:
    void onSamples (const float* l, const float* r, int n) override { spectrum.push (l, r, n); }

    void update() override
    {
        spectrum.update();
        history[(size_t) writePos] = module.reduction.load();
        writePos = (writePos + 1) % (int) history.size();
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const auto x = freqAxis::toX (f, area);
            g.setColour (gridColour());
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            drawGridLabel (g, f >= 1000.0f ? juce::String ((int) f / 1000) + "k" : juce::String ((int) f),
                           { x + 2.0f, area.getBottom() - 11.0f, 30.0f, 11.0f });
        }

        const auto sampleRate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const auto reduction = history[(size_t) ((writePos - 1 + (int) history.size()) % (int) history.size())];
        const auto xover = freqAxis::toX (paramValue ("crossover"), area);
        const auto band = area.withLeft (xover);

        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.05f + 0.035f * juce::jmin (reduction, 8.0f)), band.getX(), 0.0f,
                                                 accent.withAlpha (0.02f), band.getRight(), 0.0f, false));
        g.fillRect (band);

        const auto spectrumFill = spectrum.path (area, sampleRate, -84.0f, -6.0f, true);
        g.setColour (accent.withAlpha (0.1f));
        g.fillPath (spectrumFill);
        {
            const juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (band.toNearestInt());
            g.setColour (accent.withAlpha (0.3f));
            g.fillPath (spectrumFill);
        }
        g.setColour (accent.withAlpha (0.45f));
        g.strokePath (spectrumFill, juce::PathStrokeType (1.0f));

        juce::Path line;
        line.startNewSubPath (xover, area.getY());
        line.lineTo (xover, area.getBottom());
        draw::glow (g, line, accent.brighter (0.3f), 1.5f);
        drawGridLabel (g, "XOVER " + juce::String (paramValue ("crossover") / 1000.0f, 1) + "k",
                       { xover + 5.0f, area.getY() + 30.0f, 90.0f, 11.0f });

        // Gain-reduction history: 0 dB at the top edge, 12 dB a third of the way down.
        const auto grArea = area.withHeight (area.getHeight() * 0.33f);
        juce::Path gr;
        const auto count = (int) history.size();
        for (int i = 0; i < count; ++i)
        {
            const auto value = history[(size_t) ((writePos + i) % count)];
            const auto x = juce::jmap ((float) i, 0.0f, (float) (count - 1), area.getX(), area.getRight());
            const auto y = grArea.getY() + juce::jmin (value, 12.0f) / 12.0f * grArea.getHeight();
            i == 0 ? gr.startNewSubPath (x, y) : gr.lineTo (x, y);
        }
        auto grFill = gr;
        grFill.lineTo (area.getRight(), grArea.getY());
        grFill.lineTo (area.getX(), grArea.getY());
        grFill.closeSubPath();
        g.setColour (accent.withAlpha (0.18f));
        g.fillPath (grFill);
        draw::glow (g, gr, accent.brighter (0.5f), 1.6f);

        drawGridLabel (g, "GR " + juce::String (reduction, 1) + " dB", { xover + 5.0f, area.getY() + 44.0f, 90.0f, 11.0f });
    }

private:
    Module& module;
    Spectrum spectrum;
    std::array<float, 240> history {};
    int writePos = 0;
};

} // namespace fenix::hfcomp
