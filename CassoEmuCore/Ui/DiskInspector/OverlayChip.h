#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip
//
//  A compact check box and label for one of the platter's overlays (FR-031).
//  A click, Space or Enter turns it over.
//
////////////////////////////////////////////////////////////////////////////////

class OverlayChip : public IDxuiControl
{
public:
    using ChangeFn = std::function<void (bool isOn)>;

    static constexpr float  kTextDip     = 12.0f;
    static constexpr float  kPadDip      = 5.0f;
    static constexpr float  kCheckDip    = 13.0f;
    static constexpr float  kCheckGapDip = 5.0f;
    static constexpr LPCWSTR  kpszCheck     = L"\xE73E";
    static constexpr LPCWSTR  kpszCheckFace = L"Segoe MDL2 Assets";

    explicit OverlayChip (const std::wstring & label);

    void  SetOn       (bool isOn) { m_isOn = isOn; }
    bool  IsOn        () const    { return m_isOn; }
    void  SetOnChange (ChangeFn fn) { m_change = std::move (fn); }

    const std::wstring & GetLabel () const { return m_label; }

    int   GetWidthPx (IDxuiTextRenderer & text, const DxuiDpiScaler & scaler) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    void                OnFocusChanged    (bool focused) override { m_isFocused = focused; }
    std::wstring        GetAccessibleName () const override { return m_label; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Checkbox; }

private:
    void  Flip ();

    std::wstring   m_label;
    ChangeFn       m_change;
    bool           m_isOn      = false;
    bool           m_isHover   = false;
    bool           m_isPressed = false;
    bool           m_isFocused = false;
    DxuiDpiScaler  m_scaler;
};
