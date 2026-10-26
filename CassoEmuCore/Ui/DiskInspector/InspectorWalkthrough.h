#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WalkthroughArea / WalkthroughButton
//
//  The part of the window a walkthrough step is about, None for the steps
//  on disk facts, and the card's buttons.
//
////////////////////////////////////////////////////////////////////////////////

enum class WalkthroughArea
{
    Platter,
    Toolbar,
    Track,
    Tabs,
    Strip,
    None,
};


enum class WalkthroughButton
{
    None,
    Back,
    Next,
    Close,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWalkthrough
//
//  The inspector's walkthrough (FR-153): a step for each area of the window,
//  then steps on basic disk facts, on a card over the window with the
//  rest of the window dimmed around the step's area. Back and Next step
//  through it, and Close or Next on the last step closes it. Drawn with the
//  text renderer's FillRect and DrawString, which draw in order with the
//  views' text, so the dimming covers that text too.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorWalkthrough
{
public:
    struct Step
    {
        WalkthroughArea  area  = WalkthroughArea::None;
        LPCWSTR          title = L"";
        LPCWSTR          body  = L"";
    };

    static constexpr float  kCardWidthDip = 420.0f;

    static std::span<const Step>  GetSteps ();

    void             Open    ();
    void             Close   () { m_isOpen = false; }
    void             Back    ();
    void             Next    ();
    bool             IsOpen  () const { return m_isOpen; }
    int              GetStep () const { return m_step; }
    WalkthroughArea  GetArea () const;

    //  The card and the dimming around `areaPx`, which is empty for a step
    //  on disk facts. HitTest uses the buttons' rectangles from the last Paint.
    void               Paint   (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, const RECT & windowPx, const RECT & areaPx);
    WalkthroughButton  HitTest (POINT pointPx) const;

    //  Where the card goes: beside the area where it fits, in the order
    //  right, left, below and above, else in the middle of the window.
    static RECT  PlaceCard (const RECT & windowPx, const RECT & areaPx, int widthPx, int heightPx, int gapPx);

private:
    bool  m_isOpen  = false;
    int   m_step    = 0;
    RECT  m_backPx  = {};
    RECT  m_nextPx  = {};
    RECT  m_closePx = {};
};
