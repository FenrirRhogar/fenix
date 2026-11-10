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

    juce::AudioProcessorEditor* createEditor() override
    {
        RackSpec spec;
        spec.left = { { "width", true, marks ({ { 0, "0" }, { 50, "50" }, { 100, "100" }, { 150, "150" }, { 200, "200" } }), "%" },
                      { "space", false, pctMarks(), "%" } };
        spec.right = { { "output", true, dbMarks(), "dB" } };
        spec.centre = Centre::display;
        spec.centreWidth = 210;
        return new RackEditor (*this, std::move (spec));
    }

private:
    width::Module module;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WidthProcessor(); }
