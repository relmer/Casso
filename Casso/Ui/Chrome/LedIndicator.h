#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "Core/IDxuiControl.h"





enum class LedState
{
    Idle,
    Present,
    Active,
};


struct LedIndicatorLayout
{
    RECT  haloRect = {};
    RECT  coreRect = {};
};


//
//  LedIndicator is Casso-specific (Apple ][ disk LED + joystick-mode
//  indicator); its theme-driven paint path assumes the IDxuiTheme
//  reference it receives is actually a ChromeTheme and `static_cast`s
//  to read drive-LED palette fields. A debug `dynamic_cast` guard in
//  Paint pins the contract.
//
class LedIndicator : public IDxuiControl
{
public:
    LedIndicator  ();
    ~LedIndicator () override = default;

    using IDxuiControl::Layout;

    void               Layout       (int x, int y, UINT dpi);
    void               SetState     (LedState state) { m_state = state; }
    LedState           GetState     () const { return m_state; }
    LedIndicatorLayout GetLayout    () const { return m_layout; }
    uint32_t           CoreArgb     (const ChromeTheme & theme) const;
    uint32_t           HaloArgb     (const ChromeTheme & theme) const;
    void               Paint        (IDxuiPainter        & painter,
                                     IDxuiTextRenderer   & text,
                                     const IDxuiTheme    & theme) override;
    void               Paint        (IDxuiPainter & painter, uint32_t coreArgb, uint32_t haloArgb) const;

    void               Layout       (const RECT          & boundsDip,
                                     const DxuiDpiScaler & scaler) override;

private:
    LedState            m_state  = LedState::Idle;
    LedIndicatorLayout  m_layout = {};
};
