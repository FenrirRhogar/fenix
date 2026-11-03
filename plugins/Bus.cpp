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
// Pill tab for choosing which stage's display is shown, with a dot that lights while the stage is in.
class StageTab : public juce::Button
{
public:
    StageTab() : Button ({}) {}

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto corner = area.getHeight() * 0.5f;
        g.setColour (selected ? colour.withAlpha (0.2f) : colours::button.brighter (over ? 0.1f : 0.0f));
        g.fillRoundedRectangle (area, corner);
        g.setColour (selected ? colour.withAlpha (0.8f) : colours::cardEdge);
        g.drawRoundedRectangle (area, corner, 1.0f);

        draw::led (g, { area.getX() + 14.0f, area.getCentreY() }, 3.0f, colour, stageOn);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.12f));
        g.setColour (selected ? colour.brighter (0.4f) : (over ? colours::engrave : colours::engraveDim));
        g.drawText (getButtonText(), area.withTrimmedLeft (22.0f), juce::Justification::centred);
    }

    juce::Colour colour;
    bool selected = false, stageOn = true;
};

//==============================================================================
// Bus faceplate: tabs choose the stage shown on the display; below, one card per stage with its
// IN switch and knobs in signal order, then a trim card. Clicking a card's title selects its display.
class BusEditor final : public EditorBase
{
public:
    explicit BusEditor (BusProcessor& p) : EditorBase (p), bus (p)
    {
        views[0] = std::make_unique<drive::View> (p, p.driveModule, stages[0].prefix);
        views[1] = std::make_unique<shelf::View> (p, p.shelfModule, stages[1].prefix);
        views[2] = std::make_unique<hfcomp::View> (p, p.hfcompModule, stages[2].prefix);
        views[3] = std::make_unique<width::View> (p, p.widthModule, stages[3].prefix);
        views[4] = std::make_unique<iron::View> (p, p.ironModule, stages[4].prefix);

        for (size_t i = 0; i < numStages; ++i)
        {
            addChildComponent (*views[i]);

            tabs[i].setButtonText (stages[i].name.toUpperCase());
            tabs[i].colour = stages[i].colour;
            tabs[i].onClick = [this, i] { select (i); };
            addAndMakeVisible (tabs[i]);

            switches[i] = std::make_unique<PowerButton> (stages[i].colour, true);
            switchAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, stages[i].prefix + "on", *switches[i]);
            addAndMakeVisible (*switches[i]);

            for (auto* raw : p.getParameters())
            {
                auto* param = dynamic_cast<juce::RangedAudioParameter*> (raw);
                if (param == nullptr)
                    continue;
                const auto id = param->getParameterID();
                if (id.startsWith (stages[i].prefix) && id != stages[i].prefix + "on")
                    stageKnobs[i].push_back (&addKnob (*param, stages[i].colour,
                                                       param->getName (64).fromFirstOccurrenceOf (stages[i].name + " ", false, false)));
            }
        }

        for (auto* id : { "input", "output" })
            trimKnobs.push_back (&addKnob (*p.apvts.getParameter (id), p.accent()));

        select ((size_t) juce::jlimit (0, (int) numStages - 1, (int) p.apvts.state.getProperty ("view", 0)));
        start (1280, 660);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < numStages; ++i)
            if (titles[i].contains (e.getPosition()))
                select (i);
    }

protected:
    void layout() override
    {
        constexpr int top = 82, meterWidth = 190, mainHeight = 232, gap = 12, headerHeight = 36;
        leftMeter->setBounds (20, top, meterWidth, mainHeight);
        rightMeter->setBounds (getWidth() - 20 - meterWidth, top, meterWidth, mainHeight);

        auto centre = juce::Rectangle<int> (20 + meterWidth + 14, top, getWidth() - 2 * (20 + meterWidth + 14), mainHeight);
        auto tabRow = centre.removeFromTop (28);
        centre.removeFromTop (6);
        const int tabWidth = (tabRow.getWidth() - (int) (numStages - 1) * 8) / (int) numStages;
        for (size_t i = 0; i < numStages; ++i)
        {
            tabs[i].setBounds (tabRow.removeFromLeft (tabWidth));
            tabRow.removeFromLeft (8);
            views[i]->setBounds (centre);
        }

        // Cards: knobs fill two rows, column by column; the whole row of cards is centred.
        // Columns holding a stepped switch are wider to fit its position labels.
        const auto columnWidths = [] (const std::vector<Knob*>& cardKnobs) {
            std::vector<int> widths ((cardKnobs.size() + 1) / 2, 90);
            for (size_t k = 0; k < cardKnobs.size(); ++k)
                if (cardKnobs[k]->isStepped())
                    widths[k / 2] = 112;
            return widths;
        };
        const auto cardWidth = [&] (const std::vector<Knob*>& cardKnobs) {
            const auto widths = columnWidths (cardKnobs);
            return std::accumulate (widths.begin(), widths.end(), 24);
        };
        int total = cardWidth (trimKnobs);
        for (const auto& k : stageKnobs)
            total += cardWidth (k) + gap;

        const int cardTop = top + mainHeight + 16;
        const int cardHeight = getHeight() - cardTop - 32;
        int x = (getWidth() - total) / 2;

        const auto placeCard = [&] (const std::vector<Knob*>& cardKnobs, juce::Rectangle<int>& card, juce::Rectangle<int>& title) {
            card = { x, cardTop, cardWidth (cardKnobs), cardHeight };
            x += card.getWidth() + gap;
            auto inner = card.reduced (12, 0);
            title = inner.removeFromTop (headerHeight);
            inner.removeFromTop (4);
            const int rowHeight = (inner.getHeight() - 8) / 2;
            const auto widths = columnWidths (cardKnobs);
            for (size_t k = 0; k < cardKnobs.size(); ++k)
            {
                const int columnX = std::accumulate (widths.begin(), widths.begin() + (long) (k / 2), inner.getX());
                cardKnobs[k]->setBounds (columnX, inner.getY() + (int) (k % 2) * rowHeight, widths[k / 2], rowHeight);
            }
        };

        for (size_t i = 0; i < numStages; ++i)
        {
            placeCard (stageKnobs[i], cards[i], titles[i]);
            switches[i]->setBounds (titles[i].removeFromRight (44).withSizeKeepingCentre (44, 22));
        }
        placeCard (trimKnobs, trimCard, trimTitle);
    }

    void paintPanels (juce::Graphics& g) override
    {
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.14f));
        const auto paintCard = [&g] (juce::Rectangle<int> card, juce::Rectangle<int> title, const juce::String& text, juce::Colour colour) {
            draw::card (g, card.toFloat(), 14.0f);
            g.setColour (colour);
            g.drawText (text.toUpperCase(), title.toFloat(), juce::Justification::centredLeft);
            g.setColour (colours::cardEdge);
            g.fillRect ((float) card.getX() + 12.0f, (float) title.getBottom(), (float) card.getWidth() - 24.0f, 1.0f);
        };

        for (size_t i = 0; i < numStages; ++i)
        {
            paintCard (cards[i], titles[i], stages[i].name, stages[i].colour);

            // Chevron between cards: the signal flows left to right.
            const auto right = (float) cards[i].getRight() + 6.0f, mid = (float) cards[i].getCentreY();
            juce::Path chevron;
            chevron.startNewSubPath (right - 2.0f, mid - 4.0f);
            chevron.lineTo (right + 2.0f, mid);
            chevron.lineTo (right - 2.0f, mid + 4.0f);
            g.setColour (colours::engraveDim.withAlpha (0.5f));
            g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        paintCard (trimCard, trimTitle, "Trim", colours::engraveDim);
    }

    void tick() override
    {
        views[selected]->tick();
        views[selected]->repaint();

        // Dim a stage's knobs while it is switched out.
        for (size_t i = 0; i < numStages; ++i)
        {
            const bool on = bus.isStageOn (i);
            if (lastOn[i] == (on ? 1 : 0))
                continue;
            lastOn[i] = on ? 1 : 0;
            for (auto* knob : stageKnobs[i])
                knob->setAlpha (on ? 1.0f : 0.4f);
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
    std::array<std::unique_ptr<PowerButton>, numStages> switches;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, numStages> switchAttachments;
    std::array<std::vector<Knob*>, numStages> stageKnobs;
    std::vector<Knob*> trimKnobs;
    std::array<juce::Rectangle<int>, numStages> cards, titles;
    juce::Rectangle<int> trimCard, trimTitle;
    std::array<int, numStages> lastOn { -1, -1, -1, -1, -1 };
    size_t selected = 0;
};

juce::AudioProcessorEditor* BusProcessor::createEditor() { return new BusEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BusProcessor(); }
