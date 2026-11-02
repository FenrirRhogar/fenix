// Renders a plugin's editor to PNG while feeding it a synthetic mix, so the meters and
// displays show live content. Used for README screenshots and visual checks.
//   <Plugin>_snapshot out.png [factoryPresetIndex] [--bench]
// --bench also prints the average time to paint one full frame of the editor.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    // Kick, bass, hats and a wide pad: enough spectral and stereo content to drive every display.
    struct TestMix
    {
        void render (juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                const auto t = (double) sample++ / sampleRate;
                const auto beat = std::fmod (t, 0.5);
                const auto kick = std::sin (juce::MathConstants<double>::twoPi * (50.0 + 90.0 * std::exp (-beat * 30.0)) * beat) * std::exp (-beat * 7.0);
                const auto bass = 0.25 * std::sin (juce::MathConstants<double>::twoPi * 55.0 * t) + 0.12 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t);
                const auto hatEnv = std::exp (-std::fmod (t, 0.125) * 60.0);
                const auto noise = random.nextFloat() * 2.0f - 1.0f;
                hat = 0.6f * (noise - lastNoise) + 0.4f * hat;
                lastNoise = noise;
                const auto padL = 0.08 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * t) + 0.06 * std::sin (juce::MathConstants<double>::twoPi * 495.3 * t);
                const auto padR = 0.08 * std::sin (juce::MathConstants<double>::twoPi * 331.1 * t + 1.0) + 0.06 * std::sin (juce::MathConstants<double>::twoPi * 494.1 * t);
                const auto mono = 0.45 * kick + bass + 0.4 * hatEnv * hat;
                constexpr double level = 0.3; // around -18 dBFS RMS, 0 VU
                buffer.setSample (0, n, (float) (level * (mono + padL)));
                buffer.setSample (1, n, (float) (level * (mono + padR)));
            }
        }

        juce::int64 sample = 0;
        juce::Random random { 42 };
        float hat = 0.0f, lastNoise = 0.0f;
    };
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 400;
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor->prepareToPlay (sampleRate, blockSize);
    if (argc > 2)
        processor->setCurrentProgram (juce::String (argv[2]).getIntValue());

    auto* editor = processor->createEditorAndMakeActive();
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    TestMix mix;

    // ~2 s of audio in real time, two blocks per 60 Hz frame, letting the editor's timers run.
    for (int frame = 0; frame < 120; ++frame)
    {
        for (int b = 0; b < 2; ++b)
        {
            mix.render (buffer, sampleRate);
            processor->processBlock (buffer, midi);
        }
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (16);
    }

    if (argc > 3 && juce::String (argv[3]) == "--bench")
    {
        constexpr int frames = 60;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < frames; ++i)
        {
            mix.render (buffer, sampleRate);
            processor->processBlock (buffer, midi);
            juce::Timer::callPendingTimersSynchronously();
            editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        }
        std::printf ("%s: %.2f ms per frame\n", processor->getName().toRawUTF8(),
                     (juce::Time::getMillisecondCounterHiRes() - start) / frames);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    delete editor;
    return stream.getStatus().wasOk() ? 0 : 1;
}
