#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiDpiScaler.h"
#include "Theme/IDxuiTheme.h"


class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText
//
//  Read-only text, wrapped and aligned in its box as a label draws it, that
//  can be selected with the mouse and copied: a drag selects, a double-click
//  takes a word, Ctrl+A everything, and Ctrl+C or the host's Copy copies.
//
//  WHERE A CHARACTER IS COMES FROM THE RENDERER, which lays the text out the
//  way it draws it, so a selection lines up with the glyphs under it whatever
//  the wrapping and alignment.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSelectableText : public IDxuiControl
{
public:
    void  SetText         (const std::wstring & text) { m_text = text; m_anchor = m_caret = 0; m_dragging = false; }
    void  SetTextRole     (DxuiTextRole role)         { m_role = role; }
    void  SetAlign        (DxuiTextHAlign h, DxuiTextVAlign v) { m_hAlign = h; m_vAlign = v; }
    void  SetTextRenderer (IDxuiTextRenderer * text)  { m_renderer = text; }
    void  SetOwnerWindow  (HWND hwnd)                 { m_hwnd = hwnd; }

    const std::wstring &  GetText         () const { return m_text; }
    std::wstring          GetSelectedText () const;
    bool                  HasSelection    () const { return m_anchor != m_caret; }
    bool                  IsInteracting   () const { return m_dragging; }

    void  SelectAll ()  { m_anchor = 0; m_caret = m_text.size(); }
    void  Copy      () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override;
    bool                InvokeCommand     (DxuiStandardCommand command) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    bool    HitTest    (POINT clientPx, size_t & outIndex) const;
    void    SelectWord (size_t index);

    static bool  IsWordChar (wchar_t c) { return iswalnum (c) || c == L'_' || c == L'.' || c == L','; }

    std::wstring          m_text;
    DxuiTextRole          m_role      = DxuiTextRole::Body;
    DxuiTextHAlign        m_hAlign    = DxuiTextHAlign::Left;
    DxuiTextVAlign        m_vAlign    = DxuiTextVAlign::Top;
    IDxuiTextRenderer   * m_renderer  = nullptr;
    HWND                  m_hwnd      = nullptr;
    DxuiDpiScaler         m_scaler;
    size_t                m_anchor    = 0;
    size_t                m_caret     = 0;
    bool                  m_dragging  = false;
    ULONGLONG             m_lastClick = 0;

    //  The font as the theme gave it at the last paint, so a click is tested
    //  against the layout drawn.
    std::wstring          m_face      = L"Segoe UI";
    float                 m_fontPx    = 0.0f;
};
