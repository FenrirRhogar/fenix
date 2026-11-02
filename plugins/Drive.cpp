// Fenix Drive: harmonic saturation, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/Drive.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "modules/Drive.h"

using namespace fenix;

class DriveProcessor final : public Processor
{
public:
    DriveProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = drive::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        drive::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "Harmonic saturator"; }
    juce::Colour accent() const override { return drive::accent(); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override { module.process (buffer, getTotalNumInputChannels()); }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<drive::View> (*this, module); }

private:
    drive::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DriveProcessor(); }
