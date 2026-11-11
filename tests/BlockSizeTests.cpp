// A host may send a block larger than the one it announced in prepareToPlay. Every Fenix plugin must
// then work through it in pieces and give exactly the audio it would give if the host had sent those
// pieces itself, without growing or overrunning a buffer sized from the announced block.
// Build the <Plugin>_tests targets (FENIX_TESTS=ON); they exit non-zero on a failure. Build them with
// -fsanitize=address to catch bad memory accesses.

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr int announced = 128, total = 1000, rounds = 3;
    int failures = 0;

    const auto make = []
    {
        std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
        plugin->setPlayConfigDetails (2, 2, 48000.0, announced);
        plugin->prepareToPlay (48000.0, announced);

        // The same, non-trivial settings in every instance; effects engaged, nothing bypassed.
        juce::Random values (5);
        for (auto* parameter : plugin->getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            if (ranged == nullptr)
                continue;
            const auto id = ranged->getParameterID();
            ranged->setValueNotifyingHost (id == "bypass" ? 0.0f : id.endsWith ("_on") ? 1.0f : values.nextFloat());
        }
        return plugin;
    };

    auto whole = make(), pieces = make();
    juce::AudioBuffer<float> a (2, total), b (2, total);
    juce::Random noise (11);
    juce::MidiBuffer midi;
    float worst = 0.0f;

    for (int round = 0; round < rounds; ++round)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < total; ++n)
                a.setSample (ch, n, 0.5f * (noise.nextFloat() * 2.0f - 1.0f));
        b.makeCopyOf (a);

        whole->processBlock (a, midi);
        for (int start = 0; start < total; start += announced)
        {
            juce::AudioBuffer<float> piece (b.getArrayOfWritePointers(), 2, start, juce::jmin (announced, total - start));
            pieces->processBlock (piece, midi);
        }

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < total; ++n)
            {
                const float x = a.getSample (ch, n), y = b.getSample (ch, n);
                worst = std::isfinite (x) && std::isfinite (y) ? juce::jmax (worst, std::abs (x - y)) : 1.0e9f;
            }
    }

    if (worst > 1.0e-6f)
    {
        std::printf ("%s: an oversized block differs from the same audio sent in pieces (worst %g)\n",
                     whole->getName().toRawUTF8(), (double) worst);
        ++failures;
    }
    else
    {
        std::printf ("%s: passed\n", whole->getName().toRawUTF8());
    }
    return failures == 0 ? 0 : 1;
}
