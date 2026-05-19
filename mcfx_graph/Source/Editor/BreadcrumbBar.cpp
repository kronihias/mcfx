#include "BreadcrumbBar.h"
#include "GraphEditorComponent.h"

BreadcrumbBar::BreadcrumbBar (GraphEditorComponent& editor)
    : editor_ (editor)
{
    rebuild();
}

void BreadcrumbBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a22));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void BreadcrumbBar::rebuild()
{
    buttons_.clear();
    separators_.clear();

    auto path = editor_.getBreadcrumbPath();
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        auto* b = buttons_.add (new juce::TextButton (path[i].label));
        b->setColour (juce::TextButton::buttonColourId,
                      i + 1 == path.size() ? juce::Colour (0xff445566)
                                           : juce::Colour (0xff333344));
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b->onClick = [this, i] { editor_.jumpToBreadcrumb (i); };
        b->setEnabled (i + 1 != path.size());
        addAndMakeVisible (b);

        if (i + 1 < path.size())
        {
            auto* sep = separators_.add (new juce::Label ({}, ">"));
            sep->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
            sep->setJustificationType (juce::Justification::centred);
            addAndMakeVisible (sep);
        }
    }

    resized();
}

void BreadcrumbBar::resized()
{
    auto area = getLocalBounds().reduced (4, 4);
    int sepIdx = 0;
    for (int i = 0; i < buttons_.size(); ++i)
    {
        const auto label = buttons_[i]->getButtonText();
        const int  w = juce::jmax (50,
                          juce::GlyphArrangement::getStringWidthInt (
                              juce::Font (juce::FontOptions (14.0f)), label) + 16);
        buttons_[i]->setBounds (area.removeFromLeft (w));

        if (i + 1 < buttons_.size())
        {
            separators_[sepIdx++]->setBounds (area.removeFromLeft (16));
        }
    }
}
