// Fenix Shelf EQ: two broad, stepped-frequency shelves for tonal tilts on a bus.
// Low Q shelves give a gentle slope with a slight counter-dip, closer to passive
// console EQ than to a surgical parametric. Runs at 2x so the top shelf (up to 20 kHz)
// keeps its shape instead of cramping against Nyquist.

#include "Dsp.h"
#include "Editor.h"

using namespace fenix;

namespace
{
    const float lowFreqs[] { 30.0f, 60.0f, 90.0f, 120.0f, 200.0f };
    const float highFreqs[] { 6000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f };
    constexpr double shelfQ = 0.6;
    constexpr int updateInterval = 32;
}

class ShelfProcessor final : public Processor
{
public:
    ShelfProcessor() : Processor (makeLayout())
    {
        lowGain = param ("lowGain");
        lowFreq = param ("lowFreq");
        highGain = param ("highGain");
        highFreq = param ("highFreq");
        output = param ("output");
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (choiceParam ("lowFreq", "Low Freq", { "30 Hz", "60 Hz", "90 Hz", "120 Hz", "200 Hz" }, 1),
                    dbParam ("lowGain", "Low", -12.0f, 12.0f, 0.0f),
                    choiceParam ("highFreq", "High Freq", { "6 kHz", "8 kHz", "12 kHz", "16 kHz", "20 kHz" }, 3),
                    dbParam ("highGain", "High", -12.0f, 12.0f, 0.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::String tagline() const override { return "Broad shelving equaliser"; }
    juce::Colour accent() const override { return juce::Colour (0xff9b7cf0); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        oversampling.reset();
        oversampling.initProcessing ((size_t) maxBlockSize);
        setLatencySamples ((int) oversampling.getLatencyInSamples());
        osRate = sampleRate * (double) oversampling.getOversamplingFactor();

        for (auto* s : { &lowDb, &highDb })
            s->reset (osRate, 0.05);
        for (auto* s : { &lowHz, &highHz })
            s->reset (osRate, 0.08);
        outGain.reset (osRate, 0.03);

        lowDb.setCurrentAndTargetValue (lowGain->load());
        highDb.setCurrentAndTargetValue (highGain->load());
        lowHz.setCurrentAndTargetValue (lowFreqs[(int) lowFreq->load()]);
        highHz.setCurrentAndTargetValue (highFreqs[(int) highFreq->load()]);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        low.reset();
        high.reset();
        updateCoefficients();
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numChannels = getTotalNumInputChannels();
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                            (size_t) buffer.getNumSamples());

        lowDb.setTargetValue (lowGain->load());
        highDb.setTargetValue (highGain->load());
        lowHz.setTargetValue (lowFreqs[(int) lowFreq->load()]);
        highHz.setTargetValue (highFreqs[(int) highFreq->load()]);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));

        auto up = oversampling.processSamplesUp (block);
        const int total = (int) up.getNumSamples();

        for (int start = 0; start < total; start += updateInterval)
        {
            const int len = juce::jmin (updateInterval, total - start);
            if (lowDb.isSmoothing() || highDb.isSmoothing() || lowHz.isSmoothing() || highHz.isSmoothing())
            {
                for (auto* s : { &lowDb, &highDb })
                    s->skip (len);
                for (auto* s : { &lowHz, &highHz })
                    s->skip (len);
                updateCoefficients();
            }

            for (int n = start; n < start + len; ++n)
            {
                const float out = outGain.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* x = up.getChannelPointer ((size_t) ch);
                    x[n] = high.process (ch, low.process (ch, x[n])) * out;
                }
            }
        }

        oversampling.processSamplesDown (block);
    }

private:
    void updateCoefficients()
    {
        low.lowShelf (osRate, lowHz.getCurrentValue(), shelfQ, lowDb.getCurrentValue());
        high.highShelf (osRate, highHz.getCurrentValue(), shelfQ, highDb.getCurrentValue());
    }

    std::atomic<float>* lowGain = nullptr;
    std::atomic<float>* lowFreq = nullptr;
    std::atomic<float>* highGain = nullptr;
    std::atomic<float>* highFreq = nullptr;
    std::atomic<float>* output = nullptr;

    juce::SmoothedValue<float> lowDb, highDb, outGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowHz { 60.0f }, highHz { 16000.0f };
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    double osRate = 88200.0;
    Biquad low, high;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShelfProcessor(); }
