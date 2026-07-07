#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"


class DxuiHwndSource;
class DxuiPopupHost;






////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltip
//
//  Single-line pop-up text balloon. The owning widget calls
//  `RequestShow (anchorRect, text)` whenever it detects a hover that
//  should surface explanatory text -- the hardware tree, for
//  instance, uses it to render the lockReason on platform-locked
//  rows. The tooltip auto-hides after a small dwell timeout the
//  caller drives with `Tick (nowMs)`.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTooltip : public IDxuiControl
{
public:
    ~DxuiTooltip() override = default;

    void  SetDwellOpenMs  (int ms) { m_dwellOpenMs = ms; }
    void  SetDwellCloseMs (int ms) { m_dwellCloseMs = ms; }
    void  SetFontSizeDip  (float dip) { m_fontDip = dip; }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetViewportSize (int widthPx, int heightPx) { m_viewportWPx = widthPx; m_viewportHPx = heightPx; }
    void  SetTheme        (const IDxuiTheme & theme)  { m_bgArgb = theme.TooltipBackground(); m_borderArgb = theme.TooltipBorder(); m_textArgb = theme.TooltipForeground(); }

    //
    //  Opt-in popup hosting (FR-054 / FR-061). When a host is wired
    //  up the tooltip renders into a WS_POPUP HWND with
    //  WS_EX_TRANSPARENT | WS_EX_LAYERED so pointer events pass
    //  through to whatever is underneath; dismiss is OnPointerLeave.
    //
    void  SetPopupHost    (DxuiHwndSource * host) { m_popupHost = host; }
    DxuiHwndSource *  PopupHost   () const { return m_popupHost;   }
    DxuiPopupHost  *  ActivePopup () const { return m_activePopup; }

    void  RequestShow     (const RECT & anchor, const std::wstring & text, int64_t nowMs);
    void  RequestHide     (int64_t nowMs);

    // Shows immediately (no open dwell) and auto-hides after durationMs.
    // For transient notices where no pointer-leave will arrive to dismiss
    // it -- e.g. entering paddle mode captures the mouse, so the hover that
    // would normally hide the tooltip never fires.
    void  ShowTimed       (const RECT & anchor, const std::wstring & text, int64_t nowMs, int durationMs);
    void  Tick            (int64_t nowMs) override;

    //
    //  Synchronously tear down any live popup and reset the dwell
    //  state. Called by the owner before its popup host (and pool) is
    //  destroyed, since the timed RequestHide path would release the
    //  popup too late.
    //
    void  HideImmediate   ();

    bool                 IsVisible () const { return m_visible; }
    const std::wstring & Text      () const { return m_text;    }
    const RECT         & Anchor    () const { return m_anchor;  }

    void  Paint           (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    //
    //  IDxuiControl overrides — additive shims so DxuiTooltip can
    //  appear in a DxuiPanel tree. Typical hosting is via
    //  DxuiPopupHost (WS_POPUP transparent overlay).
    //
    void                Layout         (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring        AccessibleName () const override { return m_text; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    //
    //  Acquire + size + show the popup balloon for the current
    //  anchor/text. No-op without a wired host or when a popup is
    //  already up. Releases the popup on Show failure.
    //
    void  ShowPopup          ();

    //
    //  Return the live popup to the host pool (hiding its HWND) and
    //  drop the pointer. Safe to call when none is active.
    //
    void  ReleaseActivePopup ();

    //
    //  Render hook invoked by the popup host (popup-local pixels,
    //  origin top-left). Draws the balloon border + text over the
    //  host's opaque background clear.
    //
    void  RenderPopup        (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    DxuiDpiScaler     m_scaler;
    RECT          m_anchor       = {};
    std::wstring  m_text;
    std::wstring  m_pendingText;
    RECT          m_pendingAnchor = {};
    int64_t       m_showAtMs     = 0;
    int64_t       m_hideAtMs     = 0;
    int           m_dwellOpenMs  = 500;
    int           m_dwellCloseMs = 100;
    float         m_fontDip      = 12.0f;
    uint32_t      m_bgArgb       = 0xFF2D2D2D;
    uint32_t      m_borderArgb   = 0xFF606060;
    uint32_t      m_textArgb     = 0xFFE8EEF4;
    int           m_viewportWPx  = 0;
    int           m_viewportHPx  = 0;
    bool          m_visible      = false;
    bool          m_pending      = false;
    DxuiHwndSource  *  m_popupHost     = nullptr;
    DxuiPopupHost   *  m_activePopup   = nullptr;
};
