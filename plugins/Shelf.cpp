// Fenix Shelf EQ: two broad, stepped-frequency shelves for tonal tilts on a bus.
// Low Q shelves give a gentle slope with a slight counter-dip, closer to passive
// console EQ than to a surgical parametric. Runs at 2x so the top shelf (up to 20 kHz)
// keeps its shape instead of cramping against Nyquist.

#include "Analyser.h"
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

        // Frequency values are indices: low 30/60/90/120/200 Hz, high 6/8/12/16/20 kHz.
        factoryPresets = {
            { "Flat", {} },
            { "Air Lift", { { "highFreq", 3 }, { "highGain", 3.0f } } },
            { "Open Top", { { "highFreq", 4 }, { "highGain", 4.5f } } },
            { "Big Bottom", { { "lowFreq", 1 }, { "lowGain", 3.0f } } },
            { "Smile", { { "lowFreq", 2 }, { "lowGain", 2.0f }, { "highFreq", 2 }, { "highGain", 2.5f } } },
            { "Tame Boom", { { "lowFreq", 4 }, { "lowGain", -3.0f } } },
            { "Vintage Dark", { { "lowFreq", 2 }, { "lowGain", 1.5f }, { "highFreq", 1 }, { "highGain", -3.0f } } },
            { "Master Polish", { { "lowFreq", 0 }, { "lowGain", 1.5f }, { "highFreq", 4 }, { "highGain", 2.0f } } },
        };
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

    std::unique_ptr<Display> createDisplay() override;

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

//==============================================================================
// Response curve of both shelves over a live spectrum of the output, with a marker at
// each selected shelf frequency.
class ShelfDisplay final : public Display
{
public:
    using Display::Display;

protected:
    void onSamples (const float* l, const float* r, int n) override { spectrum.push (l, r, n); }
    void update() override { spectrum.update(); }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        constexpr float range = 15.0f;
        const auto yFor = [&] (float db) { return juce::jmap (db, -range, range, area.getBottom(), area.getY()); };

        g.setFont (draw::mono (10.0f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const auto x = freqAxis::toX (f, area);
            g.setColour (gridColour());
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            drawGridLabel (g, f >= 1000.0f ? juce::String ((int) f / 1000) + "k" : juce::String ((int) f),
                           { x + 2.0f, area.getBottom() - 11.0f, 30.0f, 11.0f });
        }
        for (int db = -12; db <= 12; db += 6)
        {
            g.setColour (db == 0 ? accent.withAlpha (0.22f) : gridColour());
            g.drawHorizontalLine ((int) yFor ((float) db), area.getX(), area.getRight());
            if (db != 0)
                drawGridLabel (g, (db > 0 ? "+" : "") + juce::String (db), { area.getX() + 2.0f, yFor ((float) db) - 11.0f, 30.0f, 11.0f });
        }

        const auto sampleRate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        g.setColour (accent.withAlpha (0.12f));
        g.fillPath (spectrum.path (area, sampleRate, -84.0f, -6.0f, true));
        g.setColour (accent.withAlpha (0.3f));
        g.strokePath (spectrum.path (area, sampleRate, -84.0f, -6.0f, false), juce::PathStrokeType (1.0f));

        // Shelves evaluated at a high rate so the curve shows the oversampled filter, not a cramped one.
        constexpr double curveRate = 192000.0;
        const auto lowHz = lowFreqs[(int) paramValue ("lowFreq")];
        const auto highHz = highFreqs[(int) paramValue ("highFreq")];
        Biquad low, high;
        low.lowShelf (curveRate, lowHz, shelfQ, paramValue ("lowGain"));
        high.highShelf (curveRate, highHz, shelfQ, paramValue ("highGain"));

        juce::Path curve, fill;
        for (auto x = area.getX(); x <= area.getRight(); x += 1.5f)
        {
            const auto f = freqAxis::fromX (x, area);
            const auto y = yFor (juce::jlimit (-range, range, low.magnitudeDb (f, curveRate) + high.magnitudeDb (f, curveRate)));
            curve.isEmpty() ? curve.startNewSubPath (x, y) : curve.lineTo (x, y);
        }
        fill = curve;
        fill.lineTo (area.getRight(), yFor (0.0f));
        fill.lineTo (area.getX(), yFor (0.0f));
        fill.closeSubPath();
        g.setColour (accent.withAlpha (0.2f));
        g.fillPath (fill);
        draw::glow (g, curve, accent.brighter (0.4f), 2.0f);

        for (auto [f, gainId] : { std::pair { lowHz, "lowGain" }, { highHz, "highGain" } })
        {
            const auto x = freqAxis::toX (f, area);
            const auto y = yFor (juce::jlimit (-range, range, low.magnitudeDb (f, curveRate) + high.magnitudeDb (f, curveRate)));
            draw::led (g, { x, y }, 4.0f, accent.brighter (0.5f), std::abs (paramValue (gainId)) > 0.05f);
        }
    }

private:
    Spectrum spectrum;
};

std::unique_ptr<Display> ShelfProcessor::createDisplay() { return std::make_unique<ShelfDisplay> (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShelfProcessor(); }
