#include "PluginSearchPopup.h"

PluginSearchPopup::PluginSearchPopup (juce::Array<juce::PluginDescription> allTypes,
                                       std::function<void (int)> onPicked)
    : all_ (std::move (allTypes)), onPicked_ (std::move (onPicked))
{
    search_.setTextToShowWhenEmpty ("Type to search...", juce::Colours::grey);
    search_.addListener (this);
    search_.setWantsKeyboardFocus (true);
    addAndMakeVisible (search_);

    // Collect the unique format names that appear in the list.
    juce::StringArray formats;
    for (const auto& d : all_)
        if (d.pluginFormatName.isNotEmpty() && ! formats.contains (d.pluginFormatName))
            formats.add (d.pluginFormatName);
    formats.sort (true);

    for (const auto& fmt : formats)
    {
        auto* b = new juce::TextButton (fmt);
        b->setClickingTogglesState (true);
        b->setToggleState (true, juce::dontSendNotification);
        b->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3a3a3a));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3e6aa8));
        b->onClick = [this] { rebuildFiltered(); };
        addAndMakeVisible (b);
        formatButtons_.add (b);
    }

    list_.setModel (this);
    list_.setRowHeight (20);
    list_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1e1e1e));
    addAndMakeVisible (list_);

    rebuildFiltered();
    setSize (440, 480);
}

void PluginSearchPopup::resized()
{
    auto r = getLocalBounds().reduced (4);
    search_.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);

    if (! formatButtons_.isEmpty())
    {
        auto row = r.removeFromTop (22);
        const int n   = formatButtons_.size();
        const int gap = 4;
        const int bw  = (row.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
        for (int i = 0; i < n; ++i)
        {
            formatButtons_[i]->setBounds (row.removeFromLeft (bw));
            if (i < n - 1) row.removeFromLeft (gap);
        }
        r.removeFromTop (4);
    }

    list_.setBounds (r);
}

void PluginSearchPopup::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a2a));
}

void PluginSearchPopup::visibilityChanged()
{
    if (isShowing())
        search_.grabKeyboardFocus();
}

bool PluginSearchPopup::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::upKey
        || key == juce::KeyPress::pageDownKey || key == juce::KeyPress::pageUpKey)
    {
        list_.keyPressed (key);
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        pickRow (list_.getSelectedRow());
        return true;
    }
    return false;
}

//==============================================================================
// TextEditor::Listener

void PluginSearchPopup::textEditorTextChanged (juce::TextEditor&) { rebuildFiltered(); }

void PluginSearchPopup::textEditorReturnKeyPressed (juce::TextEditor&)
{
    pickRow (list_.getSelectedRow() >= 0 ? list_.getSelectedRow() : 0);
}

void PluginSearchPopup::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

//==============================================================================
// ListBoxModel

int PluginSearchPopup::getNumRows() { return filtered_.size(); }

void PluginSearchPopup::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= filtered_.size()) return;
    if (selected)
        g.fillAll (juce::Colour (0xff3e6aa8));

    const auto& d = all_.getReference (filtered_[row]);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));

    // Right-aligned format tag
    auto fmt = d.pluginFormatName;
    int fmtW = 0;
    if (fmt.isNotEmpty())
    {
        fmtW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), fmt) + 12;
        g.setColour (juce::Colour (0xffa0a0a0));
        g.drawText (fmt, w - fmtW, 0, fmtW - 6, h, juce::Justification::centredRight, true);
    }

    g.setColour (juce::Colours::white);
    auto text = d.name;
    if (d.manufacturerName.isNotEmpty())
        text += "  (" + d.manufacturerName + ")";
    g.drawText (text, 6, 0, w - 12 - fmtW, h, juce::Justification::centredLeft, true);
}

void PluginSearchPopup::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    pickRow (row);
}

void PluginSearchPopup::returnKeyPressed (int row) { pickRow (row); }

//==============================================================================

void PluginSearchPopup::pickRow (int row)
{
    if (row < 0 || row >= filtered_.size()) return;
    const int idx = filtered_[row];
    auto cb = onPicked_;
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
    if (cb) cb (idx);
}

void PluginSearchPopup::rebuildFiltered()
{
    filtered_.clearQuick();

    auto q = search_.getText().trim();
    juce::StringArray tokens;
    tokens.addTokens (q, " ", "");
    tokens.removeEmptyStrings();

    juce::StringArray enabledFormats;
    for (auto* b : formatButtons_)
        if (b->getToggleState())
            enabledFormats.add (b->getButtonText());
    const bool anyEnabled = ! enabledFormats.isEmpty();

    for (int i = 0; i < all_.size(); ++i)
    {
        const auto& d = all_.getReference (i);
        if (anyEnabled && ! enabledFormats.contains (d.pluginFormatName))
            continue;

        const auto hay = (d.name + " " + d.manufacturerName + " " + d.pluginFormatName).toLowerCase();
        bool ok = true;
        for (auto& t : tokens)
            if (! hay.contains (t.toLowerCase())) { ok = false; break; }
        if (ok) filtered_.add (i);
    }

    list_.updateContent();
    if (filtered_.size() > 0)
        list_.selectRow (0);
}
