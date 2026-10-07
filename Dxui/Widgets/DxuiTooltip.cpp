#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTooltip.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Core/DxuiSystemSettings.h"




static constexpr float     s_kPadXDip         = 8.0f;
static constexpr float     s_kPadYDip         = 4.0f;
static constexpr float     s_kBorderDip       = 1.0f;

//
//  Text wider than this wraps onto additional lines instead of growing the
//  balloon past the window edge.
//
static constexpr float     s_kMaxTextWidthDip = 340.0f;

//
//  Fallback glyph metrics used to size the balloon when precise text
//  measurement is unavailable (e.g. test mode, where the popup has no
//  DWrite factory). Deliberately a little generous so text never clips.
//
static constexpr float     s_kEstCharWidthEm  = 0.62f;
static constexpr float     s_kEstLineHeightEm = 1.4f;





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeVisibleMs
//
//  How long a tip stays up: the system's tip lifetime, or long enough to
//  read its text when that is longer, up to kMaxReadMs.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTooltip::ComputeVisibleMs (size_t textLength, int systemMs)
{
    int64_t  readMs = (int64_t) textLength * kReadMsPerChar;



    readMs = (std::min) (readMs, (int64_t) kMaxReadMs);

    return (int) (std::max) ((int64_t) systemMs, readMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasurePointerExtent
//
//  The rows of the current pointer's image that draw anything, measured from
//  its hot spot, as the system's own tips are placed below the pointer's
//  visible bottom rather than below its whole cell. A row draws where its AND
//  mask is clear or, for a monochrome pointer, where its XOR mask inverts. A
//  pointer that cannot be read gives the system's cursor height below the hot
//  spot, which is never short of the image.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltip::PointerExtent DxuiTooltip::MeasurePointerExtent()
{
    constexpr WORD        kBitsPerPixel = 32;
    constexpr uint32_t    kRgbMask      = 0x00FFFFFFu;
    HRESULT               hr            = S_OK;
    PointerExtent         extent        = { 0, GetSystemMetrics (SM_CYCURSOR) };
    CURSORINFO            cursor        = { sizeof (cursor) };
    ICONINFO              icon          = {};
    BITMAP                mask          = {};
    BITMAPINFO            info          = {};
    std::vector<uint32_t> pixels;
    HDC                   screen        = nullptr;
    BOOL                  isRead        = FALSE;
    int                   copied        = 0;
    int                   rows          = 0;
    int                   first         = -1;
    int                   last          = -1;
    bool                  isMono        = false;
    bool                  isDrawn       = false;



    isRead = GetCursorInfo (&cursor);
    CBR (isRead && cursor.hCursor != nullptr);

    isRead = GetIconInfo (cursor.hCursor, &icon);
    CBR (isRead);

    copied = GetObjectW (icon.hbmMask, sizeof (mask), &mask);
    CBR (copied == sizeof (mask) && mask.bmWidth > 0 && mask.bmHeight > 0);

    isMono = icon.hbmColor == nullptr;
    rows   = isMono ? mask.bmHeight / 2 : mask.bmHeight;

    pixels.resize ((size_t) mask.bmWidth * (size_t) mask.bmHeight);

    info.bmiHeader.biSize        = sizeof (info.bmiHeader);
    info.bmiHeader.biWidth       = mask.bmWidth;
    info.bmiHeader.biHeight      = -mask.bmHeight;
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = kBitsPerPixel;
    info.bmiHeader.biCompression = BI_RGB;

    screen = GetDC (nullptr);
    CWR (screen);

    copied = GetDIBits (screen, icon.hbmMask, 0, (UINT) mask.bmHeight, pixels.data(), &info, DIB_RGB_COLORS);
    CBR (copied == mask.bmHeight);

    for (int y = 0; y < rows; y++)
    {
        isDrawn = false;

        for (int x = 0; x < mask.bmWidth && !isDrawn; x++)
        {
            isDrawn = (pixels[(size_t) y * (size_t) mask.bmWidth + (size_t) x] & kRgbMask) == 0 ||
                      (isMono && (pixels[(size_t) (y + rows) * (size_t) mask.bmWidth + (size_t) x] & kRgbMask) != 0);
        }

        if (isDrawn)
        {
            first = (first < 0) ? y : first;
            last  = y;
        }
    }

    CBR (last >= 0);

    extent.aboveHotspotPx = (std::max) (0, (int) icon.yHotspot - first);
    extent.belowHotspotPx = (std::max) (0, last + 1 - (int) icon.yHotspot);

Error:
    if (screen != nullptr)
    {
        ReleaseDC (nullptr, screen);
    }

    if (icon.hbmMask != nullptr)
    {
        DeleteObject (icon.hbmMask);
    }

    if (icon.hbmColor != nullptr)
    {
        DeleteObject (icon.hbmColor);
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePointerClearAnchor
//
//  The pointer can be anywhere inside the anchor, so the tip clears the
//  lowest its image reaches from the anchor's bottom edge, and, placed above,
//  the highest it reaches from the top edge.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTooltip::MakePointerClearAnchor (const RECT & anchor, const PointerExtent & extent, int gapPx)
{
    RECT  clear = anchor;



    clear.top    -= extent.aboveHotspotPx + gapPx;
    clear.bottom += extent.belowHotspotPx + gapPx;

    return clear;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlacementAnchor
//
//  An instant tip follows the pointer, so it is placed clear of the pointer's
//  image, where the anchor alone would put it under the pointer's arrow. A
//  dwelled tip belongs to a control and is placed against the control.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTooltip::GetPlacementAnchor() const
{
    PointerExtent  extent = m_pointerExtent;



    if (!m_isInstant || m_pfnMeasurePointer == nullptr)
    {
        return m_anchor;
    }

    //  The pointer's image is read once when the tip comes up, not on every
    //  move it follows.
    if (!m_hasPointerExtent)
    {
        extent = m_pfnMeasurePointer();
    }

    return MakePointerClearAnchor (m_anchor, extent, m_scaler.ToPx (kPointerGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSystemVisibleMs
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTooltip::GetSystemVisibleMs()
{
    UINT  doubleClickMs = GetDoubleClickTime();



    return (doubleClickMs > 0) ? (int) doubleClickMs * 10 : kMaxVisibleMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestShow
//
//  Queues the tooltip for display after the open dwell timeout. If
//  the tooltip is already up over a different anchor, swap text +
//  anchor instantly.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShow (const RECT & anchor, const std::wstring & text, int64_t nowMs)
{
    m_isInstant = false;

    if (m_visible)
    {
        bool  changed = (text != m_text) ||
                        anchor.left   != m_anchor.left  ||
                        anchor.top    != m_anchor.top   ||
                        anchor.right  != m_anchor.right ||
                        anchor.bottom != m_anchor.bottom;

        m_anchor = anchor;
        m_text   = text;

        // THE DEADLINE SURVIVES A RE-REQUEST for the same tip. Consumers
        // re-issue RequestShow on every mouse-move over the same control, so
        // clearing the hide time here meant a resting pointer wiped it
        // sixty times a second and the tip never dismissed itself -- the
        // lifetime existed and could not once be reached. A move to a
        // DIFFERENT control is a new tip and starts its own clock.
        if (changed && m_popupHost != nullptr)
        {
            m_hideAtMs = nowMs + ComputeVisibleMs (m_text.size(), GetSystemVisibleMs());

            ReleaseActivePopup();
            ShowPopup();
        }

        return;
    }

    m_pendingAnchor = anchor;
    m_pendingText   = text;
    m_pending       = true;
    m_showAtMs      = nowMs + (int64_t) m_dwellOpenMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestShowNow
//
//  Shown on the request itself. A repeat for the same anchor and text keeps
//  the tip and its deadline, as RequestShow's does.
//
//  A TIP ALREADY FOLLOWING THE POINTER IS MOVED, NOT RAISED AGAIN. Hiding the
//  balloon and showing a new one at every cell the pointer crossed made the
//  tip flicker from place to place; the one window that is up moves to the
//  new anchor instead, with its text replaced in place. New text starts its
//  own lifetime; the same text moved along keeps the one it has.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShowNow (const RECT & anchor, const std::wstring & text, int64_t nowMs)
{
    bool  isUp    = m_visible && !m_fadingOut && m_isInstant;
    bool  isSame  = isUp && text == m_text && EqualRect (&anchor, &m_anchor) != FALSE;
    bool  isMoved = isUp && m_activePopup != nullptr;
    bool  isNew   = text != m_text;



    if (isSame)
    {
        return;
    }

    m_anchor    = anchor;
    m_text      = text;
    m_pending   = false;
    m_visible   = true;
    m_fadingOut = false;
    m_isInstant = true;

    if (!isUp || isNew)
    {
        m_hideAtMs = nowMs + ComputeVisibleMs (m_text.size(), GetSystemVisibleMs());
    }

    if (isMoved)
    {
        MovePopup (isNew);
        return;
    }

    m_hasPointerExtent = false;

    if (m_pfnMeasurePointer != nullptr)
    {
        m_pointerExtent    = m_pfnMeasurePointer();
        m_hasPointerExtent = true;
    }

    ReleaseActivePopup();
    ShowPopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowTimed
//
//  Shows the tooltip immediately and schedules an auto-hide durationMs
//  later, for notices that no pointer-leave will dismiss.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowTimed (const RECT & anchor, const std::wstring & text, int64_t nowMs, int durationMs)
{
    bool  changed = !m_visible || text != m_text;



    m_anchor    = anchor;
    m_text      = text;
    m_pending   = false;
    m_visible   = true;
    m_isInstant = false;
    m_hideAtMs  = nowMs + (int64_t) durationMs;

    if (m_popupHost != nullptr && (changed || m_activePopup == nullptr))
    {
        ReleaseActivePopup();
        ShowPopup();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestHide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestHide (int64_t nowMs)
{
    if (m_pending)
    {
        m_pending = false;
    }

    if (m_visible)
    {
        m_hideAtMs = nowMs + (int64_t) m_dwellCloseMs;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Tick (int64_t nowMs)
{
    if (m_pending && nowMs >= m_showAtMs)
    {
        m_anchor   = m_pendingAnchor;
        m_text     = m_pendingText;
        m_visible  = true;
        m_pending  = false;

        // A TOOLTIP HAS A LIFETIME. A hover tip used to set no hide time at
        // all, so it stayed up for as long as the pointer rested -- and a
        // pointer that has been captured, or simply parked, rests forever.
        // The OS dismisses its own after a few seconds for the same reason:
        // the tip has been read by then, and what is left is an obstruction
        // sitting over the thing it was explaining.
        m_hideAtMs = nowMs + ComputeVisibleMs (m_text.size(), GetSystemVisibleMs());

        ShowPopup();

    }

    // Time up: start the fade rather than vanish on the frame. The tip stays
    // `m_visible` until the fade finishes, which is what keeps it rendered
    // and what keeps this loop asking for frames.
    if (m_visible && m_hideAtMs != 0 && nowMs >= m_hideAtMs && !m_fadingOut)
    {
        if (m_activePopup != nullptr && DxuiSystemSettings::Instance().AreMenuAnimationsEnabled())
        {
            m_fadingOut = true;
            m_activePopup->BeginFadeOut (kFadeMs);
        }
        else
        {
            FinishHide();
        }
    }

    // Drive whatever animation is running. AdvanceReveal reports false when
    // nothing is, which is the ordinary case, so the fade-out completion is
    // read only while one was actually started.
    if (m_activePopup != nullptr)
    {
        bool  more = m_activePopup->AdvanceReveal (nowMs);

        if (!more && m_fadingOut)
        {
            FinishHide();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishHide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::FinishHide()
{
    m_visible   = false;
    m_fadingOut = false;
    m_hideAtMs  = 0;
    m_text.clear();

    ReleaseActivePopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HideImmediate
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::HideImmediate()
{
    m_pending  = false;
    m_visible  = false;
    m_hideAtMs = 0;
    m_text.clear();

    ReleaseActivePopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowPopup
//
//  Raises the tooltip balloon in a pooled popup window, sized to its text.
//
//  Three conditions mean "nothing to do" and none is an error: no host, no
//  text, or a balloon ALREADY UP. That last one is why the test cannot be
//  re-derived from m_activePopup further down -- by then this function may
//  have just acquired one, and it would look like the already-up case.
//
//  The DPI is taken from the host at show time, which folds what used to be an
//  explicit SetDpi push from every consumer into this one path.
//
//  Text is measured on the pooled popup's own text renderer BEFORE Show builds
//  the swap chain, so the balloon is created at the right size rather than
//  resized after appearing. When that renderer is unavailable -- test mode has
//  no device -- it falls back to a glyph-count estimate, so placement logic
//  stays testable without a GPU.
//
//  An exhausted pool simply shows nothing. A tooltip is an enhancement, and
//  failing to show one must never disturb what the user is doing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowPopup()
{
    DxuiPopupHost::ShowParams  showParams;
    HWND                       owner    = nullptr;
    HRESULT                    hr       = S_OK;
    bool                       shows    = false;



    // No host, nothing to say, or a balloon ALREADY UP: all three mean there
    // is nothing to do, and none of them is an error. The already-up case is
    // why this cannot be re-derived from m_activePopup further down -- by
    // then this function may have just acquired one.
    shows = (m_popupHost != nullptr && m_activePopup == nullptr && !m_text.empty());

    if (shows)
    {
        // The tooltip's DPI follows its host window, folding what used to be
        // an explicit SetDpi push from the consumer into the show path.
        m_scaler.SetDpi (m_popupHost->GetScaler().GetDpi());

        owner         = m_popupHost->GetHwnd();
        m_activePopup = m_popupHost->AcquirePopup();

        // The pool can be exhausted, leaving no balloon to fill in.
        shows = (m_activePopup != nullptr);
    }

    if (shows)
    {
        showParams.ownerHwnd        = owner;
        showParams.anchorRectScreen = GetScreenAnchor();
        showParams.placement        = DxuiPopupPlacement::Below;
        showParams.flipIfOffscreen  = true;
        showParams.dismiss          = DxuiPopupDismiss::Manual;
        showParams.input            = DxuiPopupInput::PassThrough;
        showParams.shadow           = true;
        showParams.sizeDip          = MeasureBoxDip();
        showParams.backgroundArgb   = m_bgArgb;

        // A tip fades in rather than appearing. Same switch the menus read,
        // so turning menu animation off turns this off with it. An instant
        // tip appears whole, since it moves from cell to cell with the pointer.
        showParams.revealMs         = (DxuiSystemSettings::Instance().AreMenuAnimationsEnabled() && !m_isInstant)
                                          ? kFadeMs : 0;
        showParams.revealFade       = true;
        showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopup (p, t); };
        showParams.onClosed         = [this] () { m_activePopup = nullptr; };

        hr = m_activePopup->Show (std::move (showParams));
        if (FAILED (hr))
        {
            m_popupHost->ReleasePopup (m_activePopup);
            m_activePopup = nullptr;
        }
        else
        {
            m_showCount++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureBoxDip
//
//  The balloon's size for the current text.
//
//  MEASURED IN THE PIXELS IT WILL BE DRAWN IN, not in DIPs. RenderPopup draws
//  at the DPI-scaled font, and a string's width at that size is not its width
//  at 96 DPI scaled up -- hinting rounds each glyph. A balloon sized from the
//  DIP measurement therefore came out a hair narrow, which is all DWrite needs
//  to wrap at the last break opportunity and hide a trailing glyph on a second
//  line the balloon has no room for. "Power-cycle the Apple //e" lost its "e"
//  that way, to the break after the slashes.
//
//  The pooled popup's renderer is bound at 96 DPI, so its logical units ARE
//  pixels and it measures whatever size it is handed. If it is unavailable
//  (test mode) the size falls back to a glyph-count estimate wrapped the same
//  way.
//
//  The popup scales the size back up by the owner DPI, so the trip into DIPs
//  rounds UP -- rounding down would hand back the pixel the measurement exists
//  to keep.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiTooltip::MeasureBoxDip()
{
    HRESULT  hr      = S_OK;
    UINT     dpi     = m_scaler.GetDpi();
    float    fontPx  = m_scaler.ToPxf (m_fontDip);
    float    maxWPx  = m_scaler.ToPxf (s_kMaxTextWidthDip);
    float    padXPx  = m_scaler.ToPxf (s_kPadXDip);
    float    padYPx  = m_scaler.ToPxf (s_kPadYDip);
    float    textWPx = 0.0f;
    float    textHPx = 0.0f;
    SIZE     sizeDip = {};



    dpi = (dpi == 0) ? (UINT) DxuiDpiScaler::kBaseDpi : dpi;
    hr  = (m_activePopup != nullptr)
              ? m_activePopup->MeasureTextWrapped (m_text.c_str(), fontPx, GetFace(), maxWPx, textWPx, textHPx)
              : E_FAIL;

    if (FAILED (hr) || textWPx <= 0.0f)
    {
        float  estWPx   = (float) m_text.size() * fontPx * s_kEstCharWidthEm;
        float  estLines = std::ceil (estWPx / maxWPx);

        textWPx = std::min (estWPx, maxWPx);
        textHPx = std::max (estLines, 1.0f) * fontPx * s_kEstLineHeightEm;
    }

    if (textHPx <= 0.0f)
    {
        textHPx = fontPx * s_kEstLineHeightEm;
    }

    sizeDip.cx = (int) std::ceil ((std::ceil (textWPx) + padXPx * 2.0f) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    sizeDip.cy = (int) std::ceil ((std::ceil (textHPx) + padYPx * 2.0f) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);

    return sizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScreenAnchor
//
//  The placement anchor, which arrives in client pixels, in the screen pixels
//  the popup is placed in.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTooltip::GetScreenAnchor() const
{
    RECT   anchor   = GetPlacementAnchor();
    POINT  topLeft  = { anchor.left,  anchor.top    };
    POINT  botRight = { anchor.right, anchor.bottom };
    HWND   owner    = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;



    if (owner != nullptr)
    {
        ClientToScreen (owner, &topLeft);
        ClientToScreen (owner, &botRight);
    }

    return RECT { topLeft.x, topLeft.y, botRight.x, botRight.y };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MovePopup
//
//  The balloon that is up goes to the current anchor without being hidden:
//  sized again and drawn with the new text before it moves, or, for the same
//  text, moved as it is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::MovePopup (bool isNewText)
{
    HRESULT  hr = S_OK;



    if (m_activePopup == nullptr)
    {
        return;
    }

    if (isNewText)
    {
        hr = m_activePopup->MoveTo (GetScreenAnchor(), MeasureBoxDip());
    }
    else
    {
        hr = m_activePopup->Reposition (GetScreenAnchor());
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseActivePopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ReleaseActivePopup()
{
    DxuiPopupHost *  popup = m_activePopup;



    // Null the pointer first so the popup's onClosed callback (which
    // routes back here) is a no-op and cannot double-release.
    m_activePopup = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Draws the IN-WINDOW tooltip: the fallback used when no popup host is
//  available.
//
//  It exits immediately when a popup IS active, because that balloon renders
//  itself in its own window. Painting both would double-draw the tooltip, once
//  clipped to the client area and once not.
//
//  The box is placed below the anchor and then CLAMPED to the viewport on both
//  axes, so a tooltip near a window edge stays fully visible instead of being
//  clipped away -- which is the whole limitation of the in-window path, and
//  why the popup-hosted version exists.
//
//  Text is measured at paint time rather than cached, since the string changes
//  with whatever is hovered and the measurement is one call for a short label.
//
//  Dimensions are ceiled before padding is added, so a fractional text width
//  cannot round down and clip the final glyph.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr float     s_kAnchorGapDip = 4.0f;



    HRESULT  hr        = S_OK;
    float    fontPx    = m_scaler.ToPxf (m_fontDip);
    float    padX      = m_scaler.ToPxf (s_kPadXDip);
    float    padY      = m_scaler.ToPxf (s_kPadYDip);
    float    borderPx  = m_scaler.ToPxf (s_kBorderDip);
    float    anchorGap = m_scaler.ToPxf (s_kAnchorGapDip);
    float    textW     = 0.0f;
    float    textH     = 0.0f;
    float    width     = 0.0f;
    float    height    = 0.0f;
    float    boxLeft   = 0.0f;
    float    boxTop    = 0.0f;
    RECT     anchor    = {};



    if (!m_visible || m_text.empty() || m_activePopup != nullptr)
    {
        return;
    }

    hr = const_cast<IDxuiTextRenderer &> (text).MeasureStringWrapped (
             m_text.c_str(), fontPx, GetFace(),
             m_scaler.ToPxf (s_kMaxTextWidthDip), textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    anchor  = GetPlacementAnchor();
    width   = std::ceil (textW)  + padX * 2.0f;
    height  = std::ceil (textH)  + padY * 2.0f;
    boxLeft = (float) anchor.left;
    boxTop  = (float) anchor.bottom + anchorGap;

    if (m_viewportWPx > 0)
    {
        float  edgePad = m_scaler.ToPxf (s_kAnchorGapDip);

        if (boxLeft + width > (float) m_viewportWPx - edgePad)
        {
            boxLeft = (float) m_viewportWPx - edgePad - width;
        }

        if (boxLeft < edgePad)
        {
            boxLeft = edgePad;
        }
    }

    if (m_viewportHPx > 0)
    {
        float  flippedTop = (float) anchor.top - anchorGap - height;

        if (boxTop + height > (float) m_viewportHPx && flippedTop >= 0.0f)
        {
            boxTop = flippedTop;
        }
    }

    painter.FillRect    (boxLeft, boxTop, width, height, m_bgArgb);
    painter.OutlineRect (boxLeft, boxTop, width, height, borderPx, m_borderArgb);

    hr = text.DrawString (m_text.c_str(),
                          boxLeft + padX,
                          boxTop  + padY,
                          width  - padX * 2.0f,
                          height - padY * 2.0f,
                          m_textArgb,
                          fontPx,
                          GetFace());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltip::Layout  (IDxuiControl override)
//
//  The popup geometry is driven by RequestShow / Paint anchor-based
//  placement; the override just records bounds and DPI for the panel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltip::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    SetTheme (theme);
    static_cast<const DxuiTooltip *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPopup
//
//  Popup-host render hook. The host has already cleared the back buffer
//  to s_kBgArgb, so this only draws the border (painter / D3D, under the
//  text) and the text (D2D, composited on top). Coordinates are popup-
//  local pixels with the origin at the balloon's top-left.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RenderPopup (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT  hr       = S_OK;
    RECT     placed   = {};
    float    width    = 0.0f;
    float    height   = 0.0f;
    float    padX     = m_scaler.ToPxf (s_kPadXDip);
    float    padY     = m_scaler.ToPxf (s_kPadYDip);
    float    borderPx = m_scaler.ToPxf (s_kBorderDip);
    float    fontPx   = m_scaler.ToPxf (m_fontDip);



    if (m_activePopup == nullptr)
    {
        return;
    }

    placed = m_activePopup->GetPlacedRectScreenPx();
    width  = (float) (placed.right  - placed.left);
    height = (float) (placed.bottom - placed.top);

    painter.OutlineRoundedRect (0.0f, 0.0f, width, height, m_scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip), borderPx, m_borderArgb);

    hr = text.DrawString (m_text.c_str(),
                          padX,
                          padY,
                          width  - padX * 2.0f,
                          height - padY * 2.0f,
                          m_textArgb,
                          fontPx,
                          GetFace());
    IGNORE_RETURN_VALUE (hr, S_OK);
}
