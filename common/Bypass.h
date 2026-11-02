#pragma once

#include <juce_dsp/juce_dsp.h>

namespace fenix
{

// Crossfades between processed audio and the input delayed by the processing latency, so
// switching an effect in or out never clicks or shifts timing.
// Call capture() before processing and apply() after.
class BypassFader
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels, int latencySamples, bool bypassed)
    {
        dry.setSize (juce::jmax (1, numChannels), maxBlockSize);
        delay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) juce::jmax (1, numChannels) });
        delay.setDelay ((float) latencySamples);
        mix.reset (sampleRate, 0.02);
        mix.setCurrentAndTargetValue (bypassed ? 1.0f : 0.0f);
    }

    void capture (const juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const int numSamples = buffer.getNumSamples();
        if (dry.getNumSamples() < numSamples)
            dry.setSize (dry.getNumChannels(), numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    void apply (juce::AudioBuffer<float>& buffer, int numChannels, bool bypassed)
    {
        mix.setTargetValue (bypassed ? 1.0f : 0.0f);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float b = mix.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                delay.pushSample (ch, dry.getSample (ch, n));
                const float delayed = delay.popSample (ch);
                const float wet = buffer.getSample (ch, n);
                buffer.setSample (ch, n, wet + b * (delayed - wet));
            }
        }
    }

private:
    juce::AudioBuffer<float> dry;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> delay { 4096 };
    juce::SmoothedValue<float> mix;
};

} // namespace fenix
