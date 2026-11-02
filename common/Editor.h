#pragma once

#include "Processor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fenix
{

namespace colours
{
    const juce::Colour background { 0xff1b1d22 };
    const juce::Colour panel { 0xff25282f };
    const juce::Colour track { 0xff3a3e48 };
    const juce::Colour text { 0xffe4e6eb };
    const juce::Colour dim { 0xff9aa0ab };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { juce::Colours::orange };

    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::ComboBox::backgroundColourId, colours::panel);
        setColour (juce::ComboBox::outlineColourId, colours::track);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::ToggleButton::textColourId, colours::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto stroke = juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path trackArc, valueArc;
        trackArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (colours::track);
        g.strokePath (trackArc, stroke);
        g.setColour (accent);
        g.strokePath (valueArc, stroke);

        const auto knob = radius - 8.0f;
        g.setColour (colours::panel);
        g.fillEllipse (centre.x - knob, centre.y - knob, knob * 2.0f, knob * 2.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -knob + 3.0f, 3.0f, knob * 0.45f, 1.5f);
        g.setColour (colours::text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
    }
};

// Builds one control per parameter (knob, combo or toggle) plus an optional meter strip.
class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor (Processor& p) : AudioProcessorEditor (p), proc (p), meterSpec (p.meterSpec())
    {
        lnf.accent = p.accent();
        setLookAndFeel (&lnf);

        for (auto* raw : p.getParameters())
        {
            auto* param = dynamic_cast<juce::RangedAudioParameter*> (raw);
            if (param == nullptr)
                continue;

            auto& c = *controls.emplace_back (std::make_unique<Control>());
            const auto id = param->getParameterID();
            c.label.setText (param->getName (32), juce::dontSendNotification);
            c.label.setJustificationType (juce::Justification::centred);
            c.label.setColour (juce::Label::textColourId, colours::dim);
            addAndMakeVisible (c.label);

            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            {
                c.combo.addItemList (choice->choices, 1);
                c.comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id, c.combo);
                addAndMakeVisible (c.combo);
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
            {
                c.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id, c.button);
                addAndMakeVisible (c.button);
            }
            else
            {
                c.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                c.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
                c.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, c.slider);
                addAndMakeVisible (c.slider);
            }
        }

        const int meterHeight = meterSpec.label.isEmpty() ? 0 : 36;
        setSize (juce::jmax (320, (int) controls.size() * 110 + 32), 196 + meterHeight);

        if (meterHeight > 0)
            startTimerHz (30);
    }

    ~Editor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);

        auto header = getLocalBounds().removeFromTop (36).reduced (16, 0);
        g.setColour (lnf.accent);
        g.fillRect (header.getX(), 30, 24, 2);
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (proc.getName().toUpperCase(), header, juce::Justification::centredLeft);

        if (meterSpec.label.isEmpty())
            return;

        const auto value = meterValue;
        auto strip = getLocalBounds().removeFromBottom (36).reduced (16, 10);
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (meterSpec.label, strip.removeFromLeft (90), juce::Justification::centredLeft);
        g.drawText (juce::String (value, 1) + meterSpec.unit, strip.removeFromRight (70), juce::Justification::centredRight);

        auto bar = strip.reduced (8, 2).toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (bar, 3.0f);
        const auto norm = juce::jlimit (0.0f, 1.0f, (value - meterSpec.min) / (meterSpec.max - meterSpec.min));
        g.setColour (lnf.accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * norm), 3.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16, 0);
        area.removeFromTop (44);
        area = area.removeFromTop (140);
        const int w = area.getWidth() / juce::jmax (1, (int) controls.size());

        for (auto& c : controls)
        {
            auto cell = area.removeFromLeft (w).reduced (4, 0);
            c->label.setBounds (cell.removeFromTop (18));
            if (c->comboAttachment)
                c->combo.setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8, 26));
            else if (c->buttonAttachment)
                c->button.setBounds (cell.withSizeKeepingCentre (28, 28));
            else
                c->slider.setBounds (cell);
        }
    }

private:
    void timerCallback() override
    {
        meterValue = proc.meter.load();
        repaint (getLocalBounds().removeFromBottom (36));
    }

    struct Control
    {
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox combo;
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    Processor& proc;
    MeterSpec meterSpec;
    LookAndFeel lnf;
    std::vector<std::unique_ptr<Control>> controls;
    float meterValue = 0.0f;
};

inline juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

} // namespace fenix
