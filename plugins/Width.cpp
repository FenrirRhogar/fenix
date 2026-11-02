// Fenix Width: mid/side stereo image control.
// Width scales the side signal (0 % = mono, 100 % = untouched, 200 % = doubled).
// Space lifts the side channel below ~700 Hz, a shuffler-style trick that makes a mix
// feel wider and deeper without the phasey top end of plain side boosting.
// The meter shows L/R correlation (1 = mono, 0 = uncorrelated, -1 = out of phase).

#include "Dsp.h"
#include "Editor.h"

using namespace fenix;

namespace
{
    constexpr double spaceFreq = 700.0;
    constexpr float spaceMaxDb = 6.0f;
}

class WidthProcessor final : public Processor
{
public:
    WidthProcessor() : Processor (makeLayout())
    {
        width = param ("width");
        space = param ("space");
        output = param ("output");
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (percentParam ("width", "Width", 200.0f, 100.0f),
                    percentParam ("space", "Space", 100.0f, 0.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::Colour accent() const override { return juce::Colour (0xff6ccf8a); }
    bool allowMono() const override { return false; }
    MeterSpec meterSpec() const override { return { "Correlation", -1.0f, 1.0f, "" }; }

    void prepare (double sampleRate, int) override
    {
        fs = sampleRate;
        for (auto* s : { &sideGain, &outGain })
            s->reset (sampleRate, 0.03);
        spaceDb.reset (sampleRate, 0.05);
        sideGain.setCurrentAndTargetValue (width->load() / 100.0f);
        spaceDb.setCurrentAndTargetValue (space->load() / 100.0f * spaceMaxDb);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        shuffler.reset();
        shuffler.lowShelf (fs, spaceFreq, 0.6, spaceDb.getCurrentValue());

        corrCoef = std::exp (-1.0f / (0.3f * (float) sampleRate));
        sumLR = sumLL = sumRR = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numSamples = buffer.getNumSamples();
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        sideGain.setTargetValue (width->load() / 100.0f);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
        spaceDb.setTargetValue (space->load() / 100.0f * spaceMaxDb);
        if (spaceDb.isSmoothing())
        {
            spaceDb.skip (numSamples);
            shuffler.lowShelf (fs, spaceFreq, 0.6, spaceDb.getCurrentValue());
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float mid = 0.5f * (left[n] + right[n]);
            const float side = shuffler.process (0, 0.5f * (left[n] - right[n]) * sideGain.getNextValue());
            const float out = outGain.getNextValue();
            left[n] = (mid + side) * out;
            right[n] = (mid - side) * out;

            sumLR = left[n] * right[n] + corrCoef * (sumLR - left[n] * right[n]);
            sumLL = left[n] * left[n] + corrCoef * (sumLL - left[n] * left[n]);
            sumRR = right[n] * right[n] + corrCoef * (sumRR - right[n] * right[n]);
        }

        const float energy = std::sqrt (sumLL * sumRR);
        meter.store (energy > 1.0e-9f ? sumLR / energy : 1.0f);
    }

private:
    std::atomic<float>* width = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* output = nullptr;

    double fs = 44100.0;
    juce::SmoothedValue<float> sideGain, spaceDb, outGain;
    Biquad shuffler;
    float corrCoef = 0.0f, sumLR = 0.0f, sumLL = 0.0f, sumRR = 0.0f;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WidthProcessor(); }
