// Fenix Drive: level-dependent harmonic saturation.
// Drive pushes the signal into a variable-knee sigmoid; Density hardens the knee and
// adds DC bias for even-order harmonics. Output is level-compensated so Drive changes
// colour rather than loudness. Runs at 4x oversampling.

#include "Dsp.h"
#include "Editor.h"

using namespace fenix;

class DriveProcessor final : public Processor
{
public:
    DriveProcessor() : Processor (makeLayout())
    {
        drive = param ("drive");
        density = param ("density");
        mix = param ("mix");
        output = param ("output");
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (dbParam ("drive", "Drive", 0.0f, 24.0f, 6.0f),
                    percentParam ("density", "Density", 100.0f, 40.0f),
                    percentParam ("mix", "Mix", 100.0f, 100.0f),
                    dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::String tagline() const override { return "Harmonic saturator"; }
    juce::Colour accent() const override { return juce::Colour (0xffe8743b); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        oversampling.reset();
        oversampling.initProcessing ((size_t) maxBlockSize);
        setLatencySamples ((int) oversampling.getLatencyInSamples());

        const auto osRate = sampleRate * (double) oversampling.getOversamplingFactor();
        for (auto* s : { &driveGain, &densityAmt, &mixAmt, &outGain })
            s->reset (osRate, 0.03);
        driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (drive->load()));
        densityAmt.setCurrentAndTargetValue (density->load() / 100.0f);
        mixAmt.setCurrentAndTargetValue (mix->load() / 100.0f);
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));

        dc.prepare (sampleRate);
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numChannels = getTotalNumInputChannels();
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels,
                                            (size_t) buffer.getNumSamples());

        driveGain.setTargetValue (juce::Decibels::decibelsToGain (drive->load()));
        densityAmt.setTargetValue (density->load() / 100.0f);
        mixAmt.setTargetValue (mix->load() / 100.0f);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));

        auto up = oversampling.processSamplesUp (block);

        for (size_t n = 0; n < up.getNumSamples(); ++n)
        {
            const float g = driveGain.getNextValue();
            const float d = densityAmt.getNextValue();
            const float wet = mixAmt.getNextValue();
            const float out = outGain.getNextValue();

            const float knee = 1.6f + 4.4f * d; // 1.6 = very soft, 6 = close to hard clip
            const float bias = 0.3f * d;
            const float offset = shape (bias, knee);

            // Gain at which a -12 dBFS sine leaves at the same level, averaged over both halves.
            constexpr float ref = 0.25f;
            const float refOut = 0.5f * (shape (g * ref + bias, knee) - shape (-g * ref + bias, knee));
            const float makeup = ref / juce::jmax (refOut, 1.0e-6f);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* x = up.getChannelPointer ((size_t) ch);
                const float dry = x[n];
                const float sat = (shape (g * dry + bias, knee) - offset) * makeup;
                x[n] = (dry + wet * (sat - dry)) * out;
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

private:
    // Algebraic sigmoid x / (1 + |x|^k)^(1/k): k sets how abruptly it bends toward +-1.
    static float shape (float x, float k)
    {
        return x / std::pow (1.0f + std::pow (std::abs (x), k), 1.0f / k);
    }

    std::atomic<float>* drive = nullptr;
    std::atomic<float>* density = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;

    juce::SmoothedValue<float> driveGain, densityAmt, mixAmt, outGain;
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    DcBlocker dc;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DriveProcessor(); }
