#pragma once
#include <JuceHeader.h>

// SANDWAVE visual identity: airy white / ice blue / violet + neon accents.
namespace sw
{
namespace col
{
    inline const juce::Colour snow     (0xffF8FAFF);
    inline const juce::Colour mist     (0xffE6EEFF);
    inline const juce::Colour ice      (0xffC4DBFF);
    inline const juce::Colour sky      (0xff59B2FF);
    inline const juce::Colour neonCyan (0xff3DF2FF);
    inline const juce::Colour violet   (0xff8D6BFF);
    inline const juce::Colour neonPink (0xffFF5FE5);
    inline const juce::Colour ink      (0xff26346E);
    inline const juce::Colour inkSoft  (0xff6272B5);
    inline const juce::Colour night    (0xff070B24);
    inline const juce::Colour indigo   (0xff1B2370);
}

inline juce::Font font (float height, bool bold = true, float kerning = 0.f)
{
    juce::Font f (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    return kerning != 0.f ? f.withExtraKerningFactor (kerning) : f;
}

// Fine "sand grain" speckle clipped to a path - gives surfaces a matte, sandy texture.
inline void sprinkle (juce::Graphics& g, const juce::Path& area, int count, int seed,
                      juce::Colour a, juce::Colour b)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (area, juce::AffineTransform());
    const auto r = area.getBounds();
    juce::Random rng (seed);
    for (int i = 0; i < count; ++i)
    {
        const float x = r.getX() + rng.nextFloat() * r.getWidth();
        const float y = r.getY() + rng.nextFloat() * r.getHeight();
        g.setColour (rng.nextBool() ? a : b);
        g.fillRect (x, y, 1.3f, 1.3f);
    }
}
} // namespace sw
