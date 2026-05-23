#include "Pch.h"

#include "TitleBar.h"






namespace
{
    constexpr int  s_kBaseTitleHeightPx = 32;
    constexpr int  s_kBaseButtonWidthPx = 46;
    constexpr int  s_kBaseDpi           = 96;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::Compute
//
////////////////////////////////////////////////////////////////////////////////

TitleBarLayoutOutput TitleBarLayout::Compute (const TitleBarLayoutInput & in)
{
    TitleBarLayoutOutput  out          = {};
    int                   buttonStripW = in.buttonWidth * 3;
    int                   buttonsLeft  = in.clientWidth - buttonStripW;
    bool                  collapse     = (buttonsLeft < 0) || (in.titleHeight <= 0) || (in.buttonWidth <= 0);



    out.titleBar.left    = 0;
    out.titleBar.top     = 0;
    out.titleBar.right   = in.clientWidth;
    out.titleBar.bottom  = in.titleHeight;

    if (collapse)
    {
        out.dragRegion.left   = 0;
        out.dragRegion.top    = 0;
        out.dragRegion.right  = in.clientWidth;
        out.dragRegion.bottom = in.titleHeight;
        return out;
    }

    out.minButton.left   = buttonsLeft;
    out.minButton.top    = 0;
    out.minButton.right  = buttonsLeft + in.buttonWidth;
    out.minButton.bottom = in.titleHeight;

    out.maxButton.left   = out.minButton.right;
    out.maxButton.top    = 0;
    out.maxButton.right  = out.maxButton.left + in.buttonWidth;
    out.maxButton.bottom = in.titleHeight;

    out.closeButton.left   = out.maxButton.right;
    out.closeButton.top    = 0;
    out.closeButton.right  = out.closeButton.left + in.buttonWidth;
    out.closeButton.bottom = in.titleHeight;

    out.dragRegion.left   = 0;
    out.dragRegion.top    = 0;
    out.dragRegion.right  = buttonsLeft;
    out.dragRegion.bottom = in.titleHeight;

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultTitleHeight
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::DefaultTitleHeight (UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;

    return MulDiv (s_kBaseTitleHeightPx, static_cast<int> (effectiveDpi), s_kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultButtonWidth
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::DefaultButtonWidth (UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;

    return MulDiv (s_kBaseButtonWidthPx, static_cast<int> (effectiveDpi), s_kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::WindowsUiFontFamily
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * TitleBarLayout::WindowsUiFontFamily ()
{
    return L"Segoe UI";
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::WindowsUiFontWeight
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::WindowsUiFontWeight ()
{
    return FW_NORMAL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::TitleBar ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::~TitleBar ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  No-op until the native painter pass takes over chrome rendering.
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Show ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
//  No-op counterpart to Show().
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Hide ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateGeometry
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::UpdateGeometry (int clientWidth, UINT dpi)
{
    TitleBarLayoutInput  in = {};

    in.clientWidth = clientWidth;
    in.titleHeight = TitleBarLayout::DefaultTitleHeight (dpi);
    in.buttonWidth = TitleBarLayout::DefaultButtonWidth (dpi);

    m_layout = TitleBarLayout::Compute (in);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRect
//
////////////////////////////////////////////////////////////////////////////////

RECT TitleBar::GetButtonRect (SystemButton which) const
{
    switch (which)
    {
    case SystemButton::Minimize: return m_layout.minButton;
    case SystemButton::Maximize: return m_layout.maxButton;
    case SystemButton::Close:    return m_layout.closeButton;
    }

    return RECT { 0, 0, 0, 0 };
}
