#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace prisma
{
struct Colours
{
    static juce::Colour bg()     { return juce::Colour (0xff17171a); }
    static juce::Colour panel()  { return juce::Colour (0xff202024); }
    static juce::Colour line()   { return juce::Colour (0xff303036); }
    static juce::Colour text()   { return juce::Colour (0xffe6e4df); }
    static juce::Colour dim()    { return juce::Colour (0xff9a988f); }
    static juce::Colour accent() { return juce::Colour (0xffe8c39e); } // warm sand, the only accent
    static juce::Colour wave()   { return juce::Colour (0xff8d8a83); }
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
class SlotView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    SlotView (PrismaProcessor&, int index, std::function<void (int)> onSelect, std::function<void (const juce::String&)> onStatus);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void setSelected (bool s) { selected = s; repaint(); }
    void refresh();

private:
    void load (const juce::File&);
    PrismaProcessor& proc;
    int index;
    bool selected = false, dragOver = false;
    juce::TextButton loadButton { "Load" }, clearButton { "x" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void (int)> select;
    std::function<void (const juce::String&)> status;
    const void* shown = nullptr;
    bool initialised = false;
};

//==============================================================================
class WaveView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WaveView (PrismaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setFocusSlot (int s) { focus = s; repaint(); }
    void tick();

private:
    void setPositionFrom (const juce::MouseEvent&);
    PrismaProcessor& proc;
    int focus = 0, readIndex = 0;
    struct Mark { float pos; int slot; float life; };
    std::vector<Mark> marks;
};

//==============================================================================
class XYPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit XYPad (PrismaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (18.0f); }
    void setFrom (const juce::MouseEvent&);
    PrismaProcessor& proc;
};

//==============================================================================
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct Choice : public juce::Component
{
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label);
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

} // namespace prisma

//==============================================================================
class PrismaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismaEditor (PrismaProcessor&);
    ~PrismaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectSlot (int);
    void setStatus (const juce::String&);

    struct Group { juce::String title; juce::Rectangle<int> bounds; };

    PrismaProcessor& proc;
    prisma::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    juce::ComboBox presetBox;
    juce::TextButton diceButton { "Dice" }, copyButton { "Copy settings" }, pasteButton { "Paste settings" };

    std::vector<std::unique_ptr<prisma::SlotView>> slotViews;
    prisma::WaveView wave;
    prisma::XYPad xy;

    prisma::Choice playMode, rate, shape, pitchSet;
    prisma::Knob position, scan, range, snap, length, probability, reverse;
    prisma::Knob pitchAmount, cloud, grainSize, density, spread, attack, release, crunch, wash, pump, gain;
    juce::ToggleButton slotRandom { "Slot random" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slotRandomAtt;

    std::vector<Group> groups;
    juce::String status;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismaEditor)
};
