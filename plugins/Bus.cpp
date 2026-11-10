// Fenix Bus: all five Fenix stages in one chain, Drive -> Shelf EQ -> HF Comp -> Width -> Iron,
// each with its own IN switch, plus input and output trims. Presets set the whole chain.
// Every stage runs the same module code as its standalone plugin.

#include "Editor.h"
#include "modules/Drive.h"
#include "modules/HFComp.h"
#include "modules/Iron.h"
#include "modules/Shelf.h"
#include "modules/Width.h"

#include <numeric>

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
    // sizeof, not std::size: older Apple Clang rejects std::size on a non-constexpr array here.
    constexpr size_t numStages = sizeof (stages) / sizeof (stages[0]);
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

//==============================================================================
// Stage selector cap: an LED in the stage colour (lit while the stage is in), latched down when
// its stage's view is on the screen.
class StageTab : public juce::Button
{
public:
    StageTab() : Button ({}) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f, 1.0f).withTrimmedBottom (1.5f);
        rackui::cap (g, r, over, down, selected);
        if (selected || down)
            r.translate (0.0f, 0.5f);
        rackui::led (g, { r.getX() + 13.0f, r.getCentreY() }, 3.0f, colour, stageOn);
        rackui::print (g, getButtonText(), r.withTrimmedLeft (22.0f), rackui::Face::legend, 11.0f, 0.16f,
                       selected ? juce::Colours::white : rackui::colour::inkSoft, juce::Justification::centred);
    }

    juce::Colour colour;
    bool selected = false, stageOn = true;
};

//==============================================================================
// Fenix Bus as one rack unit. Top: nameplate, input VU, stage tabs over the screen, output VU, IN.
// Below: one module per stage (its LED switch carries the name), knobs in columns of two, then trims.
class BusEditor final : public RackBase
{
public:
    static constexpr int plateWidth = standardPlateWidth, plateHeight = 570, knobSize = 44, columnPitch = 104, moduleGap = 14;

    explicit BusEditor (BusProcessor& p) : RackBase (p), bus (p)
    {
        views[0] = std::make_unique<drive::View> (p, p.driveModule, stages[0].prefix);
        views[1] = std::make_unique<shelf::View> (p, p.shelfModule, stages[1].prefix);
        views[2] = std::make_unique<hfcomp::View> (p, p.hfcompModule, stages[2].prefix);
        views[3] = std::make_unique<width::View> (p, p.widthModule, stages[3].prefix);
        views[4] = std::make_unique<iron::View> (p, p.ironModule, stages[4].prefix);

        using M = std::vector<rackui::Mark>;
        const M pct = marks ({ { 0, "0" }, { 50, "50" }, { 100, "100" } });
        const M db = marks ({ { -12, "-12" }, { 0, "0" }, { 12, "+12" } });
        struct K { const char* id; const char* name; const char* unit; M m; bool metal; };
        const std::vector<std::vector<K>> layouts {
            { { "drive", "DRIVE", "dB", marks ({ { 0, "0" }, { 12, "12" }, { 24, "24" } }), true }, { "density", "DENSITY", "%", pct, false },
              { "mix", "MIX", "%", pct, false }, { "output", "OUTPUT", "dB", db, false } },
            { { "lowFreq", "LOW FREQ", "Hz", marks ({ { 0, "30" }, { 1, "60" }, { 2, "90" }, { 3, "120" }, { 4, "200" } }), true },
              { "lowGain", "LOW", "dB", db, false },
              { "highFreq", "HIGH FREQ", "kHz", marks ({ { 0, "6" }, { 1, "8" }, { 2, "12" }, { 3, "16" }, { 4, "20" } }), true },
              { "highGain", "HIGH", "dB", db, false }, { "output", "OUTPUT", "dB", db, false } },
            { { "threshold", "THRESHOLD", "dB", marks ({ { -40, "-40" }, { -20, "-20" }, { 0, "0" } }), true },
              { "crossover", "CROSSOVER", "kHz", marks ({ { 2000, "2" }, { 6000, "6" }, { 16000, "16" } }), false },
              { "mix", "MIX", "%", pct, false }, { "output", "OUTPUT", "dB", db, false } },
            { { "width", "WIDTH", "%", marks ({ { 0, "0" }, { 100, "100" }, { 200, "200" } }), true }, { "space", "SPACE", "%", pct, false },
              { "output", "OUTPUT", "dB", db, false } },
            { { "amount", "AMOUNT", "%", pct, true }, { "output", "OUTPUT", "dB", db, false } },
        };

        for (size_t i = 0; i < numStages; ++i)
        {
            addChildComponent (*views[i]);
            tabs[i].setButtonText (stages[i].name.toUpperCase());
            tabs[i].colour = stages[i].colour;
            tabs[i].onClick = [this, i] { select (i); };
            addAndMakeVisible (tabs[i]);

            switches[i] = std::make_unique<rackui::LedToggle> (stages[i].name.toUpperCase(), stages[i].colour);
            switchAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, stages[i].prefix + "on", *switches[i]);
            addAndMakeVisible (*switches[i]);

            for (const auto& k : layouts[i])
                stageKnobs[i].push_back (&addKnob (stages[i].prefix + k.id, k.metal, k.m, k.unit, k.name, stages[i].colour, (float) knobSize, (float) knobSize));
        }
        for (auto* id : { "input", "output" })
            trimKnobs.push_back (&addKnob (id, false, db, "dB", juce::String (id).toUpperCase(), rackui::colour::inkDim, (float) knobSize, (float) knobSize));

        inputVu = makeVu ("INPUT", p.inputRms, p.inputPeak);
        outputVu = makeVu ("OUTPUT", p.outputRms, p.outputPeak);
        addAndMakeVisible (*inputVu);
        addAndMakeVisible (*outputVu);

        select ((size_t) juce::jlimit (0, (int) numStages - 1, (int) p.apvts.state.getProperty ("view", 0)));
        start (plateWidth, plateHeight);
    }

protected:
    void layout() override
    {
        const int left = ear + 30, right = ear + plateWidth - 30;
        // Top row.
        inputVu->setBounds (left + 206 + 30, 30, 172, 172);
        outputVu->setBounds (right - 60 - 30 - 172, 30, 172, 172);
        const int screenX = inputVu->getRight() + 26, screenW = outputVu->getX() - 26 - screenX;
        const int tabW = (screenW - (int) (numStages - 1) * 6) / (int) numStages;
        for (size_t i = 0; i < numStages; ++i)
        {
            tabs[i].setBounds (screenX + (int) i * (tabW + 6), 28, tabW, 26);
            views[i]->setBounds (screenX - 2, 60, screenW + 4, 150);
        }
        inSwitch.setBounds (right - 50, 72, 50, 64);
        ruleY = 236;

        // Modules: columns of two knobs; the whole row is centred.
        std::vector<int> columns;
        for (size_t i = 0; i < numStages; ++i)
            columns.push_back ((int) (stageKnobs[i].size() + 1) / 2);
        columns.push_back (1);
        int total = -moduleGap;
        for (auto c : columns)
            total += c * columnPitch + moduleGap;
        int x = ear + (plateWidth - total) / 2;
        const int headerY = ruleY + 18, knobY = headerY + 26;
        const auto place = [&] (const std::vector<rackui::Knob*>& ks, int columnCount) {
            for (size_t k = 0; k < ks.size(); ++k)
            {
                const auto s = ks[k]->preferredSize();
                const int cx = x + (int) (k / 2) * columnPitch + columnPitch / 2;
                ks[k]->setBounds (cx - s.x / 2, knobY + (int) (k % 2) * (s.y + 6), s.x, s.y);
            }
            const int start = x;
            x += columnCount * columnPitch + moduleGap;
            return start;
        };
        moduleX.clear();
        for (size_t i = 0; i < numStages; ++i)
        {
            const int mx = place (stageKnobs[i], columns[i]);
            moduleX.push_back (mx);
            switches[i]->setBounds (mx + 14, headerY, columns[i] * columnPitch - 20, 20);
        }
        moduleX.push_back (place (trimKnobs, 1));
    }

    void paintPanel (juce::Graphics& g) override
    {
        rackui::rule (g, (float) ear + 30.0f, (float) (ear + plateWidth - 30), (float) ruleY);
        rackui::print (g, "TRIM", { (float) moduleX.back() + 14.0f, (float) ruleY + 18.0f, 100.0f, 20.0f }, rackui::Face::legendBold, 13.0f, 0.2f,
                       rackui::colour::ink, juce::Justification::centredLeft);
        for (size_t i = 1; i < moduleX.size(); ++i)
            rackui::divider (g, (float) moduleX[i] - (float) moduleGap * 0.5f - 1.0f, (float) ruleY + 22.0f, 260.0f);
    }

    void tick() override
    {
        constexpr float dt = 1.0f / 60.0f;
        for (auto* vu : { inputVu.get(), outputVu.get() })
            if (vu->tick (dt))
                vu->repaint();
        views[selected]->tick();
        views[selected]->repaint();

        for (size_t i = 0; i < numStages; ++i)
        {
            const bool on = bus.isStageOn (i);
            if (lastOn[i] == (on ? 1 : 0))
                continue;
            lastOn[i] = on ? 1 : 0;
            for (auto* knob : stageKnobs[i])
                knob->setAlpha (on ? 1.0f : 0.45f);
            tabs[i].stageOn = on;
            tabs[i].repaint();
        }
    }

private:
    void select (size_t index)
    {
        selected = index;
        for (size_t i = 0; i < numStages; ++i)
        {
            views[i]->setVisible (i == index);
            tabs[i].selected = i == index;
            tabs[i].repaint();
        }
        bus.apvts.state.setProperty ("view", (int) index, nullptr);
    }

    BusProcessor& bus;
    std::array<std::unique_ptr<Display>, numStages> views;
    std::array<StageTab, numStages> tabs;
    std::array<std::unique_ptr<rackui::LedToggle>, numStages> switches;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, numStages> switchAttachments;
    std::array<std::vector<rackui::Knob*>, numStages> stageKnobs;
    std::vector<rackui::Knob*> trimKnobs;
    std::unique_ptr<rackui::VuMeter> inputVu, outputVu;
    std::array<int, numStages> lastOn { -1, -1, -1, -1, -1 };
    std::vector<int> moduleX;
    int ruleY = 236;
    size_t selected = 0;
};

juce::AudioProcessorEditor* BusProcessor::createEditor() { return new BusEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BusProcessor(); }
