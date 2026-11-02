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
// Rotary: dark dished knob with a glowing value arc and an accent indicator.
// Choice parameters become stepped switches with a lit dot and a label at every position.
class Knob : public juce::Slider
{
public:
    Knob (juce::RangedAudioParameter& p, juce::Colour accentColour, const juce::String& label = {})
        : name ((label.isEmpty() ? p.getName (24) : label).toUpperCase()), accent (accentColour)
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

    bool isStepped() const { return ! stepLabels.isEmpty(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto valueArea = bounds.removeFromBottom (16.0f);
        const auto nameArea = bounds.removeFromBottom (18.0f);

        const bool stepped = ! stepLabels.isEmpty();
        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const auto ringR = size * 0.5f - (stepped ? 18.0f : 8.0f);
        const auto r = ringR - 7.0f;
        const auto params = getRotaryParameters();
        const auto pos = (float) valueToProportionOfLength (getValue());
        const auto angleAt = [&] (float t) { return params.startAngleRadians + t * (params.endAngleRadians - params.startAngleRadians); };
        const bool active = isMouseOverOrDragging();
        const auto stroke = juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, params.startAngleRadians, params.endAngleRadians, true);
        g.setColour (colours::tickOff);
        g.strokePath (track, stroke);

        if (stepped)
        {
            const int steps = stepLabels.size() - 1;
            g.setFont (draw::font (10.0f, true));
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (float) i / (float) steps;
                const bool selected = std::abs (t - pos) < 0.5f / (float) steps;
                const auto dot = detail::polar (centre, ringR, angleAt (t));
                if (selected)
                {
                    draw::led (g, dot, 3.2f, accent, true);
                }
                else
                {
                    g.setColour (colours::engraveDim.withAlpha (0.55f));
                    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (dot));
                }
                g.setColour (selected ? accent.brighter (0.4f) : colours::engraveDim);
                g.drawText (stepLabels[i], juce::Rectangle<float> (34.0f, 12.0f).withCentre (detail::polar (centre, ringR + 13.0f, angleAt (t))),
                            juce::Justification::centred);
            }
        }
        else
        {
            const auto origin = bipolar ? 0.5f : 0.0f;
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, angleAt (juce::jmin (origin, pos)), angleAt (juce::jmax (origin, pos)), true);
            draw::glow (g, value, active ? accent.brighter (0.25f) : accent, 3.5f);

            g.setFont (draw::font (10.0f));
            g.setColour (colours::engraveDim);
            for (int end = 0; end < 2; ++end)
            {
                const auto p = detail::polar (centre, ringR, angleAt ((float) end)).translated (end == 0 ? -4.0f : 4.0f, 13.0f);
                g.drawText (endLabels[end], juce::Rectangle<float> (34.0f, 12.0f).withCentre (p), juce::Justification::centred);
            }
        }

        // Body: soft shadow, convex rim and a slightly dished face.
        const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff40444d), body.getCentreX(), body.getY(),
                                                 juce::Colour (0xff1a1c21), body.getCentreX(), body.getBottom(), false));
        g.fillEllipse (body);
        const auto face = body.reduced (r * 0.14f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23262c), face.getCentreX(), face.getY(),
                                                 juce::Colour (0xff30343b), face.getCentreX(), face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (active ? 0.12f : 0.06f));
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        const auto a = angleAt (pos);
        juce::Path indicator;
        indicator.startNewSubPath (detail::polar (centre, r * 0.3f, a));
        indicator.lineTo (detail::polar (centre, r * 0.78f, a));
        draw::glow (g, indicator, accent.brighter (0.3f), 3.0f);

        g.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.1f));
        g.setColour (colours::engraveDim);
        g.drawText (name, nameArea, juce::Justification::centred);
        g.setFont (draw::font (12.5f));
        g.setColour (active ? accent.brighter (0.5f) : colours::engrave);
        g.drawText (getTextFromValue (getValue()), valueArea, juce::Justification::centred);
    }

private:
    juce::String name;
    juce::Colour accent;
    juce::StringArray stepLabels;
    juce::String endLabels[2];
    bool bipolar = false;
};

//==============================================================================
// Pill switch with a glowing track when engaged. By default it is attached to "bypass",
// so it lights while the toggle is off; pass litWhenOn for ordinary on/off parameters.
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour accentColour, bool litWhenToggleOn = false)
        : accent (accentColour), litWhenOn (litWhenToggleOn)
    {
        setTooltip ("Engage / bypass");
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto track = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), area.getWidth() * 0.55f));
        const auto radius = track.getHeight() * 0.5f;
        const bool lit = litWhenOn == getToggleState();

        if (lit)
        {
            g.setColour (accent.withAlpha (0.2f));
            g.fillRoundedRectangle (track.expanded (2.5f), radius + 2.5f);
        }
        g.setColour (lit ? accent.withAlpha (0.9f) : colours::button);
        g.fillRoundedRectangle (track, radius);
        g.setColour (over ? juce::Colours::white.withAlpha (0.2f) : colours::cardEdge);
        g.drawRoundedRectangle (track, radius, 1.0f);

        const auto thumbSize = track.getHeight() - 5.0f;
        const auto thumbX = lit ? track.getRight() - thumbSize - 2.5f : track.getX() + 2.5f;
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillEllipse (thumbX, track.getY() + 3.5f, thumbSize, thumbSize);
        g.setColour (lit ? juce::Colours::white : juce::Colour (0xff8a909b));
        g.fillEllipse (thumbX, track.getY() + 2.5f, thumbSize, thumbSize);
    }

private:
    juce::Colour accent;
    bool litWhenOn;
};

//==============================================================================
// Analogue-style VU on a dark face, with spring-damped needle ballistics.
// Level mode reads RMS with 0 VU = -18 dBFS; Reduction mode reads gain reduction in dB.
class VuMeter : public juce::Component
{
public:
    enum class Mode { level, reduction };

    VuMeter (juce::String labelText, Mode m, std::function<float()> readSource, juce::Colour accentColour = colours::engrave)
        : label (std::move (labelText)), mode (m), source (std::move (readSource)), accent (accentColour)
    {
        setInterceptsMouseClicks (false, false);
    }

    // Returns true when the needle or peak light moved enough to need a repaint.
    bool tick (float dt)
    {
        const auto previous = position;
        const auto wasPeaking = peakHold > 0.0f;
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
        return std::abs (position - previous) > 0.0005f || wasPeaking != (peakHold > 0.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f, 0.0f).withTrimmedBottom (3.0f);
        draw::card (g, bounds, 12.0f);

        const auto labelArea = bounds.removeFromBottom (22.0f);
        const auto face = bounds.reduced (6.0f);
        juce::Path faceShape;
        faceShape.addRoundedRectangle (face, 8.0f);

        g.setColour (colours::glass);
        g.fillPath (faceShape);
        drawFace (g, face, faceShape);

        g.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.12f));
        g.setColour (colours::engraveDim);
        g.drawText (label, labelArea.translated (0.0f, -2.0f), juce::Justification::centred);
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
        const auto ink = colours::engraveDim;
        const auto red = juce::Colour (0xffff5a4a);

        // Faint backlight rising from the pivot.
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.12f), pivot.x, face.getBottom(),
                                                 accent.withAlpha (0.0f), pivot.x, face.getY(), true));
        g.fillRect (face);

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

        g.setColour (ink.withAlpha (0.6f));
        g.setFont (draw::font (13.0f, true));
        g.drawText (mode == Mode::level ? "VU" : "GR", face.withTrimmedTop (face.getHeight() * 0.5f).withTrimmedBottom (16.0f),
                    juce::Justification::centred);

        if (mode == Mode::level)
            draw::led (g, { face.getRight() - 12.0f, face.getY() + 11.0f }, 3.5f, juce::Colour (0xffff3b2a), peakHold > 0.0f);

        // Glowing needle, then a hairline edge around the face.
        const auto a = angleAt (position);
        juce::Path needle;
        needle.startNewSubPath (detail::polar (pivot, radius * 0.3f, a));
        needle.lineTo (detail::polar (pivot, radius * 0.97f, a));
        draw::glow (g, needle, colours::engrave, 1.5f);
        draw::led (g, { pivot.x, face.getBottom() - 2.0f }, 5.0f, accent, true);

        g.setColour (colours::cardEdge);
        g.strokePath (faceShape, juce::PathStrokeType (1.0f));
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
    juce::Colour accent;
    float position = 0.0f, velocity = 0.0f, peakHold = 0.0f;
};

//==============================================================================
// Dark screen inset in a card. Subclasses draw into the screen and get fresh output
// samples from the processor once per frame.
class Display : public juce::Component
{
public:
    // `paramPrefix` is prepended to parameter ids, so a module's view can sit inside a larger plugin.
    Display (Processor& p, juce::Colour accentColour, juce::String paramPrefix = {})
        : proc (p), accent (accentColour), prefix (std::move (paramPrefix))
    {
        setInterceptsMouseClicks (false, false);
    }

    explicit Display (Processor& p) : Display (p, p.accent()) {}

    void tick()
    {
        const int n = proc.scope.pull (left.data(), right.data(), ScopeFifo::capacity);
        onSamples (left.data(), right.data(), n);
        update();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f, 0.0f).withTrimmedBottom (3.0f);
        draw::card (g, bounds, 12.0f);

        const auto screen = bounds.reduced (6.0f);
        juce::Path screenShape;
        screenShape.addRoundedRectangle (screen, 8.0f);
        g.setColour (colours::glass);
        g.fillPath (screenShape);

        {
            const juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (screenShape);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.13f), screen.getCentreX(), screen.getBottom(),
                                                     accent.withAlpha (0.0f), screen.getCentreX(), screen.getY(), true));
            g.fillRect (screen);
            drawContent (g, screen.reduced (10.0f, 8.0f));
        }

        g.setColour (colours::cardEdge);
        g.strokePath (screenShape, juce::PathStrokeType (1.0f));
    }

protected:
    virtual void onSamples (const float*, const float*, int) {}
    virtual void update() {}
    virtual void drawContent (juce::Graphics&, juce::Rectangle<float> area) = 0;

    float paramValue (const char* id) const { return proc.apvts.getRawParameterValue (prefix + id)->load(); }

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
    juce::String prefix;
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

        // One vertex per pixel column is all the screen can show.
        juce::Path trace;
        const auto columns = juce::jmax (2, (int) area.getWidth());
        for (int c = 0; c < columns; ++c)
        {
            const auto k = c * (window - 1) / (columns - 1);
            const auto v = juce::jlimit (-1.0f, 1.0f, history[(size_t) ((start + k) % size)] * gain);
            const auto x = area.getX() + (float) c;
            const auto y = area.getCentreY() - v * area.getHeight() * 0.46f;
            c == 0 ? trace.startNewSubPath (x, y) : trace.lineTo (x, y);
        }
        draw::glow (g, trace, accent.brighter (0.3f), 1.6f);
    }

    std::array<float, 4096> history {};
    int writePos = 0;
};

} // namespace fenix
