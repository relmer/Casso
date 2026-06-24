#pragma once

#include "Pch.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Tooltip
//
//  Pop-up text balloon. The owning widget calls
//  `RequestShow (anchorRect, text)` whenever it detects a hover that
//  should surface explanatory text -- the hardware tree, for
//  instance, uses it to render the lockReason on platform-locked
//  rows. The tooltip auto-hides after a small dwell timeout the
//  caller drives with `Tick (nowMs)`. Single-line by default; call
//  `SetMaxWidthDip` to wrap long text to multiple lines (further
//  capped to the viewport) instead of running off the window edge.
//
////////////////////////////////////////////////////////////////////////////////

class Tooltip
{
public:
    void  SetDwellOpenMs  (int ms) { m_dwellOpenMs = ms; }
    void  SetDwellCloseMs (int ms) { m_dwellCloseMs = ms; }
    void  SetFontSizeDip  (float dip) { m_fontDip = dip; }
    void  SetMaxWidthDip  (float dip) { m_maxWidthDip = dip; }
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetViewportSize (int widthPx, int heightPx) { m_viewportWPx = widthPx; m_viewportHPx = heightPx; }

    void  RequestShow     (const RECT & anchor, const std::wstring & text, int64_t nowMs);
    void  RequestHide     (int64_t nowMs);
    void  DismissAfter    (int64_t nowMs, int delayMs);
    void  Tick            (int64_t nowMs);

    bool                 IsVisible () const { return m_visible; }
    const std::wstring & Text      () const { return m_text;    }
    const RECT         & Anchor    () const { return m_anchor;  }

    void  Paint           (DxUiPainter & painter, DwriteTextRenderer & text) const;

private:
    DpiScaler     m_scaler;
    RECT          m_anchor       = {};
    std::wstring  m_text;
    std::wstring  m_pendingText;
    RECT          m_pendingAnchor = {};
    int64_t       m_showAtMs     = 0;
    int64_t       m_hideAtMs     = 0;
    int           m_dwellOpenMs  = 500;
    int           m_dwellCloseMs = 100;
    float         m_fontDip      = 12.0f;
    float         m_maxWidthDip  = 0.0f;
    int           m_viewportWPx  = 0;
    int           m_viewportHPx  = 0;
    bool          m_visible      = false;
    bool          m_pending      = false;
};
