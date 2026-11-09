#include "PluginEditor.h"
#include "Params.h"
#include "Presets.h"
#include "Tooltips.h"
#include <cstring>

//==============================================================================
GlizzLookAndFeel::GlizzLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);
    setColour (juce::Label::textColourId, Theme::text);

    setColour (juce::ComboBox::backgroundColourId, Theme::control);
    setColour (juce::ComboBox::outlineColourId, Theme::track);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, Theme::accent);

    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::headerTextColourId, Theme::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);

    setColour (juce::TextButton::buttonColourId, Theme::control);
    setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TextButton::textColourOnId, Theme::bg);

    setColour (juce::AlertWindow::backgroundColourId, Theme::panel);
    setColour (juce::AlertWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::outlineColourId, Theme::edge);
    setColour (juce::TextEditor::backgroundColourId, Theme::bg);
    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::outlineColourId, Theme::track);
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);

    setColour (juce::BubbleComponent::backgroundColourId, Theme::panel);
    setColour (juce::BubbleComponent::outlineColourId, Theme::edge);
    setColour (juce::TooltipWindow::backgroundColourId, Theme::panel);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::TooltipWindow::outlineColourId, Theme::edge);
}

void GlizzLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float stroke = juce::jmax (3.0f, radius * 0.09f);

    juce::Path track;
    track.addCentredArc (cx, cy, radius - stroke, radius - stroke, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::track);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float arcStart = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - arcStart) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, radius - stroke, radius - stroke, 0.0f,
                             juce::jmin (arcStart, angle), juce::jmax (arcStart, angle), true);
        g.setColour (Theme::accent);
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float inner = radius - stroke * 2.6f;
    if (inner <= 2.0f)
        return;

    g.setColour (Theme::control);
    g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);
    g.setColour (Theme::track);
    g.drawEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.0f);

    const float px = cx + std::sin (angle) * inner * 0.75f;
    const float py = cy - std::cos (angle) * inner * 0.75f;
    const float qx = cx + std::sin (angle) * inner * 0.25f;
    const float qy = cy - std::cos (angle) * inner * 0.25f;
    g.setColour (slider.isMouseOverOrDragging() ? Theme::accent : Theme::text);
    g.drawLine (qx, qy, px, py, juce::jmax (2.0f, inner * 0.09f));
}

void GlizzLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool isHighlighted, bool isDown)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? Theme::accent : Theme::control;
    if (isHighlighted) c = c.brighter (0.08f);
    if (isDown)        c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Theme::track);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

//==============================================================================
void KnobSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        const int cc = proc.getCCFor (id);
        juce::PopupMenu m;
        m.addItem (1, "MIDI learn (then move a control)");
        m.addItem (2, cc >= 0 ? "Forget MIDI (CC " + juce::String (cc) + ")" : "Forget MIDI", cc >= 0);
        auto& p = proc;
        const auto paramID = id;
        m.showMenuAsync (juce::PopupMenu::Options(), [&p, paramID] (int r)
        {
            if (r == 1) p.startLearn (paramID);
            else if (r == 2) p.forgetMapping (paramID);
        });
        return;
    }
    juce::Slider::mouseDown (e);
}

LabeledKnob::LabeledKnob (GlizzarineProcessor& p, const juce::String& paramID, const juce::String& name, bool isBig)
    : proc (p), id (paramID), slider (p, paramID), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setTooltip (Tooltips::get (paramID));
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (big ? 14.0f : 12.0f)));
    label.setColour (juce::Label::textColourId, big ? Theme::text : Theme::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramID, slider);
    if (auto* param = p.apvts.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void LabeledKnob::setHighlight (bool on)
{
    label.setColour (juce::Label::textColourId, on ? Theme::accent : (big ? Theme::text : Theme::textDim));
}

void LabeledKnob::rebind (const juce::String& paramID, const juce::String& name)
{
    if (paramID == id && label.getText() == name)
        return;

    attachment.reset();
    id = paramID;
    slider.setParamID (paramID);
    slider.setTooltip (Tooltips::get (paramID));
    label.setText (name, juce::dontSendNotification);

    if (paramID.isNotEmpty() && proc.apvts.getParameter (paramID) != nullptr)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramID, slider);
        auto* param = proc.apvts.getParameter (paramID);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }
}

void LabeledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (big ? 20 : 16));
    slider.setBounds (b);
}

void LabeledKnob::paintOverChildren (juce::Graphics& g)
{
    if (proc.getLearnTarget() == id)
    {
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.5f);
    }
}

//==============================================================================
ParamCombo::ParamCombo (GlizzarineProcessor& p, const juce::String& paramID, const juce::String& name)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (paramID)))
        box.addItemList (choice->choices, 1);
    box.setTooltip (Tooltips::get (paramID));
    addAndMakeVisible (box);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, paramID, box);
}

void ParamCombo::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (16));
    box.setBounds (b.withSizeKeepingCentre (b.getWidth() - 4, juce::jmin (28, b.getHeight())));
}

//==============================================================================
OscTop::OscTop (GlizzarineProcessor& p, int i) : proc (p), index (i)
{
    box.addItemList (Params::oscTypes(), 1);
    box.setTooltip (Tooltips::get (Params::osc (i, "type")));
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, Params::osc (i, "type"), box);
    startTimerHz (15);
}

void OscTop::timerCallback()
{
    const int type = (int) std::lround (proc.apvts.getRawParameterValue (Params::osc (index, "type"))->load());
    const float shape = proc.apvts.getRawParameterValue (Params::osc (index, "shape"))->load();
    const float color = proc.apvts.getRawParameterValue (Params::osc (index, "color"))->load();

    if (type != lastType)
    {
        if (shapeK != nullptr) shapeK->setLabelText (Params::shapeName (type));
        if (colorK != nullptr) colorK->setLabelText (Params::colorName (type));
    }

    if (type != lastType || shape != lastShape || color != lastColor)
    {
        lastType = type;
        lastShape = shape;
        lastColor = color;
        GlizzarineOsc::renderPreview (type, shape, color, wave.data(), (int) wave.size(), &proc.getWaveBank());

        float peak = 0.0f;
        for (auto s : wave) peak = juce::jmax (peak, std::abs (s));
        if (peak > 1.0e-6f)
            for (auto& s : wave) s /= peak;
        repaint();
    }
}

void OscTop::resized()
{
    auto b = getLocalBounds();
    box.setBounds (b.removeFromBottom (26));
    b.removeFromBottom (6);
    scopeArea = b;
}

void OscTop::paint (juce::Graphics& g)
{
    const auto b = scopeArea.toFloat();
    g.setColour (Theme::bg);
    g.fillRoundedRectangle (b, 6.0f);
    if (lastType < 0)
        return;

    const auto area = b.reduced (8.0f, 8.0f);
    juce::Path path;
    const int n = (int) wave.size();
    for (int i = 0; i < n; ++i)
    {
        const float px = area.getX() + area.getWidth() * (float) i / (float) (n - 1);
        const float py = area.getCentreY() - wave[(size_t) i] * area.getHeight() * 0.48f;
        if (i == 0) path.startNewSubPath (px, py);
        else        path.lineTo (px, py);
    }
    g.setColour (Theme::accent);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void LiveScope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Theme::bg);
    g.fillRoundedRectangle (b, 6.0f);

    const int w = proc.scopeWrite.load();
    const int n = 512;
    const auto area = b.reduced (8.0f, 6.0f);
    juce::Path path;
    for (int i = 0; i < n; ++i)
    {
        const float s = juce::jlimit (-1.0f, 1.0f, proc.scope[(size_t) ((w - n + i) & 2047)] * 1.6f);
        const float px = area.getX() + area.getWidth() * (float) i / (float) (n - 1);
        const float py = area.getCentreY() - s * area.getHeight() * 0.48f;
        if (i == 0) path.startNewSubPath (px, py);
        else        path.lineTo (px, py);
    }
    g.setColour (Theme::accent);
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

//==============================================================================
SetupPanel::SetupPanel (GlizzarineProcessor& p) : proc (p)
{
    learnButton.onClick = [this]
    {
        if (proc.getWizardStep() >= 0) proc.cancelWizard();
        else                           proc.startWizard();
    };
    clearButton.onClick = [this] { proc.clearMappings(); };
    addAndMakeVisible (learnButton);
    addAndMakeVisible (clearButton);
    startTimerHz (10);
}

void SetupPanel::resized()
{
    auto b = getLocalBounds().reduced (20);
    auto row = b.removeFromTop (34);
    learnButton.setBounds (row.removeFromLeft (160));
    row.removeFromLeft (10);
    clearButton.setBounds (row.removeFromLeft (110));
}

void SetupPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (44);

    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    if (proc.getWizardStep() >= 0)
    {
        g.setColour (Theme::accent);
        g.drawText (proc.getWizardText(), area.removeFromTop (28), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Theme::textDim);
        g.drawText ("Set your KeyLab to User mode, click Learn KeyLab and follow the steps. Right-click any knob to map it on its own.",
                    area.removeFromTop (28), juce::Justification::centredLeft);
    }
    area.removeFromTop (10);

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    const int colW = area.getWidth() / 2;
    for (int i = 0; i < GlizzarineProcessor::numWizardSteps; ++i)
    {
        const int col = i / 8, rowIdx = i % 8;
        auto r = juce::Rectangle<int> (area.getX() + col * colW, area.getY() + rowIdx * 30, colW - 20, 28);
        const auto target = proc.getWizardTarget (i);
        const int cc = proc.getCCFor (target);
        const bool current = proc.getWizardStep() == i;

        g.setColour (current ? Theme::accent.withAlpha (0.18f) : Theme::control);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (Theme::text);
        const juce::String control = i < 8 ? "Knob " + juce::String (i + 1) : "Fader " + juce::String (i - 7);
        g.drawText (control, r.reduced (10, 0), juce::Justification::centredLeft);
        g.setColour (Theme::textDim);
        g.drawText (proc.describeTarget (target), r.reduced (90, 0).withTrimmedRight (-80), juce::Justification::centredLeft);
        g.setColour (cc >= 0 ? Theme::accent : Theme::textDim);
        g.drawText (cc >= 0 ? "CC " + juce::String (cc) : "not set", r.reduced (10, 0), juce::Justification::centredRight);
    }
}

//==============================================================================
void GlizzLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool isHighlighted, bool)
{
    // Pill-shaped on/off switch
    const auto r = b.getLocalBounds().toFloat().withSizeKeepingCentre (34.0f, 18.0f);
    const bool on = b.getToggleState();
    auto c = on ? Theme::accent : Theme::track;
    if (isHighlighted) c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 9.0f);
    const float d = 12.0f;
    const float x = on ? r.getRight() - d - 3.0f : r.getX() + 3.0f;
    g.setColour (on ? Theme::bg : Theme::text);
    g.fillEllipse (x, r.getCentreY() - d * 0.5f, d, d);
}

//==============================================================================
ParamSwitch::ParamSwitch (GlizzarineProcessor& p, const juce::String& paramID, const juce::String& name)
{
    button.setTooltip (Tooltips::get (paramID));
    addAndMakeVisible (button);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramID, button);
}

void ParamSwitch::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (16));
    button.setBounds (b.withSizeKeepingCentre (juce::jmin (b.getWidth(), 50), juce::jmin (b.getHeight(), 30)));
}

//==============================================================================
Card::Card (GlizzarineProcessor& p, juce::String t, int cols, const juce::String& switchParam)
    : proc (p), title (std::move (t)), columns (juce::jmax (1, cols))
{
    if (switchParam.isNotEmpty())
    {
        power = std::make_unique<juce::ToggleButton>();
        power->setTooltip (Tooltips::get (switchParam));
        addAndMakeVisible (*power);
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, switchParam, *power);
        power->onStateChange = [this] { updateEnabled(); };
    }
}

void Card::updateEnabled()
{
    const bool on = power == nullptr || power->getToggleState();
    for (auto& item : items)
        item->setAlpha (on ? 1.0f : 0.4f);
    if (top != nullptr)
        top->setAlpha (on ? 1.0f : 0.4f);
    repaint();
}

LabeledKnob* Card::addKnob (const juce::String& id, const juce::String& name, bool big)
{
    auto k = std::make_unique<LabeledKnob> (proc, id, name, big);
    auto* raw = k.get();
    addItem (std::move (k));
    return raw;
}

void Card::addCombo (const juce::String& id, const juce::String& name)  { addItem (std::make_unique<ParamCombo> (proc, id, name)); }
void Card::addSwitch (const juce::String& id, const juce::String& name) { addItem (std::make_unique<ParamSwitch> (proc, id, name)); }

void Card::addItem (std::unique_ptr<juce::Component> c)
{
    addAndMakeVisible (*c);
    items.push_back (std::move (c));
    updateEnabled();
}

void Card::setTop (std::unique_ptr<juce::Component> component, int height)
{
    top = std::move (component);
    topHeight = height;
    addAndMakeVisible (*top);
    updateEnabled();
}

void Card::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    if (title.isNotEmpty())
    {
        const bool on = power == nullptr || power->getToggleState();
        g.setColour (on ? Theme::text : Theme::textDim);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (title, b.removeFromTop (28.0f).reduced (12.0f, 0.0f), juce::Justification::centredLeft);
    }
}

void Card::resized()
{
    auto b = getLocalBounds().reduced (8);
    if (title.isNotEmpty() || power != nullptr)
    {
        auto head = b.removeFromTop (22);
        if (power != nullptr)
            power->setBounds (head.removeFromRight (44));
    }

    if (top != nullptr)
    {
        top->setBounds (b.removeFromTop (topHeight));
        b.removeFromTop (6);
    }

    const int n = (int) items.size();
    if (n == 0)
        return;

    const int rows = (n + columns - 1) / columns;
    const int cellW = b.getWidth() / columns;
    const int cellH = b.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const int r = i / columns, c = i % columns;
        items[(size_t) i]->setBounds (juce::Rectangle<int> (b.getX() + c * cellW, b.getY() + r * cellH, cellW, cellH).reduced (2));
    }
}

//==============================================================================
juce::Rectangle<float> ArpDisplay::cellArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (28.0f).withTrimmedBottom (16.0f);
}

void ArpDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Theme::bg);
    g.fillRoundedRectangle (b, 6.0f);

    auto& s = proc.apvts;
    const bool on = s.getRawParameterValue ("arp_on")->load() > 0.5f;
    const int rhythm = (int) std::lround (s.getRawParameterValue ("arp_rhythm")->load());
    const int step = proc.arpDisplayStep.load();
    const int cells = numCells();

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (on ? Theme::text : Theme::textDim);
    g.drawText (on ? juce::String (cells) + " steps per bar. The 1 is accented. Click the steps to draw your own rhythm."
                   : "Arpeggiator is off. Switch it on in the corner, then hold a chord.",
                b.reduced (12.0f, 0.0f).removeFromTop (32.0f), juce::Justification::centredLeft);

    const auto area = cellArea();
    const float cw = area.getWidth() / (float) cells;
    const char* pattern = Params::rhythmPattern (juce::jmin (rhythm, 9));
    const int len = (int) std::strlen (pattern);
    const bool custom = rhythm == Params::customRhythm;

    for (int i = 0; i < cells; ++i)
    {
        bool hit, accentCell;
        if (custom)
        {
            hit = s.getRawParameterValue (Params::arpStep (i % 16))->load() > 0.5f;
            accentCell = i == 0;
        }
        else
        {
            hit = pattern[i % len] != '.';
            accentCell = pattern[i % len] == 'X' || i == 0;
        }

        auto cell = juce::Rectangle<float> (area.getX() + cw * (float) i, area.getY(), cw, area.getHeight()).reduced (2.0f);
        g.setColour (hit ? Theme::accent.withAlpha (accentCell ? 1.0f : 0.55f) : Theme::control);
        g.fillRoundedRectangle (cell, 4.0f);

        const int shown = custom ? i % 16 : i % len;
        if (on && step >= 0 && i == step && shown == (custom ? step % 16 : step % len))
        {
            g.setColour (Theme::text);
            g.drawRoundedRectangle (cell.expanded (1.5f), 5.0f, 2.0f);
        }

        g.setColour (i == 0 ? Theme::accent : Theme::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (cell.getX(), area.getBottom() + 1.0f, cell.getWidth(), 14.0f),
                    juce::Justification::centred);
    }
}

void ArpDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto area = cellArea();
    if (! area.contains (e.position))
        return;

    const int cells = numCells();
    const int i = juce::jlimit (0, cells - 1, (int) ((e.position.x - area.getX()) / (area.getWidth() / (float) cells)));
    auto& s = proc.apvts;
    const int rhythm = (int) std::lround (s.getRawParameterValue ("arp_rhythm")->load());

    // Switching to Custom starts from the rhythm that was showing
    if (rhythm != Params::customRhythm)
    {
        const char* pattern = Params::rhythmPattern (juce::jmin (rhythm, 9));
        const int len = (int) std::strlen (pattern);
        for (int k = 0; k < 16; ++k)
            if (auto* p = s.getParameter (Params::arpStep (k)))
                p->setValueNotifyingHost (pattern[k % len] != '.' ? 1.0f : 0.0f);
        if (auto* p = s.getParameter ("arp_rhythm"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) Params::customRhythm));
    }

    if (auto* p = s.getParameter (Params::arpStep (i % 16)))
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    repaint();
}

//==============================================================================
PresetBrowser::PresetBrowser (GlizzarineProcessor& p) : proc (p)
{
    const char* cats[] = { "Favs", "Pads", "Keys", "Leads", "Bass", "Plucks", "Arps", "Video", "Weird", "User" };
    for (auto* c : cats)
    {
        auto b = std::make_unique<juce::TextButton> (c);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (777);
        const juce::String name (c);
        b->onClick = [this, name] { selectCategory (name); };
        addAndMakeVisible (*b);
        chips.push_back (std::move (b));
    }

    list.setModel (this);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, Theme::panel);
    addAndMakeVisible (list);

    const auto current = proc.getPresetCategory();
    selectCategory (presetCategories().contains (current) ? current : juce::String ("Pads"));
}

void PresetBrowser::selectCategory (const juce::String& c)
{
    category = c;
    for (auto& chip : chips)
        chip->setToggleState (chip->getButtonText() == c, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::refresh()
{
    entries.clear();
    const auto& factory = getFactoryPresets();
    const auto& users = proc.getUserPresets();

    if (category == "Favs")
    {
        for (int i = 0; i < (int) factory.size(); ++i)
            if (proc.isFavourite (factory[(size_t) i].name))
                entries.push_back ({ factory[(size_t) i].name, i, -1 });
        for (int i = 0; i < (int) users.size(); ++i)
            if (proc.isFavourite (users[(size_t) i].name))
                entries.push_back ({ users[(size_t) i].name, -1, i });
    }
    else if (category == "User")
    {
        for (int i = 0; i < (int) users.size(); ++i)
            entries.push_back ({ users[(size_t) i].name, -1, i });
    }
    else
    {
        for (int i = 0; i < (int) factory.size(); ++i)
            if (category == factory[(size_t) i].category)
                entries.push_back ({ factory[(size_t) i].name, i, -1 });
    }

    list.updateContent();
    list.repaint();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (row < 0 || row >= (int) entries.size())
        return;

    const auto& e = entries[(size_t) row];
    const bool current = (e.factory >= 0 && e.factory == proc.getCurrentFactory())
                      || (e.user >= 0 && e.user == proc.getCurrentUser());
    if (current)
    {
        g.setColour (Theme::control);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f), 5.0f);
    }

    g.setColour (current ? Theme::accent : Theme::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    const juce::String star = proc.isFavourite (e.name) ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) : juce::String();
    g.drawText (star + e.name, juce::Rectangle<int> (10, 0, width - 14, height), juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) entries.size())
        return;
    const auto& e = entries[(size_t) row];
    if (e.factory >= 0) proc.loadFactory (e.factory);
    else                proc.loadUser (e.user);
    list.repaint();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (Theme::edge);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);
}

void PresetBrowser::resized()
{
    auto b = getLocalBounds().reduced (8);
    auto chipArea = b.removeFromTop (88);
    const int perRow = 3;
    const int w = chipArea.getWidth() / perRow;
    for (int i = 0; i < (int) chips.size(); ++i)
        chips[(size_t) i]->setBounds (chipArea.getX() + (i % perRow) * w, chipArea.getY() + (i / perRow) * 22, w - 4, 20);
    b.removeFromTop (4);
    list.setBounds (b);
}

//==============================================================================
Card& GlizzarineEditor::makeCard (juce::Component& parent, const juce::String& title, int columns, const juce::String& switchParam)
{
    cards.push_back (std::make_unique<Card> (proc, title, columns, switchParam));
    auto& c = *cards.back();
    parent.addAndMakeVisible (c);
    return c;
}

GlizzarineEditor::GlizzarineEditor (GlizzarineProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    //--- Play: browser, 8 macros, scope, volume
    browser = std::make_unique<PresetBrowser> (p);
    pages[PlayPage].addAndMakeVisible (*browser);

    macroCard = &makeCard (pages[PlayPage], "", 4);
    for (int i = 0; i < 8; ++i)
        macroKnobs[(size_t) i] = macroCard->addKnob ("master", "", true);

    scopeCard = &makeCard (pages[PlayPage], "", 1);
    scopeCard->addItem (std::make_unique<LiveScope> (p));
    volumeCard = &makeCard (pages[PlayPage], "", 1);
    volumeCard->addKnob ("master", "Volume");

    //--- Sound
    auto& soundMain = pages[SoundPage];
    auto& soundMore = pages[MotionPage];

    for (int i = 0; i < 3; ++i)
    {
        auto& c = makeCard (soundMain, "Oscillator " + juce::String (i + 1), 5);
        auto top = std::make_unique<OscTop> (p, i);
        auto* topRaw = top.get();
        c.setTop (std::move (top), 92);
        auto* sk = c.addKnob (Params::osc (i, "shape"), "Shape");
        auto* ck = c.addKnob (Params::osc (i, "color"), "Color");
        c.addKnob (Params::osc (i, "level"), "Level");
        c.addKnob (Params::osc (i, "pitch"), "Pitch");
        c.addKnob (Params::osc (i, "detune"), "Detune");
        topRaw->setModKnobs (sk, ck);
        oscCard[(size_t) i] = &c;
    }

    filterCard = &makeCard (soundMain, "Filter", 4);
    filterCard->addCombo ("filter_type", "Type");
    filterCard->addKnob ("cutoff", "Cutoff");
    filterCard->addKnob ("reso", "Resonance");
    filterCard->addKnob ("fenv_amt", "Envelope");

    envCard = &makeCard (soundMain, "Envelope", 4);
    envCard->addKnob ("amp_attack", "Attack");
    envCard->addKnob ("amp_decay", "Decay");
    envCard->addKnob ("amp_sustain", "Sustain");
    envCard->addKnob ("amp_release", "Release");

    playCard = &makeCard (soundMain, "Playing", 5);
    playCard->addCombo ("mode", "Mode");
    playCard->addKnob ("glide", "Glide");
    playCard->addKnob ("vib_wheel", "Wheel vibrato");
    playCard->addKnob ("vib_at", "Touch vibrato");
    playCard->addKnob ("at_cutoff", "Touch filter");

    //--- Sound (more)
    lfo1Card = &makeCard (soundMore, "LFO 1", 3);
    lfo1Card->addCombo ("lfo1_wave", "Wave");
    lfo1Card->addCombo ("lfo1_sync", "Sync");
    lfo1Card->addKnob ("lfo1_rate", "Speed");

    lfo2Card = &makeCard (soundMore, "LFO 2", 3);
    lfo2Card->addCombo ("lfo2_wave", "Wave");
    lfo2Card->addCombo ("lfo2_sync", "Sync");
    lfo2Card->addKnob ("lfo2_rate", "Speed");

    fenvCard = &makeCard (soundMore, "Filter envelope", 4);
    fenvCard->addKnob ("flt_attack", "Attack");
    fenvCard->addKnob ("flt_decay", "Decay");
    fenvCard->addKnob ("flt_sustain", "Sustain");
    fenvCard->addKnob ("flt_release", "Release");

    toneCard = &makeCard (soundMore, "Tone", 3);
    toneCard->addKnob ("filter_drive", "Drive");
    toneCard->addKnob ("xfm", "Cross FM");
    toneCard->addKnob ("ring", "Ring mod");

    for (int i = 0; i < 4; ++i)
    {
        auto& c = makeCard (soundMore, "Mod slot " + juce::String (i + 1), 1);
        c.addCombo (Params::slot (i, "src"), "Source");
        c.addCombo (Params::slot (i, "dst"), "Target");
        c.addKnob (Params::slot (i, "amt"), "Amount");
        slotCard[(size_t) i] = &c;
    }

    physicsCard = &makeCard (soundMore, "Strange physics", 2, "physics_on");
    physicsCard->addKnob ("gravity", "Gravity");
    physicsCard->addKnob ("entropy", "Entropy");
    physicsCard->addKnob ("drift", "Drift");
    physicsCard->addKnob ("orbit", "Orbit");

    //--- Effects: one card per effect, each with its own switch
    auto& fx = pages[EffectsPage];
    fxCard[0] = &makeCard (fx, "Drive", 2, "drive_on");
    fxCard[0]->addCombo ("dist_type", "Type");
    fxCard[0]->addKnob ("dist_amt", "Amount");
    fxCard[1] = &makeCard (fx, "Tape", 1, "tape_on");
    fxCard[1]->addKnob ("tape", "Amount");
    fxCard[2] = &makeCard (fx, "Chorus", 2, "chorus_on");
    fxCard[2]->addCombo ("chorus_type", "Type");
    fxCard[2]->addKnob ("chorus_rate", "Speed");
    fxCard[2]->addKnob ("chorus_depth", "Depth");
    fxCard[2]->addKnob ("chorus_mix", "Amount");
    fxCard[3] = &makeCard (fx, "Phaser", 2, "phaser_on");
    fxCard[3]->addKnob ("phaser_rate", "Speed");
    fxCard[3]->addKnob ("phaser_depth", "Depth");
    fxCard[3]->addKnob ("phaser_mix", "Amount");
    fxCard[4] = &makeCard (fx, "Delay", 3, "delay_on");
    fxCard[4]->addCombo ("delay_sync", "Sync");
    fxCard[4]->addKnob ("delay_time", "Free time");
    fxCard[4]->addKnob ("delay_fb", "Feedback");
    fxCard[4]->addKnob ("delay_tone", "Tone");
    fxCard[4]->addKnob ("delay_mix", "Amount");
    fxCard[5] = &makeCard (fx, "Grains", 2, "grains_on");
    fxCard[5]->addKnob ("cloud_size", "Size");
    fxCard[5]->addKnob ("cloud_density", "Density");
    fxCard[5]->addKnob ("cloud_spray", "Spray");
    fxCard[5]->addKnob ("cloud_mix", "Amount");
    fxCard[6] = &makeCard (fx, "Reverb", 2, "reverb_on");
    fxCard[6]->addKnob ("reverb_size", "Size");
    fxCard[6]->addKnob ("reverb_mix", "Amount");
    fxCard[7] = &makeCard (fx, "Shimmer", 1, "shimmer_on");
    fxCard[7]->addKnob ("reverb_shimmer", "Amount");
    fxCard[8] = &makeCard (fx, "Rhythm gate", 2, "gate_on");
    fxCard[8]->addCombo ("gate_pattern", "Pattern");
    fxCard[8]->addKnob ("gate_depth", "Amount");
    fxCard[9] = &makeCard (fx, "Width", 2, "stereo_on");
    fxCard[9]->addKnob ("wide", "Wide");
    fxCard[9]->addKnob ("tremolo", "Tremolo");
    fxCard[9]->addKnob ("tremolo_rate", "Tremolo speed");

    //--- Arp
    arpCard = &makeCard (pages[ArpPage], "Arpeggiator", 6, "arp_on");
    arpCard->setTop (std::make_unique<ArpDisplay> (p), 120);
    arpCard->addCombo ("arp_mode", "Pattern");
    arpCard->addCombo ("arp_rhythm", "Rhythm");
    arpCard->addCombo ("arp_speed", "Speed");
    arpCard->addSwitch ("arp_hold", "Hold");
    arpCard->addSwitch ("arp_lock", "Keep on preset change");
    arpCard->addKnob ("arp_octaves", "Octaves");
    arpCard->addKnob ("arp_gate", "Length");
    arpCard->addKnob ("arp_swing", "Swing");
    arpCard->addKnob ("arp_mutate", "Mutation");
    arpCard->addKnob ("arp_glitch", "Glitch");

    tempoCard = &makeCard (pages[ArpPage], "Tempo and bar", 4);
    tempoCard->addCombo ("time_sig", "Time signature");
    tempoCard->addCombo ("clock_mode", "Tempo source");
    {
        auto tapButton = std::make_unique<juce::TextButton> ("Tap");
        tapButton->setTooltip ("Click four or five times in time to set your own tempo.");
        tapButton->onClick = [this] { proc.tap(); };
        tempoCard->addItem (std::move (tapButton));
    }
    tempoCard->addKnob ("own_bpm", "BPM");

    //--- Setup
    setupPanel = std::make_unique<SetupPanel> (p);
    pages[SetupPage].addAndMakeVisible (*setupPanel);

    //--- Tabs
    const char* tabNames[numPages] = { "Play", "Sound", "Motion", "Effects", "Arp", "Setup" };
    for (int i = 0; i < numPages; ++i)
    {
        auto& t = tabs[(size_t) i];
        t.setButtonText (tabNames[i]);
        t.setClickingTogglesState (true);
        t.setRadioGroupId (4242);
        t.onClick = [this, i] { showPage (i); };
        addAndMakeVisible (t);
        addChildComponent (pages[(size_t) i]);
    }

    //--- Header
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setFont (juce::Font (juce::FontOptions (15.0f)));
    presetName.setColour (juce::Label::backgroundColourId, Theme::control);
    presetName.setColour (juce::Label::outlineColourId, Theme::track);
    addAndMakeVisible (presetName);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    starButton.onClick = [this] { proc.toggleFavourite(); };
    starButton.setTooltip ("Mark this sound as a favourite");
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    aButton.onClick = [this] { proc.switchAB (0); };
    bButton.onClick = [this] { proc.switchAB (1); };
    aButton.setTooltip ("Compare two versions of your sound");
    bButton.setTooltip ("Compare two versions of your sound");
    copyButton.setTooltip ("Copy the current version to the other slot");
    copyButton.onClick = [this] { proc.copyToOtherSlot(); };
    randomButton.setTooltip ("Roll a completely new sound");
    randomButton.onClick = [this] { proc.randomize(); };
    saveButton.setTooltip ("Save this sound as your own preset");
    saveButton.onClick = [this] { savePreset(); };

    for (auto* b : { &prevButton, &nextButton, &starButton, &undoButton, &redoButton, &aButton, &bButton,
                     &copyButton, &randomButton, &saveButton })
        addAndMakeVisible (b);

    keyboard.setLowestVisibleKey (36);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Theme::accent.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Theme::accent.withAlpha (0.25f));
    addAndMakeVisible (keyboard);

    setSize (1180, 780);
    showPage (PlayPage);
    startTimerHz (10);
}

GlizzarineEditor::~GlizzarineEditor()
{
    setLookAndFeel (nullptr);
}

void GlizzarineEditor::showPage (int page)
{
    currentPage = juce::jlimit (0, numPages - 1, page);
    for (int i = 0; i < numPages; ++i)
    {
        pages[(size_t) i].setVisible (i == currentPage);
        tabs[(size_t) i].setToggleState (i == currentPage, juce::dontSendNotification);
    }
}

void GlizzarineEditor::refreshHeader()
{
    presetName.setText (proc.getPresetName(), juce::dontSendNotification);
    const bool fav = proc.isFavourite (proc.getPresetName());
    starButton.setButtonText (juce::String (juce::CharPointer_UTF8 (fav ? "\xe2\x98\x85" : "\xe2\x98\x86")));
    starButton.setToggleState (fav, juce::dontSendNotification);
    aButton.setToggleState (proc.getABSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getABSlot() == 1, juce::dontSendNotification);
}

void GlizzarineEditor::stepPreset (int delta)
{
    const int n = (int) getFactoryPresets().size();
    const int cur = juce::jmax (0, proc.getCurrentFactory());
    proc.loadFactory ((cur + delta + n) % n);
}

void GlizzarineEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save sound", "Give your sound a name", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getPresetName() == "Random sound" ? juce::String() : proc.getPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
            proc.saveUser (w->getTextEditorContents ("name"));
    }), true);
}

void GlizzarineEditor::timerCallback()
{
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undoManager.beginNewTransaction();

    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    if (proc.getPresetVersion() != lastPresetVersion)
    {
        lastPresetVersion = proc.getPresetVersion();
        refreshHeader();
        browser->refresh();
        repaint();
    }

    const int tpl = proc.getMacroTemplateIndex();
    const auto& macros = getMacroTemplate (tpl).macros;
    const double now = juce::Time::getMillisecondCounterHiRes();

    for (int i = 0; i < 8; ++i)
    {
        auto* k = macroKnobs[(size_t) i];
        if (tpl != lastTemplate)
        {
            const auto* pid = macros[i].targets[0].id;
            k->rebind (pid != nullptr ? juce::String (pid) : juce::String ("master"), macros[i].name);
            macroLast[(size_t) i] = (float) k->getSlider().getValue();
        }

        const float v = (float) k->getSlider().getValue();
        if (v != macroLast[(size_t) i])
        {
            macroLast[(size_t) i] = v;
            macroTouched[(size_t) i] = now;
        }
        k->setHighlight (now - macroTouched[(size_t) i] < 900.0);
    }
    lastTemplate = tpl;

    for (auto& card : cards)
        card->updateEnabled();

    if (proc.getLearnTarget().isNotEmpty())
        pages[(size_t) currentPage].repaint();
}

void GlizzarineEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    auto header = getLocalBounds().removeFromTop (54).reduced (18, 0).toFloat();
    g.setColour (Theme::accent);
    g.fillEllipse (header.getX(), header.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f)));
    g.drawText ("Glizzarine", header.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
}

void GlizzarineEditor::layoutRow (juce::Rectangle<int> row, std::initializer_list<std::pair<juce::Component*, float>> parts)
{
    const int gap = 8;
    const int usable = row.getWidth() - gap * ((int) parts.size() - 1);
    int index = 0;
    for (const auto& part : parts)
    {
        const bool last = index == (int) parts.size() - 1;
        const int w = last ? row.getWidth() : (int) ((float) usable * part.second);
        part.first->setBounds (row.removeFromLeft (w));
        if (! last)
            row.removeFromLeft (gap);
        ++index;
    }
}

void GlizzarineEditor::resized()
{
    auto b = getLocalBounds().reduced (14, 0);

    auto header = b.removeFromTop (54).reduced (0, 11);
    header.removeFromLeft (150);
    saveButton.setBounds (header.removeFromRight (60));   header.removeFromRight (6);
    randomButton.setBounds (header.removeFromRight (72)); header.removeFromRight (12);
    copyButton.setBounds (header.removeFromRight (54));   header.removeFromRight (4);
    bButton.setBounds (header.removeFromRight (32));      header.removeFromRight (4);
    aButton.setBounds (header.removeFromRight (32));      header.removeFromRight (12);
    redoButton.setBounds (header.removeFromRight (54));   header.removeFromRight (4);
    undoButton.setBounds (header.removeFromRight (54));   header.removeFromRight (16);
    prevButton.setBounds (header.removeFromLeft (30));    header.removeFromLeft (4);
    starButton.setBounds (header.removeFromRight (32));   header.removeFromRight (4);
    nextButton.setBounds (header.removeFromRight (30));   header.removeFromRight (4);
    presetName.setBounds (header);

    auto tabRow = b.removeFromTop (34).withTrimmedBottom (4);
    for (auto& t : tabs)
    {
        t.setBounds (tabRow.removeFromLeft (100));
        tabRow.removeFromLeft (6);
    }

    b.removeFromTop (6);
    keyboard.setBounds (b.removeFromBottom (64).withTrimmedBottom (8));
    b.removeFromBottom (8);

    for (auto& pg : pages)
        pg.setBounds (b);

    const auto area = juce::Rectangle<int> (0, 0, b.getWidth(), b.getHeight());
    const int gap = 8;

    // Play
    {
        auto a = area;
        browser->setBounds (a.removeFromLeft (250));
        a.removeFromLeft (gap);
        macroCard->setBounds (a.removeFromTop ((int) (area.getHeight() * 0.68f)));
        a.removeFromTop (gap);
        volumeCard->setBounds (a.removeFromRight (120));
        a.removeFromRight (gap);
        scopeCard->setBounds (a);
    }

    // Sound
    {
        auto m = area;
        layoutRow (m.removeFromTop ((int) (area.getHeight() * 0.56f)), { { oscCard[0], 0.333f }, { oscCard[1], 0.333f }, { oscCard[2], 0.334f } });
        m.removeFromTop (gap);
        layoutRow (m, { { filterCard, 0.3f }, { envCard, 0.3f }, { playCard, 0.4f } });
    }

    // Motion
    {
        auto r = area;
        layoutRow (r.removeFromTop ((int) (area.getHeight() * 0.48f)), { { lfo1Card, 0.22f }, { lfo2Card, 0.22f }, { fenvCard, 0.32f }, { toneCard, 0.24f } });
        r.removeFromTop (gap);
        layoutRow (r, { { slotCard[0], 0.17f }, { slotCard[1], 0.17f }, { slotCard[2], 0.17f }, { slotCard[3], 0.17f }, { physicsCard, 0.32f } });
    }

    // Effects: two rows of five cards
    {
        auto a = area;
        const int rowH = (area.getHeight() - gap) / 2;
        layoutRow (a.removeFromTop (rowH), { { fxCard[0], 0.2f }, { fxCard[1], 0.2f }, { fxCard[2], 0.2f }, { fxCard[3], 0.2f }, { fxCard[4], 0.2f } });
        a.removeFromTop (gap);
        layoutRow (a, { { fxCard[5], 0.2f }, { fxCard[6], 0.2f }, { fxCard[7], 0.2f }, { fxCard[8], 0.2f }, { fxCard[9], 0.2f } });
    }

    // Arp
    {
        auto a = area;
        arpCard->setBounds (a.removeFromTop ((int) (area.getHeight() * 0.7f)));
        a.removeFromTop (gap);
        tempoCard->setBounds (a.withWidth (juce::jmin (a.getWidth(), 640)));
    }

    setupPanel->setBounds (area);
}
