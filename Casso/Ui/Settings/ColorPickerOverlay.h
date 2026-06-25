#pragma once

#include "Pch.h"

#include "../ColorUtil.h"
#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Button.h"
#include "../Widgets/Label.h"
#include "../Widgets/Slider.h"
#include "../Widgets/TextInput.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ColorPickerOverlay
//
//  Modal HSV color picker hosted inside the Settings panel. When open it
//  dims the panel and centers a themed dialog with Hue / Saturation /
//  Value sliders, a hex entry, a live preview swatch, and OK / Cancel.
//
//  Edits fire OnChange continuously so the host can preview the color live
//  on the emulator; OnClose fires once with accepted = true (OK / Enter) or
//  false (Cancel / Esc), carrying the final color. The host routes input
//  and paint only while IsOpen() is true.
//
////////////////////////////////////////////////////////////////////////////////

class ColorPickerOverlay
{
public:
    using ChangeFn = std::function<void (uint32_t argb)>;
    using CloseFn  = std::function<void (bool accepted, uint32_t argb)>;

    void  SetTheme    (const ChromeTheme * theme) { m_theme = theme; }
    void  SetHwnd     (HWND hwnd)                 { m_hwnd = hwnd; m_hex.SetHwnd (hwnd); }
    void  SetOnChange (ChangeFn fn)               { m_onChange = std::move (fn); }
    void  SetOnClose  (CloseFn fn)                { m_onClose  = std::move (fn); }

    void  Open        (uint32_t initialArgb);
    void  Close       ()       { m_open = false; }
    bool  IsOpen      () const { return m_open; }

    uint32_t Argb     () const { return m_argb; }

    void  Layout         (const RECT & panelRect, const DpiScaler & scaler);
    void  OnLButtonDown  (int x, int y);
    void  OnLButtonUp    (int x, int y);
    void  OnMouseMove    (int x, int y);
    void  OnMouseHover   (int x, int y);
    bool  OnKey          (WPARAM vk);
    bool  OnChar         (wchar_t ch);
    void  Paint          (DxUiPainter & painter, DwriteTextRenderer & text);

    // Test accessors.
    const RECT & DialogRect () const { return m_dialogRect; }

private:
    // Recompute the packed color from the HSV sliders, refresh the hex box
    // and preview, and fire OnChange.
    void  SyncFromHsv     ();
    // Parse the hex box; on success update the HSV sliders + preview and
    // fire OnChange. No-op on malformed text so partial typing is tolerated.
    void  SyncFromHex     ();
    void  Accept          ();
    void  Cancel          ();
    void  MoveFocus       (int delta);
    void  ApplyFocus      ();

    // Copy-to-clipboard icon next to the hex box.
    void  PaintCopyIcon   (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme);
    bool  CopyHit         (int x, int y) const;

    const ChromeTheme  * m_theme    = nullptr;
    HWND                 m_hwnd     = nullptr;
    ChangeFn             m_onChange;
    CloseFn              m_onClose;

    Slider     m_hue;
    Slider     m_sat;
    Slider     m_val;
    TextInput  m_hex;
    Button     m_ok;
    Button     m_cancel;
    Label      m_title;
    Label      m_hueLabel;
    Label      m_satLabel;
    Label      m_valLabel;
    Label      m_hexLabel;

    RECT       m_panelRect   = {};
    RECT       m_dialogRect  = {};
    RECT       m_previewRect = {};
    RECT       m_copyRect    = {};       // copy-to-clipboard icon next to the hex box
    bool       m_copyHover   = false;
    int64_t    m_copyFlashMs = 0;        // GetTickCount64 at last copy, for the "copied" flash

    DpiScaler  m_scaler;
    float      m_h           = 0.0f;     // 0..360
    float      m_s           = 0.0f;     // 0..1
    float      m_v           = 1.0f;     // 0..1
    uint32_t   m_argb        = ColorUtil::kWhiteArgb;
    uint32_t   m_originalArgb = ColorUtil::kWhiteArgb;
    int        m_focusIndex  = 0;        // 0=hue 1=sat 2=val 3=hex 4=ok 5=cancel
    int        m_prevFocusIndex = -1;    // for hex select-all-on-focus edge detection
    bool       m_open        = false;
    bool       m_syncing     = false;    // re-entrancy guard for hex/slider sync
};
