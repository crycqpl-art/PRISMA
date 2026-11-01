#include "PluginEditor.h"
#include "Presets.h"

using namespace prisma;

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static juce::String slotLetter (int i) { return juce::String::charToString ((juce::juce_wchar) ('A' + i)); }

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, Colours::panel().brighter (0.05f));
    setColour (juce::ComboBox::outlineColourId, Colours::line());
    setColour (juce::ComboBox::textColourId, Colours::text());
    setColour (juce::ComboBox::arrowColourId, Colours::dim());
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel());
    setColour (juce::PopupMenu::textColourId, Colours::text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::line());
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text());
    setColour (juce::TextButton::buttonColourId, Colours::panel().brighter (0.06f));
    setColour (juce::TextButton::textColourOffId, Colours::text());
    setColour (juce::Label::textColourId, Colours::dim());
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panel());
    setColour (juce::TooltipWindow::textColourId, Colours::text());
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float a0, float a1, juce::Slider&)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
    g.setColour (Colours::line());
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, ang, true);
    g.setColour (Colours::accent());
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - 8.0f;
    g.setColour (Colours::panel().brighter (0.12f));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (Colours::text());
    g.drawLine (c.x + kr * 0.3f * std::sin (ang), c.y - kr * 0.3f * std::cos (ang),
                c.x + (kr - 2.0f) * std::sin (ang), c.y - (kr - 2.0f) * std::cos (ang), 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& col, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? col.brighter (0.15f) : over ? col.brighter (0.07f) : col);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::line().brighter (over ? 0.3f : 0.0f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? Colours::accent().withAlpha (0.18f) : Colours::panel().brighter (over ? 0.1f : 0.05f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? Colours::accent() : Colours::line());
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (on ? Colours::text() : Colours::dim());
    g.setFont (uiFont (13.0f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path p;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Colours::dim());
    g.fillPath (p);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)          { return uiFont (13.0f); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return uiFont (13.0f); }

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.0f));
    l->setColour (juce::Label::textColourId, Colours::text().withAlpha (0.85f));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (uiFont (12.0f));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& st, const juce::String& id, const juce::String& text)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (uiFont (12.0f));
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
}

//==============================================================================
SlotView::SlotView (PrismaProcessor& p, int i, std::function<void (int)> onSelect, std::function<void (const juce::String&)> onStatus)
    : proc (p), index (i), select (std::move (onSelect)), status (std::move (onStatus))
{
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load sample into slot " + slotLetter (index), juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile()) load (fc.getResult());
                              });
    };
    clearButton.setTooltip ("Clear slot");
    clearButton.onClick = [this] { proc.clearSlot (index); refresh(); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
}

void SlotView::load (const juce::File& f)
{
    juce::String err;
    if (proc.loadSlot (index, f, err))
    {
        status ("Slot " + slotLetter (index) + ": " + f.getFileName());
        select (index);
    }
    else
        status ("Could not load " + f.getFileName() + ": " + err);
    refresh();
}

void SlotView::refresh()
{
    const void* now = proc.getSlot (index).get();
    if (now == shown && initialised) return;
    initialised = true;
    shown = now;
    clearButton.setVisible (shown != nullptr);
    repaint();
}

bool SlotView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void SlotView::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { load (juce::File (f)); break; }
}

void SlotView::mouseUp (const juce::MouseEvent&) { select (index); }

void SlotView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel());
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (dragOver ? Colours::accent() : selected ? Colours::text().withAlpha (0.6f) : Colours::line());
    g.drawRoundedRectangle (r, 8.0f, dragOver || selected ? 1.5f : 1.0f);

    auto sd = proc.getSlot (index);
    g.setColour (selected ? Colours::accent() : Colours::dim());
    g.setFont (uiFont (16.0f, true));
    g.drawText (slotLetter (index), 12, 8, 20, 20, juce::Justification::centredLeft);

    g.setColour (Colours::text());
    g.setFont (uiFont (13.0f));
    const auto savedPath = proc.apvts.state.getProperty ("slot" + juce::String (index)).toString();
    const juce::String name = sd != nullptr ? sd->name
                            : savedPath.isNotEmpty() ? "Missing: " + juce::File (savedPath).getFileName()
                                                     : juce::String ("Drop a sample here");
    g.setColour (sd != nullptr ? Colours::text() : Colours::dim());
    g.drawText (name, 34, 8, getWidth() - 110, 20, juce::Justification::centredLeft, true);

    auto wave = juce::Rectangle<float> (12.0f, 36.0f, (float) getWidth() - 24.0f, (float) getHeight() - 46.0f);
    if (sd != nullptr && ! sd->overview.empty())
    {
        const int n = (int) wave.getWidth();
        g.setColour (selected ? Colours::text().withAlpha (0.75f) : Colours::wave());
        for (int x = 0; x < n; ++x)
        {
            const auto i = (size_t) ((double) x / n * sd->overview.size());
            const float a = sd->overview[i] * wave.getHeight() * 0.5f;
            g.fillRect (wave.getX() + (float) x, wave.getCentreY() - a, 1.0f, juce::jmax (1.0f, a * 2.0f));
        }
    }
    else
    {
        g.setColour (Colours::line());
        g.drawLine (wave.getX(), wave.getCentreY(), wave.getRight(), wave.getCentreY(), 1.0f);
    }
}

void SlotView::resized()
{
    clearButton.setBounds (getWidth() - 34, 8, 24, 20);
    loadButton.setBounds (getWidth() - 86, 8, 48, 20);
}

//==============================================================================
WaveView::WaveView (PrismaProcessor& p) : proc (p)
{
    setTooltip ("Click or drag to set Position");
    readIndex = proc.chopLog.write.load();
}

void WaveView::tick()
{
    const int w = proc.chopLog.write.load (std::memory_order_acquire);
    if (w - readIndex > ChopLog::size) readIndex = w - ChopLog::size;
    for (; readIndex < w; ++readIndex)
    {
        const auto i = (size_t) (readIndex % ChopLog::size);
        marks.push_back ({ proc.chopLog.pos[i].load(), proc.chopLog.slot[i].load(), 1.0f });
    }
    for (auto& m : marks) m.life -= 0.045f;
    marks.erase (std::remove_if (marks.begin(), marks.end(), [] (const Mark& m) { return m.life <= 0.0f; }), marks.end());
    repaint();
}

void WaveView::setPositionFrom (const juce::MouseEvent& e)
{
    const float x = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    if (auto* prm = proc.apvts.getParameter ("position"))
        prm->setValueNotifyingHost (x);
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    if (auto* prm = proc.apvts.getParameter ("position")) prm->beginChangeGesture();
    setPositionFrom (e);
}
void WaveView::mouseDrag (const juce::MouseEvent& e) { setPositionFrom (e); }
void WaveView::mouseUp (const juce::MouseEvent&)
{
    if (auto* prm = proc.apvts.getParameter ("position")) prm->endChangeGesture();
}

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff121214));
    g.fillRoundedRectangle (r, 8.0f);

    auto sd = proc.getSlot (focus);
    const float W = r.getWidth(), H = r.getHeight(), mid = H * 0.5f;

    // range band around the current centre
    const float position = proc.getParam ("position");
    float centre = position + proc.getScan();
    centre -= std::floor (centre);
    const float range = proc.getParam ("range");
    if (proc.getParam ("playMode") < 0.5f)
    {
        g.setColour (Colours::accent().withAlpha (0.07f));
        const float x0 = (centre - range * 0.5f) * W, x1 = (centre + range * 0.5f) * W;
        g.fillRect (x0, 0.0f, x1 - x0, H);
        if (x0 < 0.0f) g.fillRect (W + x0, 0.0f, -x0, H);
        if (x1 > W) g.fillRect (0.0f, 0.0f, x1 - W, H);
    }

    if (sd == nullptr)
    {
        g.setColour (Colours::dim());
        g.setFont (uiFont (15.0f));
        g.drawText ("Slot " + slotLetter (focus) + " is empty: drop a sample on a slot above",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    // waveform
    const int n = (int) W;
    for (int x = 0; x < n; ++x)
    {
        const auto i = (size_t) ((double) x / n * sd->overview.size());
        const float a = sd->overview[i] * (H * 0.5f - 14.0f);
        g.setColour (Colours::wave().interpolatedWith (Colours::text(), sd->brightness[i] * 0.5f));
        g.fillRect ((float) x, mid - a, 1.0f, juce::jmax (1.0f, a * 2.0f));
    }

    // onsets
    g.setColour (Colours::dim().withAlpha (0.55f));
    for (auto o : sd->onsets)
        g.fillRect (o * W, H - 8.0f, 1.0f, 6.0f);

    // chops that just fired
    for (auto& m : marks)
    {
        const float x = m.pos * W;
        const float a = m.life;
        g.setColour (Colours::accent().withAlpha (0.85f * a * (m.slot == focus ? 1.0f : 0.45f)));
        g.fillRect (x - 1.0f, 6.0f, 2.0f, H - 12.0f);
        g.setColour (Colours::accent().withAlpha (0.12f * a));
        g.fillRect (x, 6.0f, 18.0f * a, H - 12.0f);
    }

    // position + centre
    g.setColour (Colours::text().withAlpha (0.35f));
    g.drawLine (position * W, 0.0f, position * W, H, 1.0f);
    g.setColour (Colours::text());
    g.fillRect (centre * W - 1.0f, 0.0f, 2.0f, H);
}

//==============================================================================
XYPad::XYPad (PrismaProcessor& p) : proc (p)
{
    setTooltip ("Drag to blend the four slots");
}

void XYPad::setFrom (const juce::MouseEvent& e)
{
    const auto a = area();
    const float x = juce::jlimit (0.0f, 1.0f, ((float) e.x - a.getX()) / a.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, ((float) e.y - a.getY()) / a.getHeight());
    proc.apvts.getParameter ("morphX")->setValueNotifyingHost (x);
    proc.apvts.getParameter ("morphY")->setValueNotifyingHost (y);
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    proc.apvts.getParameter ("morphX")->beginChangeGesture();
    proc.apvts.getParameter ("morphY")->beginChangeGesture();
    setFrom (e);
}
void XYPad::mouseDrag (const juce::MouseEvent& e) { setFrom (e); }
void XYPad::mouseUp (const juce::MouseEvent&)
{
    proc.apvts.getParameter ("morphX")->endChangeGesture();
    proc.apvts.getParameter ("morphY")->endChangeGesture();
}

void XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff121214));
    g.fillRoundedRectangle (r, 8.0f);
    const auto a = area();

    g.setColour (Colours::line());
    for (int i = 1; i < 4; ++i)
    {
        const float fx = a.getX() + a.getWidth() * (float) i / 4.0f;
        const float fy = a.getY() + a.getHeight() * (float) i / 4.0f;
        g.drawLine (fx, a.getY(), fx, a.getBottom(), 1.0f);
        g.drawLine (a.getX(), fy, a.getRight(), fy, 1.0f);
    }
    g.drawRect (a, 1.0f);

    const float x = proc.getParam ("morphX"), y = proc.getParam ("morphY");
    const float w[] = { (1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y };
    const juce::Point<float> corners[] = { a.getTopLeft(), a.getTopRight(), a.getBottomLeft(), a.getBottomRight() };
    const juce::Point<float> dot (a.getX() + x * a.getWidth(), a.getY() + y * a.getHeight());

    for (int i = 0; i < 4; ++i)
    {
        const bool loaded = proc.getSlot (i) != nullptr;
        g.setColour (Colours::accent().withAlpha (loaded ? 0.15f + 0.6f * w[i] : 0.05f));
        g.drawLine ({ dot, corners[i] }, 1.0f + 3.0f * w[i]);
        const auto c = juce::Rectangle<float> (20.0f, 20.0f).withCentre (corners[i]);
        g.setColour (loaded ? Colours::panel().brighter (0.25f) : Colours::panel());
        g.fillEllipse (c);
        g.setColour (loaded ? Colours::text() : Colours::dim().withAlpha (0.5f));
        g.setFont (uiFont (12.0f, true));
        g.drawText (slotLetter (i), c, juce::Justification::centred);
    }

    g.setColour (Colours::accent());
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (dot));
    g.setColour (Colours::bg());
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
}

//==============================================================================
PrismaEditor::PrismaEditor (PrismaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p), xy (p),
      playMode (p.apvts, "playMode", "Mode"), rate (p.apvts, "rate", "Rate"),
      shape (p.apvts, "shape", "Shape"), pitchSet (p.apvts, "pitchSet", "Pitch set"),
      position (p.apvts, "position", "Position"), scan (p.apvts, "scan", "Scan"),
      range (p.apvts, "range", "Range"), snap (p.apvts, "snap", "Snap"),
      length (p.apvts, "length", "Length"), probability (p.apvts, "probability", "Probability"),
      reverse (p.apvts, "reverse", "Reverse"), pitchAmount (p.apvts, "pitchAmount", "Pitch rand"),
      cloud (p.apvts, "cloud", "Cloud"), grainSize (p.apvts, "grainSize", "Size"),
      density (p.apvts, "density", "Density"), spread (p.apvts, "spread", "Spread"),
      attack (p.apvts, "attack", "Attack"), release (p.apvts, "release", "Release"),
      crunch (p.apvts, "crunch", "Crunch"), wash (p.apvts, "wash", "Wash"),
      pump (p.apvts, "pump", "Pump"), gain (p.apvts, "gain", "Output")
{
    setLookAndFeel (&lnf);

    presetBox.addItemList (presetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedItemIndex() >= 0)
        {
            proc.applyPreset (presetBox.getSelectedItemIndex());
            setStatus ("Preset: " + presetBox.getText());
        }
    };
    addAndMakeVisible (presetBox);

    diceButton.setTooltip ("Randomise the Offset, Pitch and Cloud settings");
    diceButton.onClick = [this] { proc.dice(); presetBox.setSelectedItemIndex (-1, juce::dontSendNotification); setStatus ("Dice!"); };
    copyButton.setTooltip ("Copy all settings as text, to paste into a chat");
    copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (proc.settingsToText()); setStatus ("Settings copied to clipboard"); };
    pasteButton.setTooltip ("Apply settings text from the clipboard");
    pasteButton.onClick = [this]
    {
        const int n = proc.settingsFromText (juce::SystemClipboard::getTextFromClipboard());
        setStatus (n > 0 ? "Applied " + juce::String (n) + " settings" : juce::String ("No Prisma settings found in the clipboard"));
    };
    for (auto* b : { &diceButton, &copyButton, &pasteButton }) addAndMakeVisible (b);

    for (int i = 0; i < numSlots; ++i)
    {
        slotViews.push_back (std::make_unique<SlotView> (p, i, [this] (int s) { selectSlot (s); },
                                                         [this] (const juce::String& s) { setStatus (s); }));
        addAndMakeVisible (*slotViews.back());
        slotViews.back()->refresh();
    }
    addAndMakeVisible (wave);
    addAndMakeVisible (xy);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &playMode, &rate, &shape, &pitchSet, &position, &scan, &range, &snap, &length, &probability, &reverse,
             &pitchAmount, &cloud, &grainSize, &density, &spread, &attack, &release, &crunch, &wash, &pump, &gain, &slotRandom })
        addAndMakeVisible (c);
    slotRandomAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "slotRandom", slotRandom);
    slotRandom.setTooltip ("Off: chops blend all slots. On: each chop picks one slot.");

    selectSlot (0);
    setSize (1040, 720);
    startTimerHz (30);
}

PrismaEditor::~PrismaEditor() { setLookAndFeel (nullptr); }

void PrismaEditor::selectSlot (int s)
{
    for (int i = 0; i < (int) slotViews.size(); ++i) slotViews[(size_t) i]->setSelected (i == s);
    wave.setFocusSlot (s);
}

void PrismaEditor::setStatus (const juce::String& s)
{
    status = s;
    statusTicks = 150;
    repaint (0, getHeight() - 28, getWidth(), 28);
}

void PrismaEditor::timerCallback()
{
    proc.releaseRetired();
    wave.tick();
    xy.repaint();
    for (auto& s : slotViews) s->refresh();
    if (statusTicks > 0 && --statusTicks == 0) { status.clear(); repaint (0, getHeight() - 28, getWidth(), 28); }
}

void PrismaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg());

    g.setColour (Colours::text());
    g.setFont (uiFont (24.0f, true));
    g.drawText ("PRISMA", 20, 14, 140, 32, juce::Justification::centredLeft);
    g.setColour (Colours::dim());
    g.setFont (uiFont (12.0f));
    g.drawText (juce::String ("v") + PRISMA_VERSION, 132, 22, 60, 20, juce::Justification::centredLeft);

    for (auto& gr : groups)
    {
        auto r = gr.bounds.toFloat();
        g.setColour (Colours::panel());
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line());
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::dim());
        g.setFont (uiFont (11.0f, true));
        g.drawText (gr.title, gr.bounds.getX() + 12, gr.bounds.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }

    g.setColour (Colours::dim());
    g.setFont (uiFont (12.0f));
    const juce::String hint = proc.activeNotes.load() == 0 ? "Play a MIDI note to hear the slots" : juce::String();
    g.drawText (status.isNotEmpty() ? status : hint, 20, getHeight() - 26, getWidth() - 40, 20, juce::Justification::centredLeft);
}

void PrismaEditor::resized()
{
    groups.clear();
    auto area = getLocalBounds().reduced (16, 0);

    auto header = area.removeFromTop (58).withTrimmedTop (14).withTrimmedBottom (12);
    pasteButton.setBounds (header.removeFromRight (120));
    header.removeFromRight (8);
    copyButton.setBounds (header.removeFromRight (120));
    header.removeFromRight (8);
    diceButton.setBounds (header.removeFromRight (70));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (180));

    auto slotsRow = area.removeFromTop (92);
    const int sw = (slotsRow.getWidth() - 3 * 10) / 4;
    for (int i = 0; i < numSlots; ++i)
    {
        slotViews[(size_t) i]->setBounds (slotsRow.removeFromLeft (sw));
        slotsRow.removeFromLeft (10);
    }

    area.removeFromTop (10);
    auto mid = area.removeFromTop (240);
    xy.setBounds (mid.removeFromRight (240));
    mid.removeFromRight (10);
    wave.setBounds (mid);

    area.removeFromTop (10);
    area.removeFromBottom (30);

    auto placeRow = [this] (juce::Rectangle<int> row, std::vector<std::pair<juce::String, std::vector<juce::Component*>>> spec,
                            std::vector<float> weights)
    {
        float total = 0.0f;
        for (auto w : weights) total += w;
        const int gap = 10;
        const int usable = row.getWidth() - gap * ((int) spec.size() - 1);
        for (size_t i = 0; i < spec.size(); ++i)
        {
            const int w = i + 1 == spec.size() ? row.getWidth() : (int) (usable * weights[i] / total);
            auto b = row.removeFromLeft (w);
            row.removeFromLeft (gap);
            groups.push_back ({ spec[i].first, b });
            auto inner = b.reduced (8, 0).withTrimmedTop (24).withTrimmedBottom (8);
            const int cw = inner.getWidth() / (int) spec[i].second.size();
            for (auto* c : spec[i].second)
            {
                auto cell = inner.removeFromLeft (cw).reduced (2, 0);
                if (dynamic_cast<Choice*> (c) != nullptr) cell = cell.withSizeKeepingCentre (cell.getWidth(), 46);
                if (dynamic_cast<juce::ToggleButton*> (c) != nullptr) cell = cell.withSizeKeepingCentre (cell.getWidth(), 28);
                c->setBounds (cell);
            }
        }
    };

    const int rowH = (area.getHeight() - 10) / 2;
    auto row1 = area.removeFromTop (rowH);
    area.removeFromTop (10);
    auto row2 = area;

    placeRow (row1, { { "PLAY", { &playMode, &position, &scan } },
                      { "OFFSET", { &rate, &shape, &range, &snap, &length, &probability, &reverse } } },
              { 3.0f, 7.0f });
    placeRow (row2, { { "PITCH + MORPH", { &pitchSet, &pitchAmount, &slotRandom } },
                      { "CLOUD", { &cloud, &grainSize, &density, &spread } },
                      { "AMP", { &attack, &release } },
                      { "COLOR", { &crunch, &wash, &pump } },
                      { "OUT", { &gain } } },
              { 3.0f, 4.0f, 2.0f, 3.0f, 1.2f });
}
