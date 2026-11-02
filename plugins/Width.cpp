// Fenix Width: mid/side width and low-band space, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/Width.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "modules/Width.h"

using namespace fenix;

class WidthProcessor final : public Processor
{
public:
    WidthProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = width::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        width::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "Stereo image processor"; }
    juce::Colour accent() const override { return width::accent(); }
    bool allowMono() const override { return false; }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override { module.process (buffer, getTotalNumInputChannels()); }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<width::View> (*this, module); }

private:
    width::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WidthProcessor(); }
