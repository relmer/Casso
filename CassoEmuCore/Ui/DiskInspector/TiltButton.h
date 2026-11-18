#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TiltButton
//
//  The platter's tilt button (FR-151): its icon is a disk seen from above
//  while the platter is flat and a tilted disk while it is tilted, and a
//  click, Space or Enter switches between the two.
//
////////////////////////////////////////////////////////////////////////////////

class TiltButton : public IDxuiControl
{
public:
    using ClickFn = std::function<void()>;
    using StateFn = std::function<bool()>;

    TiltButton() { m_focusable = true; }

    void  SetOnClick  (ClickFn fn) { m_click = std::move (fn); }
    void  SetIsTilted (StateFn fn) { m_isTilted = std::move (fn); }
    bool  IsHovered   () const     { return m_isHover; }
    bool  IsFocused   () const     { return m_isFocused; }

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override { m_isFocused = focused; }
    std::wstring        GetAccessibleName () const override { return L"Tilt"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    ClickFn        m_click;
    StateFn        m_isTilted;
    bool           m_isHover   = false;
    bool           m_isPressed = false;
    bool           m_isFocused = false;
    DxuiDpiScaler  m_scaler;
};
