/*
  ==============================================================================

    ThemeManager.h
    Created: 30 Jan 2026
    Description: Centralized theme management for the AXIONiK product triad.
                 Defines colors for AXIONiK, NEURONiK, and NEUROTiK.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace NEURONiK::UI {

/**
 * Defines the color palette for a product theme.
 */
struct ProductTheme
{
    juce::Colour background;
    juce::Colour surface;
    juce::Colour accent;
    juce::Colour text;
    juce::Colour lcdBackground;
    juce::Colour lcdText;
    juce::Colour knobPointer;
    juce::Colour modulationRing;
};

/**
 * Centralized provider for product-specific aesthetics.
 */
class ThemeManager
{
public:
    enum class ProductType
    {
        AXIONiK,    // Flagship (Both engines) - Cyan
        NEURONiK,   // Additive focus - Red
        NEUROTiK    // Resonator focus - Amber
    };

    /** Returns the current product theme. */
    static const ::NEURONiK::UI::ProductTheme& getCurrentTheme() noexcept;

    /** Sets the current product type. */
    static void setProduct(ProductType product) noexcept;

private:
    static ProductType currentProduct;
};

} // namespace NEURONiK::UI
