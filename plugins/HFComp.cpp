// Fenix HF Comp: compresses only the band above the crossover, leaving the lows and
// mids untouched. For taming harsh cymbals, sibilance and brittle top end on a mix bus.
// Linkwitz-Riley 4th-order split (sums flat when idle), stereo-linked peak detector,
// fast attack / short release, 4:1 with a 6 dB soft knee.

#include "Dsp.h"
#include "Editor.h"

using namespace fenix;

namespace
{
    constexpr float ratio = 4.0f;
    constexpr float kneeDb = 6.0f;
    constexpr float attackMs = 0.5f;
    constexpr float releaseMs = 60.0f;
}

class HFCompProcessor final : public Processor
{
public:
    HFCompProcessor() : Processor (makeLayout())
    {
        threshold = param ("threshold");
        crossover = param ("crossover");
        mix = param ("mix");
        output = param ("output");
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (dbParam ("threshold", "Threshold", -40.0f, 0.0f, -12.0f),
                    hzParam ("crossover", "Crossover", 2000.0f, 16000.0f, 6000.0f),
                    percentParam ("mix", "Mix", 100.0f, 100.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::Colour accent() const override { return juce::Colour (0xff4fc3d9); }
    MeterSpec meterSpec() const override { return { "HF Reduction", 0.0f, 12.0f, " dB" }; }

    void prepare (double sampleRate, int maxBlockSize) override
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

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numChannels = getTotalNumInputChannels();
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

        meter.store (maxReduction);
    }

private:
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HFCompProcessor(); }
