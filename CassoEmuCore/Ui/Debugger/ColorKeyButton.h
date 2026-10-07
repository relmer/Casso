#pragma once

#include "Pch.h"

#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ColorKeyButton
//
//  The info button at the trailing end of a pane's toolbar, which shows what
//  the pane's colors mean. It draws the strip it sits on, so it reads as the
//  toolbar's last entry, and takes the keyboard focus, so Tab reaches it and
//  Space or Enter opens the key; the pointer resting on it opens the key too.
//  What opening it does is the window's (ColorKeyPopup).
//
////////////////////////////////////////////////////////////////////////////////

class ColorKeyButton : public IDxuiControl
{
public:
    using ActivateFn = std::function<void ()>;
    using FocusFn    = std::function<void (bool focused)>;

    //  As wide as the compact strip is tall, so the button is square.
    static constexpr int  kWidthDip = DxuiToolbar::kCompactBandDp;

    static constexpr const wchar_t * kpszName = L"Colors";

    ColorKeyButton (std::wstring pane, ColorLegend::Pane legend);

    const std::wstring &  GetPane   () const { return m_pane; }
    ColorLegend::Pane     GetLegend () const { return m_legend; }
    void                  SetLegend (ColorLegend::Pane legend) { m_legend = legend; }

    void  SetOnActivate (ActivateFn fn) { m_onActivate = std::move (fn); }
    void  SetOnFocus    (FocusFn fn)    { m_onFocus    = std::move (fn); }

    void  SetHovered    (bool hovered)  { m_isHovered = hovered; }
    bool  IsHovered     () const        { return m_isHovered; }
    void  SetPressed    (bool pressed)  { m_isPressed = pressed; }
    bool  IsFocused     () const        { return m_isFocused; }

    bool  Contains      (POINT point) const;

    //  The strip a pane's toolbar takes beside the button: the slot less the
    //  button's width at its trailing end.
    static RECT  GetStripBeside (const RECT & slot, const DxuiDpiScaler & scaler);

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override;
    std::wstring        GetAccessibleName () const override { return kpszName; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Button; }

private:
    std::wstring       m_pane;
    ColorLegend::Pane  m_legend     = ColorLegend::Pane::Disassembly;
    DxuiDpiScaler      m_scaler;
    ActivateFn         m_onActivate;
    FocusFn            m_onFocus;
    bool               m_isHovered  = false;
    bool               m_isPressed  = false;
    bool               m_isFocused  = false;
};
