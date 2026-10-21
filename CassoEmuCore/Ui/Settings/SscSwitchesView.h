#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SscSwitchArt
//
//  The drawing of the card's two DIP switch banks, in pixels, so its
//  geometry can be checked without a renderer. Each bank has a caption above its body;
//  the body has ON printed along its top edge and each switch's number below
//  its slot. A switch's slider sits in the top half of its slot when ON and
//  the bottom half when OFF.
//
////////////////////////////////////////////////////////////////////////////////

struct SscSwitchArtRect
{
    float  x = 0.0f;
    float  y = 0.0f;
    float  w = 0.0f;
    float  h = 0.0f;

    bool  Contains (float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct SscSwitchArtSwitch
{
    SscBank           bank   = SscBank::Sw1;
    int               number = 1;
    SscSwitchArtRect  slot;
    SscSwitchArtRect  slider;
    SscSwitchArtRect  label;
};

struct SscSwitchArtBank
{
    SscBank           bank = SscBank::Sw1;
    SscSwitchArtRect  caption;
    SscSwitchArtRect  body;
    SscSwitchArtRect  onMark;
};

struct SscSwitchArt
{
    std::array<SscSwitchArtBank, 2>  banks;
    std::vector<SscSwitchArtSwitch>  switches;
    float                            cornerRadius  = 0.0f;
    float                            markFontPx    = 0.0f;
    float                            captionFontPx = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SscSwitchesView
//
//  The Super Serial Card's DIP switches as they look on the card: two blue
//  banks of seven, SW1 and SW2, with white sliders. Clicking a switch flips
//  it; the page stages the change, so the settings above follow it.
//
////////////////////////////////////////////////////////////////////////////////

class SscSwitchesView : public IDxuiControl
{
public:
    using ToggleFn = std::function<void (SscBank bank, int number)>;

    // Size in DIPs, for the page's layout.
    static constexpr int  kWidthDip  = 300;
    static constexpr int  kHeightDip = 80;

    void                SetSwitches   (const SscSwitches & switches) { m_switches = switches; }
    void                SetOnToggle   (ToggleFn fn)                   { m_onToggle = std::move (fn); }

    void                Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    std::wstring        GetAccessibleName () const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }

    // The art for a view whose top-left is (left, top), `pxPerDip` pixels to
    // the DIP.
    static SscSwitchArt  BuildArt (float left, float top, float pxPerDip);

    // The switch at (x, y), or false when there is none.
    static bool          HitTest  (const SscSwitchArt & art, float x, float y, SscBank & bank, int & number);

private:
    SscSwitchArt  BuildArt () const;

    DxuiDpiScaler  m_scaler;
    SscSwitches    m_switches = SscSwitches::MakeDefault();
    ToggleFn       m_onToggle;
    int            m_hover    = -1;
};
