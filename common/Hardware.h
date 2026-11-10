#pragma once

// Parts for editors drawn as a single piece of studio hardware: a painted faceplate, plain black
// knobs over scales printed in real units, rectangular panel LEDs, an LED ladder meter and
// latching push buttons. No glows beyond what a real LED throws, one ink colour, no value
// readouts on the panel (the editor's toolbar shows them).

#include "FenixFonts.h"
#include "Look.h"

namespace fenix::hw
{

namespace ink
{
    const juce::Colour legend { 0xffe8e5dc };       // off-white silkscreen
    const juce::Colour legendDim { 0xffa9a69d };
    const juce::Colour paint { 0xff1c1d20 };        // matte black powder coat
}

// Noto Sans, embedded (see common/fonts/README.md).
inline juce::Font font (float height, bool bold = true, float tracking = 0.1f)
{
    static auto boldFace = juce::Typeface::createSystemTypefaceFor (FenixFonts::NotoSansBold_ttf, FenixFonts::NotoSansBold_ttfSize);
    static auto mediumFace = juce::Typeface::createSystemTypefaceFor (FenixFonts::NotoSansMedium_ttf, FenixFonts::NotoSansMedium_ttfSize);
    return juce::Font (juce::FontOptions (bold ? boldFace : mediumFace).withHeight (height).withKerningFactor (tracking));
}

// Printed legend. Silkscreen ink sits flat on the paint: no shadow, no glow.
inline void print (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                   juce::Justification just = juce::Justification::centred, bool bold = true,
                   juce::Colour colour = ink::legend, float tracking = 0.1f)
{
    g.setFont (font (height, bold, tracking));
    g.setColour (colour);
    g.drawText (text, area, just, false);
}

// Matte powder-coated plate: flat colour, a faint falloff from the top light and a fine speckle.
inline void powderCoat (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour base = ink::paint)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.08f), 0.0f, area.getY(), base.darker (0.12f), 0.0f, area.getBottom(), false));
    g.fillRect (area);
    juce::Random random (11);
    const auto dots = (int) (area.getWidth() * area.getHeight() / 5.0f);
    for (int i = 0; i < dots; ++i)
    {
        g.setColour ((random.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + random.nextFloat() * 0.035f));
        g.fillRect (area.getX() + random.nextFloat() * area.getWidth(), area.getY() + random.nextFloat() * area.getHeight(), 1.0f, 1.0f);
    }
}

// Rectangular panel LED in a black bezel. `level` 0..1 lets it fade like a real diode.
inline void rectLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float level)
{
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.expanded (1.5f), 1.5f);

    const auto off = colour.withMultipliedSaturation (0.7f).withMultipliedBrightness (0.22f);
    const auto lit = colour.interpolatedWith (juce::Colours::white, 0.15f);
    const auto body = off.interpolatedWith (lit, level);
    if (level > 0.02f)
    {
        g.setColour (colour.withAlpha (0.22f * level));
        g.fillRoundedRectangle (r.expanded (3.0f), 3.0f);
    }
    g.setGradientFill (juce::ColourGradient (body.brighter (0.15f), 0.0f, r.getY(), body.darker (0.25f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 1.0f);
    if (level > 0.02f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f * level));
        g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.25f, r.getHeight() * 0.3f), 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (r.getX() + 1.0f, r.getY() + 0.5f, r.getRight() - 1.0f, r.getY() + 0.5f, 0.8f);
}

// Round 3 mm panel LED.
inline void roundLed (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float level)
{
    const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black);
    g.fillEllipse (lens.expanded (1.5f));
    if (level > 0.02f)
    {
        g.setColour (colour.withAlpha (0.22f * level));
        g.fillEllipse (lens.expanded (radius * 0.9f));
    }
    const auto body = colour.withMultipliedSaturation (0.7f).withMultipliedBrightness (0.22f)
                          .interpolatedWith (colour.interpolatedWith (juce::Colours::white, 0.15f), level);
    g.setGradientFill (juce::ColourGradient (body.brighter (0.2f), c.x - radius * 0.4f, c.y - radius * 0.4f,
                                             body.darker (0.3f), c.x + radius, c.y + radius, true));
    g.fillEllipse (lens);
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.3f * level));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.7f, radius * 0.45f).withCentre (c.translated (-radius * 0.25f, -radius * 0.4f)));
}

//==============================================================================
// A printed scale mark: a value in the parameter's own units and its legend (empty = minor tick).
struct Mark
{
    float value;
    juce::String text;
};

// Plain black knob with a white line, over a scale printed in real units. No name, no readout:
// the name is printed on the panel and the value shows in the editor's toolbar.
class Knob : public juce::Slider
{
public:
    static constexpr float sweep = juce::MathConstants<float>::pi * 0.75f;   // ±135°

    Knob (juce::RangedAudioParameter& p, std::vector<Mark> scaleMarks, float knobRadius)
        : param (p), marks (std::move (scaleMarks)), radius (knobRadius)
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::twoPi - sweep, juce::MathConstants<float>::twoPi + sweep, true);
        setMouseDragSensitivity (220);
        setPopupMenuEnabled (false);
        setName (p.getName (32).toUpperCase());
    }

    float knobRadius() const { return radius; }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const auto angleAt = [] (float t) { return -sweep + 2.0f * sweep * t; };

        // Scale: ticks just outside the knob, legends beyond them.
        for (const auto& m : marks)
        {
            const auto a = angleAt (param.convertTo0to1 (m.value));
            const bool major = m.text.isNotEmpty();
            g.setColour (major ? ink::legend : ink::legendDim);
            g.drawLine ({ polar (c, radius + 4.0f, a), polar (c, radius + (major ? 10.0f : 7.5f), a) }, major ? 1.3f : 1.0f);
            if (major)
                print (g, m.text, juce::Rectangle<float> (36.0f, 12.0f).withCentre (polar (c, radius + 19.0f, a)),
                       radius > 40.0f ? 10.0f : 9.0f, juce::Justification::centred, false, ink::legend, 0.0f);
        }

        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (body.isNull() || ! juce::exactlyEqual (bodyScale, scale))
        {
            bodyScale = scale;
            body = renderBody (radius, scale);
        }
        const auto side = (float) body.getWidth() / scale;
        g.drawImage (body, juce::Rectangle<float> (side, side).withCentre (c));

        // The painted line on the cap, running out over the rim.
        const auto a = angleAt ((float) valueToProportionOfLength (getValue()));
        g.setColour (ink::legend);
        g.drawLine ({ polar (c, radius * 0.22f, a), polar (c, radius * 0.97f, a) }, juce::jmax (1.8f, radius * 0.065f));
    }

private:
    static juce::Point<float> polar (juce::Point<float> c, float r, float a) { return c.translated (r * std::sin (a), -r * std::cos (a)); }

    // Moulded black knob seen from straight on, lit from the upper left: contact shadow, a darker
    // side wall with fine grip ridges, and a slightly domed top with a soft specular patch.
    static juce::Image renderBody (float r, float scale)
    {
        const auto side = r * 2.8f;
        juce::Image image (juce::Image::ARGB, juce::roundToInt (side * scale), juce::roundToInt (side * scale), true);
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Point<float> c (side * 0.5f, side * 0.5f);
        const auto circle = [&c] (float rr) { return juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };

        for (int i = 6; i >= 0; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.08f));
            g.fillEllipse (circle (r + (float) i * r * 0.03f).translated (r * 0.04f, r * 0.1f));
        }

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2d31), c.x - r * 0.5f, c.y - r,
                                                 juce::Colour (0xff060607), c.x + r * 0.4f, c.y + r, false));
        g.fillEllipse (circle (r));
        const int ridges = juce::roundToInt (r * 1.6f);
        for (int i = 0; i < ridges; ++i)
        {
            const auto a = (float) i / (float) ridges * juce::MathConstants<float>::twoPi;
            const auto light = 0.5f + 0.5f * std::cos (a + juce::MathConstants<float>::pi * 0.25f);
            g.setColour (juce::Colours::white.withAlpha (0.02f + 0.06f * light));
            g.drawLine ({ polar (c, r * 0.87f, a), polar (c, r - 0.5f, a) }, 0.7f);
        }

        const auto top = r * 0.85f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (circle (top + 0.5f), 1.2f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27282b), c.x - top * 0.6f, c.y - top * 0.7f,
                                                 juce::Colour (0xff111214), c.x + top * 0.6f, c.y + top * 0.8f, false));
        g.fillEllipse (circle (top));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), c.x - top * 0.35f, c.y - top * 0.45f,
                                                 juce::Colours::white.withAlpha (0.0f), c.x - top * 0.35f + top * 0.75f, c.y - top * 0.45f, true));
        g.fillEllipse (circle (top));

        juce::Path rim;
        rim.addCentredArc (c.x, c.y, top, top, 0.0f, -2.2f, 0.4f, true);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (rim, juce::PathStrokeType (0.9f));
        return image;
    }

    juce::RangedAudioParameter& param;
    std::vector<Mark> marks;
    float radius;
    juce::Image body;
    float bodyScale = 0.0f;
};

//==============================================================================
// Square black push button, as used on most rack gear. Drawn in its own bounds;
// the LED that reports its state is drawn by whoever owns it.
inline void pushCap (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
{
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (r.expanded (1.5f), 2.5f);
    if (down)
        r.translate (0.0f, 0.8f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff36373b : 0xff2d2e32), 0.0f, r.getY(),
                                             juce::Colour (0xff131416), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.05f : 0.13f));
    g.drawLine (r.getX() + 2.0f, r.getY() + 0.6f, r.getRight() - 2.0f, r.getY() + 0.6f, 1.0f);
}

// A latching push button with a round LED above it.
class LedButton : public juce::ToggleButton
{
public:
    LedButton (juce::Colour ledColour, bool litWhenToggleOn) : colour (ledColour), litWhenOn (litWhenToggleOn) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool lit = getToggleState() == litWhenOn;
        roundLed (g, { b.getCentreX(), b.getY() + 6.0f }, 3.5f, colour, lit ? 1.0f : 0.0f);
        pushCap (g, b.withTrimmedTop (18.0f).reduced (2.0f, 1.5f), over, down);
    }

private:
    juce::Colour colour;
    bool litWhenOn;
};

//==============================================================================
// Ten rectangular LEDs in a column: green, then yellow, then red. Thresholds are given bottom to top;
// an LED lights once the reading reaches its threshold, and fades out over ~60 ms like a real diode.
class LedLadder : public juce::Component
{
public:
    static constexpr int count = 10;

    LedLadder() { setInterceptsMouseClicks (false, false); }

    void setThresholds (const std::array<float, count>& t) { thresholds = t; }

    // Returns true when something changed enough to need a repaint.
    bool tick (float reading, float dt)
    {
        bool changed = false;
        for (int i = 0; i < count; ++i)
        {
            const float target = reading >= thresholds[(size_t) i] ? 1.0f : 0.0f;
            auto& l = levels[(size_t) i];
            const auto before = l;
            l = target > l ? target : juce::jmax (0.0f, l - dt / 0.06f);
            changed |= std::abs (l - before) > 0.01f;
        }
        return changed;
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < count; ++i)
            rectLed (g, ledBounds (i), colourFor (i), levels[(size_t) i]);
    }

    // LED i (0 = bottom) in local coordinates, for printing the scale beside it.
    juce::Rectangle<float> ledBounds (int i) const
    {
        const auto pitch = (float) getHeight() / (float) count;
        return { 3.0f, (float) getHeight() - pitch * (float) (i + 1) + (pitch - 6.0f) * 0.5f, (float) getWidth() - 6.0f, 6.0f };
    }

    static juce::Colour colourFor (int i)
    {
        return i < 6 ? juce::Colour (0xff3ee05a) : i < 8 ? juce::Colour (0xffffc928) : juce::Colour (0xffff3b2e);
    }

private:
    std::array<float, count> thresholds {};
    std::array<float, count> levels {};
};

} // namespace fenix::hw
