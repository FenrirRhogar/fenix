#pragma once

#include "Look.h"
#include "Processor.h"

namespace fenix
{

namespace detail
{
    // Unit vector for a rotary angle measured clockwise from 12 o'clock.
    inline juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
    {
        return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
    }

    // Short scale label: "30 Hz" -> "30", "6 kHz" -> "6k", 2000 -> "2k".
    inline juce::String shortLabel (const juce::String& choice)
    {
        return choice.replace (" kHz", "k").replace (" Hz", "");
    }

    inline juce::String shortLabel (float value, bool signedScale)
    {
        if (std::abs (value) >= 1000.0f)
            return juce::String (juce::roundToInt (value / 1000.0f)) + "k";
        const auto rounded = juce::roundToInt (value);
        return (signedScale && rounded > 0 ? "+" : "") + juce::String (rounded);
    }
}

//==============================================================================
// Hardware-style rotary: knurled metal skirt, coloured cap, lit tick ring and a value LCD.
// Choice parameters become stepped switches with a label at every position.
class Knob : public juce::Slider
{
public:
    Knob (juce::RangedAudioParameter& p, juce::Colour accentColour)
        : name (p.getName (24).toUpperCase()), accent (accentColour)
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);

        const auto& range = p.getNormalisableRange();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
        {
            for (const auto& c : choice->choices)
                stepLabels.add (detail::shortLabel (c));
        }
        else
        {
            bipolar = range.start < 0.0f && range.end > 0.0f;
            endLabels[0] = detail::shortLabel (range.start, bipolar);
            endLabels[1] = detail::shortLabel (range.end, bipolar);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto lcdArea = bounds.removeFromBottom (18.0f).withSizeKeepingCentre (juce::jmin (bounds.getWidth() - 10.0f, 86.0f), 18.0f);
        const auto nameArea = bounds.removeFromBottom (20.0f);

        const bool stepped = ! stepLabels.isEmpty();
        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const auto r = size * 0.5f - (stepped ? 22.0f : 16.0f);
        const auto params = getRotaryParameters();
        const auto pos = (float) valueToProportionOfLength (getValue());
        const auto angleAt = [&] (float t) { return params.startAngleRadians + t * (params.endAngleRadians - params.startAngleRadians); };
        const bool active = isMouseOverOrDragging();

        // Tick ring: lit from the rest position (or centre, for +/- scales) to the current value.
        const int ticks = stepped ? stepLabels.size() - 1 : 30;
        const auto origin = bipolar ? 0.5f : 0.0f;
        for (int i = 0; i <= ticks; ++i)
        {
            const auto t = (float) i / (float) ticks;
            const bool lit = stepped ? std::abs (t - pos) < 0.5f / (float) ticks
                                     : (t >= juce::jmin (origin, pos) - 1.0e-4f && t <= juce::jmax (origin, pos) + 1.0e-4f);
            const bool major = stepped || i % 5 == 0;
            const auto a = angleAt (t);
            const juce::Line<float> tick (detail::polar (centre, r + 5.0f, a), detail::polar (centre, r + (major ? 11.0f : 8.0f), a));
            if (lit)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.drawLine (tick, 6.0f);
            }
            g.setColour (lit ? accent.brighter (0.25f) : colours::tickOff);
            g.drawLine (tick, major ? 2.2f : 1.6f);
        }

        g.setFont (draw::font (10.0f, true));
        if (stepped)
        {
            for (int i = 0; i <= ticks; ++i)
            {
                const auto t = (float) i / (float) ticks;
                const auto p = detail::polar (centre, r + 19.0f, angleAt (t));
                const bool selected = std::abs (t - pos) < 0.5f / (float) ticks;
                g.setColour (selected ? accent.brighter (0.4f) : colours::engraveDim);
                g.drawText (stepLabels[i], juce::Rectangle<float> (34.0f, 12.0f).withCentre (p), juce::Justification::centred);
            }
        }
        else
        {
            for (int end = 0; end < 2; ++end)
            {
                const auto p = detail::polar (centre, r + 13.0f, angleAt ((float) end)).translated (end == 0 ? -6.0f : 6.0f, 10.0f);
                draw::engraved (g, endLabels[end], juce::Rectangle<float> (34.0f, 12.0f).withCentre (p),
                                juce::Justification::centred, colours::engraveDim);
            }
        }

        // Body: drop shadow, metal skirt with knurling, coloured cap with highlight.
        const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (body.translated (0.0f, 4.0f).expanded (2.0f));
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (body.translated (0.0f, 8.0f).expanded (4.0f));

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5e66), centre.translated (-r * 0.5f, -r * 0.7f),
                                                 juce::Colour (0xff121316), centre.translated (r * 0.4f, r * 0.8f), false));
        g.fillEllipse (body);
        for (int i = 0; i < 64; ++i)
        {
            const auto a = (float) i * juce::MathConstants<float>::twoPi / 64.0f;
            g.setColour (i % 2 == 0 ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::black.withAlpha (0.35f));
            g.drawLine ({ detail::polar (centre, r * 0.83f, a), detail::polar (centre, r * 0.98f, a) }, 1.3f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (body, 1.0f);

        const auto capR = r * 0.74f;
        const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        const auto capColour = active ? accent.brighter (0.15f) : accent;
        g.setGradientFill (juce::ColourGradient (capColour.brighter (0.45f), centre.translated (-capR * 0.45f, -capR * 0.55f),
                                                 capColour.darker (0.9f), centre.translated (capR * 0.7f, capR * 0.9f), true));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (cap, 1.2f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), cap.getCentreX(), cap.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), cap.getCentreX(), cap.getCentreY(), false));
        g.fillEllipse (cap.reduced (capR * 0.18f, capR * 0.08f).withTrimmedBottom (capR * 0.9f));

        // Pointer line, running from the cap's centre region out across the skirt.
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.7f, -r * 0.97f, 3.4f, r * 0.55f, 1.7f);
        const auto rotation = juce::AffineTransform::rotation (angleAt (pos)).translated (centre);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (pointer, rotation.translated (0.0f, 1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillPath (pointer, rotation);

        if (active)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.drawEllipse (body.expanded (2.5f), 1.2f);
        }

        g.setFont (draw::font (11.5f, true));
        draw::engraved (g, name, nameArea, juce::Justification::centred);

        g.setColour (colours::lcd);
        g.fillRoundedRectangle (lcdArea, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (lcdArea, 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) lcdArea.getBottom(), lcdArea.getX() + 2.0f, lcdArea.getRight() - 2.0f);
        g.setFont (draw::mono (12.0f));
        g.setColour (accent.brighter (active ? 0.6f : 0.3f));
        g.drawText (getTextFromValue (getValue()), lcdArea, juce::Justification::centred);
    }

private:
    juce::String name;
    juce::Colour accent;
    juce::StringArray stepLabels;
    juce::String endLabels[2];
    bool bipolar = false;
};

//==============================================================================
// Round metal push button with an LED: lit while the processor is engaged.
// Attached to the "bypass" parameter, so the LED shows the inverse of the toggle state.
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour accentColour) : accent (accentColour)
    {
        setTooltip ("Engage / bypass");
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto b = getLocalBounds().toFloat().reduced (2.0f);
        const auto c = b.getCentre();
        const auto r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (body.translated (0.0f, 2.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff2a2c31 : 0xff50545c), body.getTopLeft(),
                                                 juce::Colour (0xff17181b), body.getBottomRight(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::white.withAlpha (over ? 0.25f : 0.1f));
        g.drawEllipse (body.reduced (1.0f), 1.0f);
        draw::led (g, c, r * 0.34f, accent, ! getToggleState());
    }

private:
    juce::Colour accent;
};

//==============================================================================
// Backlit analogue VU with spring-damped needle ballistics.
// Level mode reads RMS with 0 VU = -18 dBFS; Reduction mode reads gain reduction in dB.
class VuMeter : public juce::Component
{
public:
    enum class Mode { level, reduction };

    VuMeter (juce::String labelText, Mode m, std::function<float()> readSource)
        : label (std::move (labelText)), mode (m), source (std::move (readSource))
    {
        setInterceptsMouseClicks (false, false);
    }

    void tick (float dt)
    {
        float target;
        if (mode == Mode::level)
        {
            const auto vu = juce::Decibels::gainToDecibels (source(), -60.0f) + 18.0f;
            target = levelPosition (vu);
            if (vu > 1.0f)
                peakHold = 0.6f;
        }
        else
        {
            target = 1.0f - juce::jlimit (0.0f, 1.0f, source() / 20.0f);
        }

        // Second-order needle: natural frequency ~13 rad/s, slightly underdamped like a real movement.
        velocity += ((target - position) * 170.0f - velocity * 22.0f) * dt;
        position = juce::jlimit (-0.02f, 1.06f, position + velocity * dt);
        peakHold = juce::jmax (0.0f, peakHold - dt);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101114), bounds.getTopLeft(),
                                                 juce::Colour (0xff3b3e45), bounds.getBottomLeft(), false));
        g.fillRoundedRectangle (bounds, 8.0f);

        const auto labelArea = bounds.removeFromBottom (20.0f);
        const auto face = bounds.reduced (5.0f);
        juce::Path faceShape;
        faceShape.addRoundedRectangle (face, 5.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbefc8), face.getCentreX(), face.getY() + face.getHeight() * 0.35f,
                                                 juce::Colour (0xffc99f55), face.getX(), face.getBottom(), true));
        g.fillPath (faceShape);

        drawFace (g, face, faceShape);

        g.setFont (draw::font (11.0f, true));
        draw::engraved (g, label, labelArea.translated (0.0f, -2.0f), juce::Justification::centred);
    }

private:
    void drawFace (juce::Graphics& g, juce::Rectangle<float> face, const juce::Path& faceShape) const
    {
        const juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (faceShape);

        const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.1f);
        const auto radius = juce::jmin (face.getHeight() * 0.98f, (face.getWidth() * 0.5f - 10.0f) / (0.92f * std::sin (juce::degreesToRadians (42.0f))));
        const auto sweep = juce::degreesToRadians (42.0f);
        const auto angleAt = [sweep] (float t) { return -sweep + 2.0f * sweep * t; };
        const auto ink = juce::Colour (0xff2b2219);
        const auto red = juce::Colour (0xffc23b2a);

        // Scale arc, red above 0 VU (or the whole arc beyond 10 dB of reduction).
        const auto redStart = mode == Mode::level ? levelPosition (0.0f) : 0.0f;
        const auto redEnd = mode == Mode::level ? 1.0f : 0.5f;
        juce::Path arc, redArc;
        arc.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, angleAt (0.0f), angleAt (redStart), true);
        redArc.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, angleAt (redStart), angleAt (redEnd), true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.4f));
        g.setColour (red);
        g.strokePath (redArc, juce::PathStrokeType (4.0f));

        g.setFont (draw::font (10.0f, true));
        const auto drawMark = [&] (float t, const juce::String& text, bool isRed)
        {
            const auto a = angleAt (t);
            g.setColour (isRed ? red : ink);
            g.drawLine ({ detail::polar (pivot, radius * 0.78f, a), detail::polar (pivot, radius * (text.isEmpty() ? 0.82f : 0.85f), a) },
                        text.isEmpty() ? 1.0f : 1.5f);
            if (text.isNotEmpty())
                g.drawText (text, juce::Rectangle<float> (26.0f, 12.0f).withCentre (detail::polar (pivot, radius * 0.92f, a)),
                            juce::Justification::centred);
        };

        if (mode == Mode::level)
        {
            for (auto [vu, text] : { std::pair { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" },
                                     { -3.0f, "3" }, { -2.0f, "" }, { -1.0f, "" }, { 0.0f, "0" }, { 1.0f, "" },
                                     { 2.0f, "" }, { 3.0f, "+3" } })
                drawMark (levelPosition (vu), text, vu >= 0.0f);
        }
        else
        {
            for (int db = 0; db <= 20; db += 2)
                drawMark (1.0f - (float) db / 20.0f, db % 5 == 0 ? juce::String (db) : juce::String(), db >= 10);
        }

        g.setColour (ink.withAlpha (0.75f));
        g.setFont (draw::font (14.0f, true));
        g.drawText (mode == Mode::level ? "VU" : "GR", face.withTrimmedTop (face.getHeight() * 0.5f).withTrimmedBottom (16.0f),
                    juce::Justification::centred);

        if (mode == Mode::level)
            draw::led (g, { face.getRight() - 12.0f, face.getY() + 11.0f }, 3.5f, juce::Colour (0xffff3b2a), peakHold > 0.0f);

        // Needle with a soft shadow, then the pivot housing across the bottom.
        const auto a = angleAt (position);
        const juce::Line<float> needle (detail::polar (pivot, radius * 0.3f, a), detail::polar (pivot, radius * 0.97f, a));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ needle.getStart().translated (3.0f, 3.0f), needle.getEnd().translated (3.0f, 3.0f) }, 1.8f);
        g.setColour (juce::Colour (0xff1a1410));
        g.drawLine (needle, 1.6f);
        g.setColour (red);
        g.drawLine ({ needle.getPointAlongLineProportionally (0.82f), needle.getEnd() }, 1.6f);

        const auto housing = face.withTop (face.getBottom() - 14.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b40), housing.getTopLeft(),
                                                 juce::Colour (0xff151618), housing.getBottomLeft(), false));
        g.fillRect (housing);

        // Glass: glare over the top half and a dark inner edge.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), face.getTopLeft(),
                                                 juce::Colours::white.withAlpha (0.0f),
                                                 juce::Point<float> (face.getX() + face.getWidth() * 0.4f, face.getCentreY()), false));
        g.fillRect (face);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (faceShape, juce::PathStrokeType (2.5f));
    }

    // Real VU scales are linear in voltage, so 0 VU sits about two thirds of the way across.
    static float levelPosition (float vu)
    {
        const auto lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        return (std::pow (10.0f, vu / 20.0f) - lo) / (hi - lo);
    }

    juce::String label;
    Mode mode;
    std::function<float()> source;
    float position = 0.0f, velocity = 0.0f, peakHold = 0.0f;
};

//==============================================================================
// Glass screen in a bezel. Subclasses draw into the screen and get fresh output
// samples from the processor once per frame.
class Display : public juce::Component
{
public:
    explicit Display (Processor& p) : proc (p), accent (p.accent())
    {
        setInterceptsMouseClicks (false, false);
    }

    void tick()
    {
        const int n = proc.scope.pull (left.data(), right.data(), ScopeFifo::capacity);
        onSamples (left.data(), right.data(), n);
        update();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0f12), bounds.getTopLeft(),
                                                 juce::Colour (0xff3b3e45), bounds.getBottomLeft(), false));
        g.fillRoundedRectangle (bounds, 9.0f);

        const auto screen = bounds.reduced (5.0f);
        juce::Path screenShape;
        screenShape.addRoundedRectangle (screen, 6.0f);
        g.setColour (colours::glass);
        g.fillPath (screenShape);

        {
            const juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (screenShape);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.13f), screen.getCentreX(), screen.getBottom(),
                                                     accent.withAlpha (0.0f), screen.getCentreX(), screen.getY(), true));
            g.fillRect (screen);
            drawContent (g, screen.reduced (10.0f, 8.0f));

            g.setColour (juce::Colours::black.withAlpha (0.16f));
            for (auto y = screen.getY(); y < screen.getBottom(); y += 3.0f)
                g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());
        }

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), screen.getTopLeft(),
                                                 juce::Colours::white.withAlpha (0.0f),
                                                 juce::Point<float> (screen.getX(), screen.getCentreY()), false));
        g.fillRoundedRectangle (screen.withHeight (screen.getHeight() * 0.5f), 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.strokePath (screenShape, juce::PathStrokeType (2.0f));
    }

protected:
    virtual void onSamples (const float*, const float*, int) {}
    virtual void update() {}
    virtual void drawContent (juce::Graphics&, juce::Rectangle<float> area) = 0;

    float paramValue (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void drawGridLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                        juce::Justification just = juce::Justification::centredLeft) const
    {
        g.setColour (accent.withAlpha (0.45f));
        g.setFont (draw::mono (10.0f));
        g.drawText (text, area, just, false);
    }

    juce::Colour gridColour() const { return accent.withAlpha (0.1f); }

    Processor& proc;
    juce::Colour accent;

private:
    std::array<float, ScopeFifo::capacity> left {}, right {};
};

//==============================================================================
// Default screen: a triggered oscilloscope of the output.
class ScopeDisplay : public Display
{
public:
    using Display::Display;

protected:
    void onSamples (const float* l, const float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            history[(size_t) writePos] = 0.5f * (l[i] + r[i]);
            writePos = (writePos + 1) % (int) history.size();
        }
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        g.setColour (gridColour());
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        drawTrace (g, area, 1.0f);
    }

    // Draws `window` samples starting at the most recent rising zero crossing, so the trace stands still.
    void drawTrace (juce::Graphics& g, juce::Rectangle<float> area, float gain, int window = 1024) const
    {
        const auto size = (int) history.size();
        int start = (writePos - window - 1 + size) % size;
        for (int back = 0; back < 2048; ++back)
        {
            const int i = (writePos - window - back - 1 + 2 * size) % size;
            if (history[(size_t) i] <= 0.0f && history[(size_t) ((i + 1) % size)] > 0.0f)
            {
                start = i;
                break;
            }
        }

        juce::Path trace;
        for (int k = 0; k < window; ++k)
        {
            const auto v = juce::jlimit (-1.0f, 1.0f, history[(size_t) ((start + k) % size)] * gain);
            const auto x = area.getX() + area.getWidth() * (float) k / (float) (window - 1);
            const auto y = area.getCentreY() - v * area.getHeight() * 0.46f;
            k == 0 ? trace.startNewSubPath (x, y) : trace.lineTo (x, y);
        }
        draw::glow (g, trace, accent.brighter (0.3f), 1.6f);
    }

    std::array<float, 4096> history {};
    int writePos = 0;
};

} // namespace fenix
