// Fenix Shelf EQ: two broad shelving bands, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/Shelf.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "modules/Shelf.h"

using namespace fenix;

class ShelfProcessor final : public Processor
{
public:
    ShelfProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = shelf::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        shelf::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "Broad shelving equaliser"; }
    juce::Colour accent() const override { return shelf::accent(); }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override { module.process (buffer, getTotalNumInputChannels()); }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<shelf::View> (*this, module); }

private:
    shelf::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShelfProcessor(); }
