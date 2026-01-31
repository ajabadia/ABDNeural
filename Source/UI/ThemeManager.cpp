#include "ThemeManager.h"

namespace NEURONiK::UI {

ThemeManager::ProductType ThemeManager::currentProduct = ThemeManager::ProductType::AXIONiK;

void ThemeManager::setProduct(ProductType type) noexcept 
{ 
    currentProduct = type; 
}

const ::NEURONiK::UI::ProductTheme& ThemeManager::getCurrentTheme() noexcept
{
    static const ProductTheme axionikTheme = {
        juce::Colours::black,
        juce::Colour(0xFF1A1A1A),
        juce::Colours::cyan,
        juce::Colours::white,
        juce::Colour(0xFF001A1A),
        juce::Colours::cyan,
        juce::Colours::cyan,
        juce::Colours::cyan
    };

    static const ProductTheme neuronikTheme = {
        juce::Colour(0xFF0D0214),
        juce::Colour(0xFF1A0529),
        juce::Colour(0xFFA020F0),
        juce::Colour(0xFFE0B0FF),
        juce::Colour(0xFF150020),
        juce::Colour(0xFFD080FF),
        juce::Colour(0xFFA020F0),
        juce::Colour(0xFFA020F0)
    };

    static const ProductTheme neurotikTheme = {
        juce::Colour(0xFF140D02),
        juce::Colour(0xFF291A05),
        juce::Colour(0xFFFF8C00),
        juce::Colour(0xFFFFE4B5),
        juce::Colour(0xFF201000),
        juce::Colour(0xFFFFA500),
        juce::Colour(0xFFFF8C00),
        juce::Colour(0xFFFF8C00)
    };

    switch (currentProduct)
    {
        case ProductType::NEURONiK: return neuronikTheme;
        case ProductType::NEUROTiK: return neurotikTheme;
        default:                   return axionikTheme;
    }
}

} // namespace NEURONiK::UI
