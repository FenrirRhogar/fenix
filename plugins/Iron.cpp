// Fenix Iron: low-band transformer saturation, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/Iron.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "modules/Iron.h"

using namespace fenix;

class IronProcessor final : public Processor
{
public:
    IronProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = iron::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        iron::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "Output transformer"; }
    juce::Colour accent() const override { return iron::accent(); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override { module.process (buffer, getTotalNumInputChannels()); }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<iron::View> (*this, module); }

private:
    iron::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronProcessor(); }
