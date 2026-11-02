// Fenix Bus: all five Fenix stages in one chain, Drive -> Shelf EQ -> HF Comp -> Width -> Iron,
// each with its own IN switch, plus input and output trims. Presets set the whole chain.
// Every stage runs the same module code as its standalone plugin.

#include "Editor.h"
#include "modules/Drive.h"
#include "modules/HFComp.h"
#include "modules/Iron.h"
#include "modules/Shelf.h"
#include "modules/Width.h"

using namespace fenix;

namespace
{
    // In signal order. The prefix namespaces each stage's parameter ids.
    struct Stage
    {
        juce::String prefix, name;
        juce::Colour colour;
    };

    const Stage stages[] {
        { "drive_", "Drive", drive::accent() },
        { "shelf_", "Shelf EQ", shelf::accent() },
        { "hfcomp_", "HF Comp", hfcomp::accent() },
        { "width_", "Width", width::accent() },
        { "iron_", "Iron", iron::accent() },
    };
    constexpr size_t numStages = std::size (stages);
}

class BusProcessor final : public Processor
{
public:
    BusProcessor() : Processor (makeLayout())
    {
        inputTrim = param ("input");
        outputTrim = param ("output");
        for (size_t i = 0; i < numStages; ++i)
            stageOn[i] = apvts.getRawParameterValue (stages[i].prefix + "on");

        driveModule.attach (apvts, stages[0].prefix);
        shelfModule.attach (apvts, stages[1].prefix);
        hfcompModule.attach (apvts, stages[2].prefix);
        widthModule.attach (apvts, stages[3].prefix);
        ironModule.attach (apvts, stages[4].prefix);

        // Shelf frequencies are indices: low 30/60/90/120/200 Hz, high 6/8/12/16/20 kHz.
        factoryPresets = {
            { "Default", {} },
            { "Warm Master", { { "drive_drive", 4.0f }, { "drive_density", 25.0f }, { "shelf_lowFreq", 0 }, { "shelf_lowGain", 1.5f },
                               { "shelf_highFreq", 4 }, { "shelf_highGain", 2.0f }, { "hfcomp_threshold", -14.0f },
                               { "hfcomp_crossover", 8000.0f }, { "width_width", 115.0f }, { "iron_amount", 35.0f } } },
            { "Mix Polish", { { "drive_drive", 5.0f }, { "drive_density", 20.0f }, { "shelf_lowFreq", 2 }, { "shelf_lowGain", 1.0f },
                              { "shelf_highFreq", 3 }, { "shelf_highGain", 2.0f }, { "hfcomp_threshold", -12.0f },
                              { "hfcomp_crossover", 7000.0f }, { "hfcomp_mix", 70.0f }, { "width_width", 120.0f },
                              { "width_space", 20.0f }, { "iron_amount", 25.0f } } },
            { "Drum Bus Glue", { { "drive_drive", 10.0f }, { "drive_density", 45.0f }, { "drive_mix", 80.0f }, { "shelf_lowFreq", 1 },
                                 { "shelf_lowGain", 2.0f }, { "shelf_highFreq", 2 }, { "shelf_highGain", 1.5f },
                                 { "hfcomp_threshold", -18.0f }, { "hfcomp_crossover", 6000.0f }, { "iron_amount", 55.0f },
                                 { "output", -1.0f } } },
            { "Bass Weight", { { "drive_drive", 9.0f }, { "drive_density", 70.0f }, { "shelf_lowFreq", 1 }, { "shelf_lowGain", 3.0f },
                               { "hfcomp_on", 0.0f }, { "width_width", 90.0f }, { "iron_amount", 70.0f }, { "iron_output", -0.5f } } },
            { "Vocal Sheen", { { "drive_drive", 6.0f }, { "drive_density", 50.0f }, { "drive_mix", 60.0f }, { "shelf_lowFreq", 4 },
                               { "shelf_lowGain", -1.5f }, { "shelf_highFreq", 2 }, { "shelf_highGain", 3.0f },
                               { "hfcomp_threshold", -24.0f }, { "hfcomp_crossover", 5000.0f }, { "width_on", 0.0f },
                               { "iron_amount", 20.0f } } },
            { "Wide & Open", { { "drive_drive", 3.0f }, { "drive_density", 20.0f }, { "shelf_highFreq", 4 }, { "shelf_highGain", 3.5f },
                               { "hfcomp_threshold", -14.0f }, { "hfcomp_crossover", 8000.0f }, { "width_width", 150.0f },
                               { "width_space", 40.0f }, { "iron_amount", 30.0f } } },
            { "Tape & Iron", { { "drive_drive", 12.0f }, { "drive_density", 30.0f }, { "drive_output", -1.0f }, { "shelf_highFreq", 1 },
                               { "shelf_highGain", -2.0f }, { "hfcomp_threshold", -16.0f }, { "width_width", 105.0f },
                               { "iron_amount", 80.0f }, { "iron_output", -1.0f } } },
            { "Lo-Fi Crush", { { "drive_drive", 22.0f }, { "drive_density", 90.0f }, { "drive_mix", 70.0f }, { "shelf_lowFreq", 3 },
                               { "shelf_lowGain", 3.0f }, { "shelf_highFreq", 0 }, { "shelf_highGain", -6.0f },
                               { "hfcomp_threshold", -20.0f }, { "hfcomp_crossover", 3000.0f }, { "width_width", 70.0f },
                               { "iron_amount", 100.0f }, { "output", -2.0f } } },
            { "Clean Chain", { { "drive_on", 0.0f }, { "shelf_on", 0.0f }, { "hfcomp_on", 0.0f }, { "width_on", 0.0f }, { "iron_on", 0.0f } } },
        };
    }

    static Layout makeLayout()
    {
        Layout layout;
        layout.add (dbParam ("input", "Input", -12.0f, 12.0f, 0.0f));

        const auto addStage = [&layout] (size_t i, auto addParameters) {
            layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { stages[i].prefix + "on", 1 }, stages[i].name + " In", true));
            addParameters (layout, stages[i].prefix, stages[i].name + " ");
        };
        addStage (0, drive::addParameters);
        addStage (1, shelf::addParameters);
        addStage (2, hfcomp::addParameters);
        addStage (3, width::addParameters);
        addStage (4, iron::addParameters);

        layout.add (dbParam ("output", "Output", -12.0f, 12.0f, 0.0f));
        return layout;
    }

    juce::String tagline() const override { return "Five-stage colour chain"; }
    juce::Colour accent() const override { return juce::Colour (0xffff6a3d); }
    bool allowMono() const override { return false; }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        int latency = 0;
        const auto prepareStage = [&] (auto& module, size_t i) {
            module.prepare (sampleRate, maxBlockSize);
            faders[i].prepare (sampleRate, maxBlockSize, 2, module.latency(), stageOn[i]->load() < 0.5f);
            latency += module.latency();
        };
        prepareStage (driveModule, 0);
        prepareStage (shelfModule, 1);
        prepareStage (hfcompModule, 2);
        prepareStage (widthModule, 3);
        prepareStage (ironModule, 4);
        setLatencySamples (latency);

        for (auto* s : { &inGain, &outGain })
            s->reset (sampleRate, 0.03);
        inGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputTrim->load()));
        outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputTrim->load()));
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        const int numChannels = getTotalNumInputChannels();
        const int numSamples = buffer.getNumSamples();

        inGain.setTargetValue (juce::Decibels::decibelsToGain (inputTrim->load()));
        inGain.applyGain (buffer, numSamples);

        runStage (driveModule, 0, buffer, numChannels);
        runStage (shelfModule, 1, buffer, numChannels);
        runStage (hfcompModule, 2, buffer, numChannels);
        runStage (widthModule, 3, buffer, numChannels);
        runStage (ironModule, 4, buffer, numChannels);

        outGain.setTargetValue (juce::Decibels::decibelsToGain (outputTrim->load()));
        outGain.applyGain (buffer, numSamples);
    }

    juce::AudioProcessorEditor* createEditor() override;

    bool isStageOn (size_t i) const { return stageOn[i]->load() > 0.5f; }

    drive::Module driveModule;
    shelf::Module shelfModule;
    hfcomp::Module hfcompModule;
    width::Module widthModule;
    iron::Module ironModule;

private:
    // Stages always run, so their state is warm when switched back in; the fader picks the output.
    template <typename Module>
    void runStage (Module& module, size_t i, juce::AudioBuffer<float>& buffer, int numChannels)
    {
        faders[i].capture (buffer, numChannels);
        module.process (buffer, numChannels);
        faders[i].apply (buffer, numChannels, ! isStageOn (i));
    }

    std::atomic<float>* inputTrim = nullptr;
    std::atomic<float>* outputTrim = nullptr;
    std::array<std::atomic<float>*, numStages> stageOn {};
    std::array<BypassFader, numStages> faders;
    juce::SmoothedValue<float> inGain, outGain;
};

juce::AudioProcessorEditor* BusProcessor::createEditor() { return new Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BusProcessor(); }
