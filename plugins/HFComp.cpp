// Fenix HF Comp: high-band-only compression, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/HFComp.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "modules/HFComp.h"

using namespace fenix;

class HFCompProcessor final : public Processor
{
public:
    HFCompProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = hfcomp::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        hfcomp::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "High-frequency compressor"; }
    juce::Colour accent() const override { return hfcomp::accent(); }
    bool reductionMeter() const override { return true; }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        module.process (buffer, getTotalNumInputChannels());
        meter.store (module.reduction.load());
    }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<hfcomp::View> (*this, module); }

private:
    hfcomp::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HFCompProcessor(); }
