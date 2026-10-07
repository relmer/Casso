#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiTooltip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip
//
//  A small "i" in a ring beside a setting, whose tooltip shows what the
//  setting's own label has no room for. It is a tab stop: Enter, Space or a
//  click opens the tip at once, as hovering does after a dwell. It tracks
//  the pointer without consuming any move, so it can sit beside a control
//  without taking input from it.
//
//  The tip shows through a popup host when one is set, as a dropdown's menu
//  does, so it can extend past the page it sits on.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiInfoTip : public IDxuiControl
{
public:
    DxuiInfoTip  () { SetFocusable (true); }
    ~DxuiInfoTip () override = default;

    void  SetText      (const std::wstring & text) { m_text = text; }
    void  SetRect      (const RECT & rect)         { SetBounds (rect); }
    void  SetDpi       (UINT dpi)                  { m_scaler.SetDpi (dpi); m_tooltip.SetDpi (dpi); }
    void  SetPopupHost (DxuiHwndSource * host)     { m_tooltip.SetPopupHost (host); }

    const std::wstring & GetText () const { return m_text; }
    const DxuiTooltip  & GetTooltip () const { return m_tooltip; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse        (const DxuiMouseEvent & ev) override;
    bool  OnKey          (const DxuiKeyEvent & ev) override;
    void  OnFocusChanged (bool focused) override;
    void  Tick           (int64_t nowMs) override { m_tooltip.Tick (nowMs); }

    std::wstring        GetAccessibleName () const override { return m_text; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

    static constexpr const wchar_t *  kGlyph     = L"\xE946";              // Segoe MDL2 Info: "i" in a ring
    static constexpr const wchar_t *  kGlyphFace = L"Segoe MDL2 Assets";
    static constexpr float            kGlyphDip  = 14.0f;

private:
    static int64_t  GetNowMs ();

    void  Open ();

    std::wstring   m_text;
    DxuiTooltip    m_tooltip;
    DxuiDpiScaler  m_scaler;
    bool           m_isHovered = false;
    bool           m_isFocused = false;
};
