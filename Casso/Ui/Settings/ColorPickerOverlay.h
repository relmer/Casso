#pragma once

#include "Pch.h"

#include "../ColorUtil.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiTextInput.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay
//
//  Modal HSV color picker hosted inside the Settings panel. When open it
//  centers a themed dialog with Hue / Saturation / Value sliders, a hex entry,
//  a live preview swatch, and OK / Cancel.
//
////////////////////////////////////////////////////////////////////////////////

class ColorPickerOverlay
{
public:
    using ChangeFn = std::function<void (uint32_t argb)>;
    using CloseFn  = std::function<void (bool accepted, uint32_t argb)>;

    void      SetHwnd     (HWND hwnd)       { m_hwnd = hwnd; m_hex.SetHwnd (hwnd); }
    void      SetOnChange (ChangeFn fn)     { m_onChange = std::move (fn); }
    void      SetOnClose  (CloseFn fn)      { m_onClose  = std::move (fn); }

    void      Open        (uint32_t initialArgb);
    void      Close()                 { m_open = false; }
    bool      IsOpen() const          { return m_open; }
    uint32_t  Argb() const            { return m_argb; }

    void      Layout        (const RECT & panelRect, const DxuiDpiScaler & scaler);
    void      OnLButtonDown (int x, int y);
    void      OnLButtonUp   (int x, int y);
    void      OnMouseMove   (int x, int y);
    void      OnMouseHover  (int x, int y);
    bool      OnKey         (WPARAM vk);
    bool      OnChar        (wchar_t ch);
    void      Paint         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);

    const RECT & DialogRect() const { return m_dialogRect; }

private:
    void  SyncFromHsv();
    void  SyncFromHex();
    void  Accept();
    void  Cancel();
    void  MoveFocus          (int delta);
    void  ApplyFocus();
    void  CopyTextToClipboard (const std::wstring & text);
    void  PaintCopyIcon      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  CopyHit            (int x, int y) const;

    static RECT MakeRect     (int l, int t, int w, int h);

    HWND                 m_hwnd             = nullptr;
    ChangeFn             m_onChange;
    CloseFn              m_onClose;

    DxuiSlider           m_hue;
    DxuiSlider           m_sat;
    DxuiSlider           m_val;
    DxuiTextInput        m_hex;
    DxuiButton           m_ok;
    DxuiButton           m_cancel;
    DxuiLabel            m_title;
    DxuiLabel            m_hueLabel;
    DxuiLabel            m_satLabel;
    DxuiLabel            m_valLabel;
    DxuiLabel            m_hexLabel;

    RECT                 m_panelRect        = {};
    RECT                 m_dialogRect       = {};
    RECT                 m_previewRect      = {};
    RECT                 m_copyRect         = {};
    bool                 m_copyHover        = false;
    int64_t              m_copyFlashMs      = 0;

    DxuiDpiScaler        m_scaler;
    float                m_h                = 0.0f;
    float                m_s                = 0.0f;
    float                m_v                = 1.0f;
    uint32_t             m_argb             = ColorUtil::kWhiteArgb;
    uint32_t             m_originalArgb     = ColorUtil::kWhiteArgb;
    int                  m_focusIndex       = 0;
    int                  m_prevFocusIndex   = -1;
    bool                 m_open             = false;
    bool                 m_syncing          = false;
    bool                 m_hexReplaceOnChar = false;
};
