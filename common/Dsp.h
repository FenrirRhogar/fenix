#pragma once

#include <cmath>
#include <complex>

namespace fenix
{

// M_PI is not standard C++ and MSVC only defines it on request.
constexpr double pi = 3.14159265358979323846;

// RBJ cookbook biquad, transposed direct form II, up to two channels.
// Coefficients are recomputed in place, so it is safe to retune on the audio thread.
struct Biquad
{
    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            z1[ch] = z2[ch] = 0.0f;
    }

    void lowShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 2.0 * pi * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set (A * ((A + 1) - (A - 1) * cs + 2 * sA * alpha),
             2 * A * ((A - 1) - (A + 1) * cs),
             A * ((A + 1) - (A - 1) * cs - 2 * sA * alpha),
             (A + 1) + (A - 1) * cs + 2 * sA * alpha,
             -2 * ((A - 1) + (A + 1) * cs),
             (A + 1) + (A - 1) * cs - 2 * sA * alpha);
    }

    void highShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 2.0 * pi * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set (A * ((A + 1) + (A - 1) * cs + 2 * sA * alpha),
             -2 * A * ((A - 1) + (A + 1) * cs),
             A * ((A + 1) + (A - 1) * cs - 2 * sA * alpha),
             (A + 1) - (A - 1) * cs + 2 * sA * alpha,
             2 * ((A - 1) - (A + 1) * cs),
             (A + 1) - (A - 1) * cs - 2 * sA * alpha);
    }

    void peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
    }

    void lowpass (double fs, double f, double q)
    {
        const double w = 2.0 * pi * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }

    float process (int ch, float x)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    // Magnitude response in dB at `f`, for drawing EQ curves.
    float magnitudeDb (double f, double fs) const
    {
        const auto z1inv = std::polar (1.0, -2.0 * pi * f / fs);
        const auto z2inv = z1inv * z1inv;
        const auto h = ((double) b0 + (double) b1 * z1inv + (double) b2 * z2inv)
                     / (1.0 + (double) a1 * z1inv + (double) a2 * z2inv);
        return (float) (20.0 * std::log10 (std::abs (h) + 1.0e-12));
    }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0);
        b1 = (float) (nb1 / na0);
        b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0);
        a2 = (float) (na2 / na0);
    }

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
};

// One-pole DC blocker (~10 Hz), needed after any asymmetric waveshaper.
struct DcBlocker
{
    void prepare (double fs)
    {
        r = (float) (1.0 - 2.0 * pi * 10.0 / fs);
        for (int ch = 0; ch < 2; ++ch)
            x1[ch] = y1[ch] = 0.0f;
    }

    float process (int ch, float x)
    {
        const float y = x - x1[ch] + r * y1[ch];
        x1[ch] = x;
        y1[ch] = y;
        return y;
    }

private:
    float r = 0.999f, x1[2] {}, y1[2] {};
};

} // namespace fenix
