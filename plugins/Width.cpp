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

        factoryPresets = {
            { "Neutral", {} },
            { "Wider", { { "width", 130.0f } } },
            { "Super Wide", { { "width", 170.0f }, { "space", 30.0f } } },
            { "Deep Space", { { "width", 110.0f }, { "space", 70.0f } } },
            { "Focused", { { "width", 80.0f }, { "space", 20.0f } } },
            { "Narrow", { { "width", 60.0f } } },
            { "Mono Check", { { "width", 0.0f } } },
        };
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (percentParam ("width", "Width", 200.0f, 100.0f),
                    percentParam ("space", "Space", 100.0f, 0.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::String tagline() const override { return "Stereo image processor"; }
    juce::Colour accent() const override { return juce::Colour (0xff6ccf8a); }
    bool allowMono() const override { return false; }

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

    std::unique_ptr<Display> createDisplay() override;

private:
    std::atomic<float>* width = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* output = nullptr;

    double fs = 44100.0;
    juce::SmoothedValue<float> sideGain, spaceDb, outGain;
    Biquad shuffler;
    float corrCoef = 0.0f, sumLR = 0.0f, sumLL = 0.0f, sumRR = 0.0f;
};

//==============================================================================
// Goniometer with phosphor persistence (mid straight up, side left/right) over a
// correlation bar. A mono signal is a vertical line; wider material spreads sideways.
class WidthDisplay final : public Display
{
public:
    using Display::Display;

protected:
    void onSamples (const float* l, const float* r, int n) override
    {
        // Keep at most one frame's worth of points; older ones live on in the phosphor image.
        const int start = juce::jmax (0, n - (int) points.size());
        numPoints = n - start;
        for (int i = 0; i < numPoints; ++i)
            points[(size_t) i] = { l[start + i] - r[start + i], l[start + i] + r[start + i] };
    }

    void update() override
    {
        const auto target = proc.meter.load();
        correlation += 0.15f * (target - correlation);
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        const auto barArea = area.removeFromBottom (22.0f);
        const auto size = juce::jmin (area.getWidth(), area.getHeight());
        const auto scope = area.withSizeKeepingCentre (size, size);
        const auto c = scope.getCentre();
        const auto radius = size * 0.5f;

        g.setColour (gridColour());
        g.drawEllipse (scope.reduced (1.0f), 1.0f);
        g.drawEllipse (scope.reduced (radius * 0.5f), 0.8f);
        for (auto [dx, dy] : { std::pair { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.7071f, 0.7071f }, { -0.7071f, 0.7071f } })
            g.drawLine (c.x - dx * radius, c.y - dy * radius, c.x + dx * radius, c.y + dy * radius, 0.8f);
        drawGridLabel (g, "M", { c.x - 5.0f, scope.getY() - 1.0f, 12.0f, 11.0f });
        drawGridLabel (g, "L", { c.x - radius * 0.72f - 12.0f, c.y - radius * 0.72f - 8.0f, 12.0f, 11.0f });
        drawGridLabel (g, "R", { c.x + radius * 0.72f + 4.0f, c.y - radius * 0.72f - 8.0f, 12.0f, 11.0f });
        drawGridLabel (g, "-S", { scope.getX() - 18.0f, c.y - 12.0f, 18.0f, 11.0f });
        drawGridLabel (g, "+S", { scope.getRight() + 2.0f, c.y - 12.0f, 18.0f, 11.0f });

        drawPhosphor (g, scope);
        drawCorrelation (g, barArea.reduced (area.getWidth() * 0.12f, 4.0f));

        drawGridLabel (g, "WIDTH " + juce::String (juce::roundToInt (paramValue ("width"))) + "%", area.removeFromTop (12.0f));
        drawGridLabel (g, "SPACE " + juce::String (juce::roundToInt (paramValue ("space"))) + "%", area.removeFromTop (12.0f));
    }

private:
    void drawPhosphor (juce::Graphics& g, juce::Rectangle<float> scope)
    {
        const auto px = juce::jmax (1, (int) scope.getWidth());
        if (phosphor.getWidth() != px)
            phosphor = juce::Image (juce::Image::ARGB, px, px, true);

        phosphor.multiplyAllAlphas (0.8f);
        {
            juce::Graphics pg (phosphor);
            const auto half = (float) px * 0.5f;
            pg.setColour (accent.brighter (0.5f).withAlpha (0.45f));
            for (int i = 0; i < numPoints; ++i)
            {
                // Soft-limit so loud material bends inside the circle instead of leaving it.
                const auto x = half + std::tanh (points[(size_t) i].x * 3.0f) * half * 0.96f;
                const auto y = half - std::tanh (points[(size_t) i].y * 3.0f) * half * 0.96f;
                pg.fillRect (x - 0.75f, y - 0.75f, 1.5f, 1.5f);
            }
        }
        numPoints = 0;
        g.drawImageAt (phosphor, (int) scope.getX(), (int) scope.getY());
    }

    void drawCorrelation (juce::Graphics& g, juce::Rectangle<float> bar)
    {
        const auto track = bar.withSizeKeepingCentre (bar.getWidth(), 4.0f);
        g.setColour (gridColour().withAlpha (0.3f));
        g.fillRoundedRectangle (track, 2.0f);

        const auto xFor = [&] (float v) { return juce::jmap (v, -1.0f, 1.0f, track.getX(), track.getRight()); };
        const auto colour = correlation < 0.0f ? juce::Colour (0xffff5a4a) : accent.brighter (0.3f);
        g.setColour (colour.withAlpha (0.6f));
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (xFor (0.0f), xFor (correlation)), track.getY(),
                                                                              juce::jmax (xFor (0.0f), xFor (correlation)), track.getBottom()), 2.0f);
        draw::led (g, { xFor (correlation), track.getCentreY() }, 4.0f, colour, true);

        g.setColour (gridColour().withAlpha (0.6f));
        g.drawVerticalLine ((int) xFor (0.0f), bar.getY(), bar.getBottom());
        drawGridLabel (g, "-1", bar.withWidth (20.0f).translated (-22.0f, 0.0f), juce::Justification::centredRight);
        drawGridLabel (g, "+1", bar.withX (bar.getRight() + 4.0f).withWidth (20.0f));
    }

    std::array<juce::Point<float>, 2048> points {};
    int numPoints = 0;
    float correlation = 1.0f;
    juce::Image phosphor;
};

std::unique_ptr<Display> WidthProcessor::createDisplay() { return std::make_unique<WidthDisplay> (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WidthProcessor(); }
