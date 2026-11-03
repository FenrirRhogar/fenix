#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fenix
{

namespace colours
{
    const juce::Colour plateTop { 0xff1e2127 };
    const juce::Colour plateBottom { 0xff131519 };
    const juce::Colour card { 0xff1a1d22 };
    const juce::Colour cardEdge { 0x10ffffff };
    const juce::Colour glass { 0xff0c0e11 };
    const juce::Colour engrave { 0xffe2e5ea };
    const juce::Colour engraveDim { 0xff868c97 };
    const juce::Colour tickOff { 0xff2f333b };
    const juce::Colour lcd { 0xff101216 };
    const juce::Colour button { 0xff252930 };
}

namespace draw
{
    inline juce::Font font (float height, bool bold = false)
    {
        return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
    }

    inline juce::Font mono (float height)
    {
        return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain);
    }

    // Panel label text with a faint drop shadow so it stays crisp on gradients.
    inline void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                          juce::Justification just, juce::Colour colour = colours::engrave)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    // Stroke with a soft bloom, for lit traces on the displays.
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (path, juce::PathStrokeType (width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A lit dot with halo.
    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), centre, colour.withAlpha (0.0f),
                                                     centre.translated (radius * 3.5f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (radius * 7.0f, radius * 7.0f).withCentre (centre));
        }
        const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setColour (on ? colour.brighter (0.4f) : colour.darker (2.5f).withAlpha (0.8f));
        g.fillEllipse (body);
    }

    // Raised card: soft shadow, slightly lighter top edge and a hairline border.
    inline void card (juce::Graphics& g, juce::Rectangle<float> area, float corner = 12.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (area.translated (0.0f, 3.0f).expanded (1.0f), corner + 1.0f);
        g.setGradientFill (juce::ColourGradient (colours::card.brighter (0.07f), area.getTopLeft(),
                                                 colours::card, area.getBottomLeft(), false));
        g.fillRoundedRectangle (area, corner);
        g.setColour (colours::cardEdge);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
    }

    // Fenix mark: Fehu, the F rune. An upright stave with two branches rising to the right,
    // drawn as round-capped strokes inside a square box.
    inline juce::Path fehu (juce::Rectangle<float> box)
    {
        const auto size = juce::jmin (box.getWidth(), box.getHeight());
        const auto at = [&] (float x, float y) { return juce::Point<float> (box.getX() + x * size, box.getY() + y * size); };

        juce::Path lines;
        for (auto [from, to] : { std::pair { at (0.36f, 0.10f), at (0.36f, 0.90f) },
                                 { at (0.36f, 0.40f), at (0.74f, 0.12f) },
                                 { at (0.36f, 0.64f), at (0.74f, 0.36f) } })
        {
            lines.startNewSubPath (from);
            lines.lineTo (to);
        }

        juce::Path rune;
        juce::PathStrokeType (0.13f * size, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
            .createStrokedPath (rune, lines);
        return rune;
    }

    // The mark's fixed brand gradient, amber to red, the same in every plugin.
    inline void fillFehu (juce::Graphics& g, juce::Rectangle<float> box)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc45a), box.getCentreX(), box.getY(),
                                                 juce::Colour (0xffe84830), box.getCentreX(), box.getBottom(), false));
        g.fillPath (fehu (box));
    }
}

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { juce::Colours::orange };

    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1e23));
        setColour (juce::PopupMenu::textColourId, colours::engrave);
        setColour (juce::PopupMenu::headerTextColourId, colours::engraveDim);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1d2025));
        setColour (juce::AlertWindow::textColourId, colours::engrave);
        setColour (juce::TextEditor::backgroundColourId, colours::lcd);
        setColour (juce::TextEditor::textColourId, colours::engrave);
        setColour (juce::TextEditor::outlineColourId, colours::tickOff);
        setColour (juce::TextButton::buttonColourId, colours::button);
        setColour (juce::TextButton::textColourOffId, colours::engrave);
    }

    void setAccent (juce::Colour c)
    {
        accent = c;
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::focusedOutlineColourId, c);
        setColour (juce::TextEditor::highlightColourId, c.withAlpha (0.4f));
    }

    juce::Font getPopupMenuFont() override { return draw::font (14.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (colours::cardEdge);
        g.drawRect (0, 0, width, height);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto area = b.getLocalBounds().toFloat().reduced (0.5f);
        const auto corner = juce::jmin (8.0f, area.getHeight() * 0.5f);
        g.setColour (colours::button.brighter (down ? 0.02f : (over ? 0.12f : 0.0f)));
        g.fillRoundedRectangle (area, corner);
        g.setColour (over ? accent.withAlpha (0.45f) : colours::cardEdge);
        g.drawRoundedRectangle (area, corner, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.08f);
    }
};

} // namespace fenix
