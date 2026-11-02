#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace fenix
{

namespace colours
{
    const juce::Colour plateTop { 0xff32353c };
    const juce::Colour plateBottom { 0xff1d1f23 };
    const juce::Colour recess { 0xff15161a };
    const juce::Colour glass { 0xff0b0d10 };
    const juce::Colour engrave { 0xffc9ccd3 };
    const juce::Colour engraveDim { 0xff8d929c };
    const juce::Colour tickOff { 0xff474b54 };
    const juce::Colour lcd { 0xff0c0d0f };
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

    // Text stamped into metal: a dark shadow one pixel below the light letters.
    inline void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                          juce::Justification just, juce::Colour colour = colours::engrave)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    // Stroke with a soft bloom, for lit traces on the glass displays.
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colour.withAlpha (0.07f * (float) (4 - i)));
            g.strokePath (path, juce::PathStrokeType (width * (1.0f + 1.8f * (float) i), juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A lit LED dot with halo.
    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), centre, colour.withAlpha (0.0f),
                                                     centre.translated (radius * 3.5f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (radius * 7.0f, radius * 7.0f).withCentre (centre));
        }
        const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (on ? colour.brighter (0.8f) : colour.darker (2.5f), body.getTopLeft(),
                                                 on ? colour : colour.darker (3.5f), body.getBottomRight(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (body, 1.0f);
    }

    inline void screw (juce::Graphics& g, juce::Point<float> centre, float angle)
    {
        const auto r = 6.0f;
        const auto head = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (head.translated (0.0f, 1.0f).expanded (1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9ea6), head.getTopLeft(),
                                                 juce::Colour (0xff3b3e44), head.getBottomRight(), false));
        g.fillEllipse (head);

        juce::Path slot;
        slot.addRoundedRectangle (-r * 0.75f, -0.9f, r * 1.5f, 1.8f, 0.9f);
        g.setColour (juce::Colour (0xff1c1d20));
        g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (centre));
    }

    // Recessed panel: darker fill, shadowed top edge, highlighted bottom edge.
    inline void recess (juce::Graphics& g, juce::Rectangle<float> area, float corner)
    {
        g.setColour (colours::recess.withAlpha (0.55f));
        g.fillRoundedRectangle (area, corner);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (area.translated (0.0f, -0.5f), corner, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (area.translated (0.0f, 1.0f), corner, 1.0f);
    }

    // Fine horizontal grain, generated once per size and reused.
    inline juce::Image brushedTexture (int width, int height)
    {
        juce::Image image (juce::Image::ARGB, width, height, true);
        juce::Graphics g (image);
        juce::Random random (0xf3e41);
        for (int y = 0; y < height; ++y)
        {
            int x = 0;
            while (x < width)
            {
                const int len = 20 + random.nextInt (220);
                const bool light = random.nextBool();
                g.setColour ((light ? juce::Colours::white : juce::Colours::black)
                                 .withAlpha (random.nextFloat() * (light ? 0.035f : 0.06f)));
                g.drawHorizontalLine (y, (float) x, (float) juce::jmin (width, x + len));
                x += len;
            }
        }
        return image;
    }

    // Fenix mark: a stylised flame rising from a split base.
    inline juce::Path flame (juce::Rectangle<float> b)
    {
        juce::Path p;
        const auto x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
        p.startNewSubPath (x + w * 0.50f, y);
        p.cubicTo (x + w * 0.62f, y + h * 0.28f, x + w * 1.02f, y + h * 0.42f, x + w * 0.92f, y + h * 0.74f);
        p.cubicTo (x + w * 0.86f, y + h * 0.92f, x + w * 0.70f, y + h, x + w * 0.50f, y + h);
        p.cubicTo (x + w * 0.30f, y + h, x + w * 0.14f, y + h * 0.92f, x + w * 0.08f, y + h * 0.74f);
        p.cubicTo (x + w * 0.00f, y + h * 0.48f, x + w * 0.22f, y + h * 0.40f, x + w * 0.30f, y + h * 0.18f);
        p.cubicTo (x + w * 0.36f, y + h * 0.38f, x + w * 0.42f, y + h * 0.46f, x + w * 0.50f, y + h * 0.50f);
        p.cubicTo (x + w * 0.52f, y + h * 0.30f, x + w * 0.46f, y + h * 0.14f, x + w * 0.50f, y);
        p.closeSubPath();
        return p;
    }
}

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { juce::Colours::orange };

    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e2025));
        setColour (juce::PopupMenu::textColourId, colours::engrave);
        setColour (juce::PopupMenu::headerTextColourId, colours::engraveDim);
        setColour (juce::ComboBox::textColourId, colours::engrave);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff23252a));
        setColour (juce::AlertWindow::textColourId, colours::engrave);
        setColour (juce::TextEditor::backgroundColourId, colours::lcd);
        setColour (juce::TextEditor::textColourId, colours::engrave);
        setColour (juce::TextEditor::outlineColourId, colours::tickOff);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2f35));
        setColour (juce::TextButton::textColourOffId, colours::engrave);
    }

    void setAccent (juce::Colour c)
    {
        accent = c;
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::focusedOutlineColourId, c);
        setColour (juce::TextEditor::highlightColourId, c.withAlpha (0.4f));
    }

    // Preset selector: a small backlit LCD window.
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto area = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
        g.setColour (colours::lcd);
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (box.isMouseOver (true) ? 0.12f : 0.05f));
        g.drawRoundedRectangle (area.translated (0.0f, 1.0f), 4.0f, 1.0f);

        juce::Path arrow;
        const auto cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour (accent.withAlpha (0.9f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return draw::mono (13.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, accent.brighter (0.3f));
    }

    juce::Font getPopupMenuFont() override { return draw::font (14.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto area = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff26282d : 0xff3a3d44), area.getTopLeft(),
                                                 juce::Colour (0xff222428), area.getBottomLeft(), false));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (over ? 0.16f : 0.08f));
        g.drawHorizontalLine (1, area.getX() + 3.0f, area.getRight() - 3.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return draw::font (12.0f, true); }
};

} // namespace fenix
