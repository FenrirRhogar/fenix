#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace fenix
{

// Log-frequency axis shared by every frequency-domain display (20 Hz .. 20 kHz).
namespace freqAxis
{
    constexpr float lo = 20.0f, hi = 20000.0f;

    inline float toX (float freq, juce::Rectangle<float> area)
    {
        return area.getX() + area.getWidth() * std::log (freq / lo) / std::log (hi / lo);
    }

    inline float fromX (float x, juce::Rectangle<float> area)
    {
        return lo * std::pow (hi / lo, (x - area.getX()) / area.getWidth());
    }
}

// 4096-point FFT of the mono sum with peak-hold-and-fall smoothing, tilted +3 dB/oct
// around 1 kHz so a typical mix reads roughly flat.
class Spectrum
{
public:
    static constexpr int order = 12, size = 1 << order;

    Spectrum() { smoothed.fill (-120.0f); }

    void push (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) pos] = 0.5f * (l[i] + r[i]);
            pos = (pos + 1) % size;
        }
    }

    void update()
    {
        for (int i = 0; i < size; ++i)
            fftData[(size_t) i] = ring[(size_t) ((pos + i) % size)];
        std::fill (fftData.begin() + size, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int bin = 0; bin < size / 2; ++bin)
        {
            const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) bin] * 4.0f / (float) size, -120.0f);
            auto& s = smoothed[(size_t) bin];
            s = db > s ? 0.5f * (db + s) : juce::jmax (db, s - 0.9f);
        }
    }

    juce::Path path (juce::Rectangle<float> area, double sampleRate, float minDb, float maxDb, bool closed) const
    {
        juce::Path p;
        const auto binWidth = (float) sampleRate / (float) size;
        const auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, area.getBottom(), area.getY()); };

        for (auto x = area.getX(); x <= area.getRight(); x += 2.0f)
        {
            const auto f0 = freqAxis::fromX (x, area), f1 = freqAxis::fromX (x + 2.0f, area);
            const int b0 = juce::jlimit (1, size / 2 - 1, (int) (f0 / binWidth));
            const int b1 = juce::jlimit (b0 + 1, size / 2, (int) (f1 / binWidth) + 1);
            float level = -120.0f;
            for (int b = b0; b < b1; ++b)
                level = juce::jmax (level, smoothed[(size_t) b]);
            level += 3.0f * std::log2 (f0 / 1000.0f);

            const auto y = yFor (level);
            if (p.isEmpty())
            {
                closed ? p.startNewSubPath (x, area.getBottom()) : p.startNewSubPath (x, y);
                if (closed)
                    p.lineTo (x, y);
            }
            else
            {
                p.lineTo (x, y);
            }
        }

        if (closed)
        {
            p.lineTo (area.getRight(), area.getBottom());
            p.closeSubPath();
        }
        return p;
    }

private:
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, size> ring {};
    std::array<float, size * 2> fftData {};
    std::array<float, size / 2> smoothed {};
    int pos = 0;
};

} // namespace fenix
