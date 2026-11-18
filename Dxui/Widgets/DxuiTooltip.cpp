#include "Pch.h"
#include "Theme/DxuiTheme.h"
#include "Theme/DxuiColor.h"

#include "DxuiTooltip.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"
#include "Core/DxuiPaneMetrics.h"
#include "Core/DxuiSystemSettings.h"




static constexpr float     s_kPadXDip            = 8.0f;
static constexpr float     s_kPadYDip            = 4.0f;
static constexpr float     s_kBorderDip          = 1.0f;
static constexpr int       s_kPointerGapDip      = 26;   // above the pointer, as Explorer places its tips
static constexpr int       s_kPointerBelowGapDip = 4;    // below the pointer's arrow, when there is no room above





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
//  ApplyVisualStudioLook
//
//  Visual Studio 2026's tip, measured at 125% on 2026-10-08: corners of
//  8 pixels, 6.4 DIP; a one-pixel border with the 8- and 4-DIP padding
//  inside it, which makes a one-line tip 32 pixels tall; and a shadow that
//  reaches only a few pixels, 1 above, 3 to the sides and 6 below. Under
//  the bottom edge it is 14% black on a dark theme, fitted at 150%, and
//  about half that on a light one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ApplyVisualStudioLook()
{
    constexpr float     kCornerDip       = 6.4f;
    constexpr float     kShadowBlurDip   = 4.0f;
    constexpr float     kShadowOffsetDip = 2.0f;
    constexpr float     kShadowInsetDip  = 1.0f;
    constexpr float     kShadowOnDark    = 0.14f;
    constexpr float     kShadowOnLight   = 0.075f;
    DxuiShadow::Style   onDark           = { kShadowBlurDip, kShadowOffsetDip, kShadowInsetDip, kShadowOnDark  };
    DxuiShadow::Style   onLight          = { kShadowBlurDip, kShadowOffsetDip, kShadowInsetDip, kShadowOnLight };



    SetCornerRadiusDip (kCornerDip);
    SetWholePixelFrame (true);
    SetShadow          (onDark, onLight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBorderPx
//
//  A whole-pixel frame's border is a pane's outline: one DIP rounded to whole
//  pixels, never less than one. The default is one DIP as it falls.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltip::GetBorderPx() const
{
    return m_isWholePixelFrame ? (float) DxuiPaneMetrics::GetLinePx (m_scaler) : m_scaler.ToPxf (s_kBorderDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPadXPx
//
//  From the balloon's left edge to its text. A whole-pixel frame measures
//  the padding inside its border, in whole pixels, so the text starts on a
//  pixel; the default draws its border inside the padding.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltip::GetPadXPx() const
{
    return m_isWholePixelFrame ? GetBorderPx() + (float) m_scaler.ToPx ((int) s_kPadXDip) : m_scaler.ToPxf (s_kPadXDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPadYPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltip::GetPadYPx() const
{
    return m_isWholePixelFrame ? GetBorderPx() + (float) m_scaler.ToPx ((int) s_kPadYDip) : m_scaler.ToPxf (s_kPadYDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCornerRadiusPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltip::GetCornerRadiusPx() const
{
    float  radius = m_scaler.ToPxf (m_cornerDip);



    return m_isWholePixelFrame ? (float) std::lround (radius) : radius;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetShadow
//
//  The shadow for the fill the theme gives: a light tip's is the lighter.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiShadow::Style & DxuiTooltip::GetShadow() const
{
    constexpr float  kLightLuminance = 0.5f;



    return (DxuiColor::ComputeRelativeLuminance (m_bgArgb) > kLightLuminance) ? m_shadowOnLight : m_shadowOnDark;
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
    PointerExtent         extent        = { 0, DxuiSystemSettings::Instance().GetCursorHeightPx() };
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
//  A hover tip sits by the pointer, as the system's own tips do: below the
//  pointer's image from where it rests, or above where below would run off
//  the screen. Placed against its control instead, a tip lined up flush with
//  the control's edge, away from where the user was looking. A timed notice
//  and a tip stacked under another are placed against their anchor.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTooltip::GetPlacementAnchor() const
{
    RECT   anchor  = m_anchor;
    POINT  pointer = {};
    HWND   owner   = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;



    if (!m_isPointerTip)
    {
        return anchor;
    }

    if (m_pfnLocatePointer != nullptr && m_pfnLocatePointer (owner, pointer))
    {
        anchor = { pointer.x, pointer.y, pointer.x + 1, pointer.y + 1 };
    }

    //  The pointer's image is read once when the tip comes up, not on every
    //  move it follows.
    if (m_pfnMeasurePointer != nullptr)
    {
        anchor = MakePointerClearAnchor (anchor, m_hasPointerExtent ? m_pointerExtent : m_pfnMeasurePointer(), m_scaler.ToPx (kPointerGapDip));
    }

    return anchor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocatePointer
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTooltip::LocatePointer (HWND owner, POINT & outClientPx)
{
    bool  isFound = DxuiInputState::GetCursorPos (&outClientPx) != FALSE;



    if (isFound && owner != nullptr)
    {
        isFound = ScreenToClient (owner, &outClientPx) != FALSE;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHoverHideAtMs
//
//  Zero, for no hide time, unless SetHasLifetime turned the lifetime on.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiTooltip::GetHoverHideAtMs (int64_t nowMs) const
{
    return m_hasLifetime ? nowMs + ComputeVisibleMs (m_text.size(), GetSystemVisibleMs()) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSystemVisibleMs
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTooltip::GetSystemVisibleMs()
{
    UINT  doubleClickMs = (UINT) DxuiSystemSettings::Instance().GetDoubleClickMs();



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
//  ONLY FROM A TIP THAT WAITED ITS OWN DWELL. An instant tip -- the heat
//  map's, which follows the pointer across the map -- is up the moment the
//  pointer enters the map, so swapping from it made every control beside the
//  map show its tip at once: the Skip unchanged writes toggle above it had no
//  delay at all. That tip goes, and this one waits as though none were up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShow (const RECT & anchor, const std::wstring & text, int64_t nowMs, bool isAtPointer)
{
    bool  wasAtPointer = m_isPointerTip;
    bool  wasInstant   = m_isInstant;



    m_isInstant        = false;
    m_isPendingInstant = false;
    m_isNextPointerTip = isAtPointer;

    if (m_visible && wasInstant)
    {
        HideImmediate();
    }

    if (m_visible)
    {
        bool  isNewText = text != m_text;
        bool  isNewLook = IsNewLook();
        bool  changed   = wasAtPointer != isAtPointer || isNewText || isNewLook ||
                          anchor.left   != m_anchor.left  ||
                          anchor.top    != m_anchor.top   ||
                          anchor.right  != m_anchor.right ||
                          anchor.bottom != m_anchor.bottom;

        m_anchor       = anchor;
        m_text         = text;
        m_isPointerTip = isAtPointer;

        // THE DEADLINE SURVIVES A RE-REQUEST for the same tip. Consumers
        // re-issue RequestShow on every mouse-move over the same control, so
        // clearing the hide time here meant a resting pointer wiped it
        // sixty times a second and the tip never dismissed itself -- the
        // lifetime existed and could not once be reached. A move to a
        // DIFFERENT control is a new tip and starts its own clock.
        //  A tip that follows the pointer is placed where the pointer is when
        //  it shows, so a move to another control takes the old one down and
        //  shows the new one after the full delay again, as File Explorer's do.
        if (changed && m_followPointer)
        {
            ReleaseActivePopup();

            m_visible       = false;
            m_pendingAnchor = anchor;
            m_pendingText   = text;
            m_pending       = true;
            m_showAtMs      = nowMs + (int64_t) m_dwellOpenMs;
            return;
        }

        //  A balloon already up takes the new anchor and text in place: sized
        //  and drawn again only for new text or colors, and only moved for a
        //  new anchor, so a pointer crossing from part to part of a view does
        //  not close one window and open another, which flickered.
        if (changed && m_popupHost != nullptr && m_activePopup != nullptr && wasAtPointer == isAtPointer)
        {
            m_hideAtMs = isNewText ? GetHoverHideAtMs (nowMs) : m_hideAtMs;

            TakeLook();
            MovePopup (isNewText || isNewLook);
            return;
        }

        if (changed && m_popupHost != nullptr)
        {
            m_hideAtMs = GetHoverHideAtMs (nowMs);

            MeasurePointerForPlacement();
            ReleaseActivePopup();
            ShowPopup();
        }

        return;
    }

    //  A move over the same control keeps the clock it started, as Windows'
    //  tips do: restarting it on every move held the tip back until the
    //  pointer had stopped for the whole dwell.
    if (m_pending && text == m_pendingText)
    {
        m_pendingAnchor = anchor;
        return;
    }

    m_pendingAnchor = anchor;
    m_pendingText   = text;
    m_pending       = true;
    m_showAtMs      = nowMs + (int64_t) m_dwellOpenMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestShowFollowing
//
//  A tip already following the pointer moves with it. Otherwise the request
//  waits the open dwell, its clock running on while the pointer moves, and the tip
//  that comes up then follows the pointer as RequestShowNow's does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShowFollowing (const RECT & anchor, const std::wstring & text, int64_t nowMs)
{
    if (m_visible && !m_fadingOut && m_isInstant)
    {
        RequestShowNow (anchor, text, nowMs);
        return;
    }

    m_pendingAnchor = anchor;
    m_pendingText   = text;

    if (m_pending && m_isPendingInstant)
    {
        return;
    }

    if (m_visible)
    {
        HideImmediate();
    }

    m_isPendingInstant = true;
    m_isNextPointerTip = true;
    m_pending          = true;
    m_showAtMs         = nowMs + (int64_t) m_dwellOpenMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasurePointerForPlacement
//
//  The pointer's image, read once as a tip comes up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::MeasurePointerForPlacement()
{
    m_hasPointerExtent = false;

    if (m_pfnMeasurePointer != nullptr)
    {
        m_pointerExtent    = m_pfnMeasurePointer();
        m_hasPointerExtent = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSwatches
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::SetSwatches (const std::vector<DxuiTooltipSwatch> & swatches)
{
    std::vector<uint32_t>  argbs;



    for (const DxuiTooltipSwatch & swatch : swatches)
    {
        if (swatch.line >= 0)
        {
            argbs.resize ((std::max) (argbs.size(), (size_t) swatch.line + 1), 0);
            argbs[(size_t) swatch.line] = swatch.argb;
        }
    }

    SetLineSwatches (std::move (argbs));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestShowNowBelowPointer
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShowNowBelowPointer (POINT pointerPx, const std::wstring & text, int64_t nowMs)
{
    SetPlacement   (DxuiPopupPlacement::Below);
    RequestShowNow (RECT { pointerPx.x, pointerPx.y, pointerPx.x + 1, pointerPx.y + 1 }, text, nowMs);
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
    bool  isUp      = m_visible && !m_fadingOut && m_isInstant;
    bool  isNewLook = IsNewLook();
    bool  isSame    = isUp && !isNewLook && text == m_text && EqualRect (&anchor, &m_anchor) != FALSE;
    bool  isMoved   = isUp && m_activePopup != nullptr;
    bool  isNew     = text != m_text;



    //  A tip Tick would hide at once isn't put up: put up again at each move,
    //  it blinked on and off.
    if (isSame || (m_pfnIsActive != nullptr && !m_pfnIsActive()))
    {
        return;
    }

    m_anchor           = anchor;
    m_text             = text;
    m_pending          = false;
    m_isPendingInstant = false;
    m_visible          = true;
    m_fadingOut        = false;
    m_isInstant        = true;

    if (!isUp || isNew)
    {
        m_hideAtMs = GetHoverHideAtMs (nowMs);
    }

    if (isMoved)
    {
        TakeLook();
        MovePopup (isNew || isNewLook);
        return;
    }

    m_instantMaxPx = {};
    m_isPointerTip = true;

    MeasurePointerForPlacement();
    ReleaseActivePopup();
    ShowPopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowAt
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowAt (const RECT & anchor, const std::wstring & text)
{
    bool  isUp  = m_visible && !m_fadingOut && !m_isPointerTip && m_activePopup != nullptr;
    bool  isNew = text != m_text;



    if (isUp && !isNew && EqualRect (&anchor, &m_anchor) != FALSE)
    {
        return;
    }

    m_anchor       = anchor;
    m_text         = text;
    m_pending      = false;
    m_visible      = true;
    m_fadingOut    = false;
    m_isInstant    = true;
    m_isPointerTip = false;
    m_hideAtMs     = 0;

    if (isUp)
    {
        isNew = isNew || IsNewLook();

        TakeLook();
        MovePopup (isNew);
        return;
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
    bool  changed = !m_visible || m_fadingOut || text != m_text;



    m_fadingOut    = false;
    m_anchor       = anchor;
    m_text         = text;
    m_pending      = false;
    m_visible      = true;
    m_isInstant    = false;
    m_isPointerTip = false;
    m_hideAtMs     = nowMs + (int64_t) durationMs;

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
//  IsPointerOverApp
//
//  Whether the window under the pointer is one of this process's. A test of
//  the app in front hid every tip while the user hovered the debugger with
//  another app in front, and closed the instant ones a frame after they
//  opened.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTooltip::IsPointerOverApp()
{
    POINT  pointer   = {};
    HWND   under     = nullptr;
    DWORD  processId = 0;



    if (DxuiInputState::GetCursorPos (&pointer) == FALSE)
    {
        return true;
    }

    under = DxuiDisplay::Get().WindowFromPoint (pointer);

    if (under == nullptr)
    {
        return false;
    }

    GetWindowThreadProcessId (under, &processId);
    return processId == GetCurrentProcessId();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Tick (int64_t nowMs)
{
    //  A hover tip is hidden once the pointer is over another app's window,
    //  such as one brought over this one, since this app's windows get no
    //  pointer messages to dismiss it then. A timed notice ends at its set
    //  time.
    if (m_isPointerTip && (m_pending || m_visible) && m_pfnIsActive != nullptr && !m_pfnIsActive())
    {
        m_pending = false;
        FinishHide();
    }

    if (m_pending && nowMs >= m_showAtMs)
    {
        m_anchor           = m_pendingAnchor;
        m_text             = m_pendingText;
        m_visible          = true;
        m_pending          = false;
        m_isPointerTip     = m_isNextPointerTip;
        m_isInstant        = m_isPendingInstant;
        m_isPendingInstant = false;

        MeasurePointerForPlacement();

        // A hover tip has no hide time unless SetHasLifetime turned that on.
        // With one, a consumer that requests the tip again on every frame
        // closed it at the deadline and opened it again a dwell later, with
        // the pointer at rest. A caller that captures the pointer calls
        // HideImmediate as the capture starts, since no later move dismisses
        // the tip.
        m_hideAtMs = GetHoverHideAtMs (nowMs);

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

    GrowIfTooSmall();

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
    m_instantMaxPx = {};
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
    m_instantMaxPx = {};
    m_pending   = false;
    m_visible   = false;
    m_fadingOut = false;
    m_hideAtMs  = 0;
    m_text.clear();

    ReleaseActivePopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtendVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ExtendVisible (int64_t atMs)
{
    if (m_visible && !m_fadingOut)
    {
        m_hideAtMs = std::max (m_hideAtMs, atMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOnPopup
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTooltip::IsOnPopup (POINT screenPt) const
{
    RECT  rect = {};



    return m_activePopup != nullptr && m_activePopup->GetHwnd() != nullptr && GetWindowRect (m_activePopup->GetHwnd(), &rect) && PtInRect (&rect, screenPt);
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
    float                      widthPx  = 0.0f;
    float                      heightPx = 0.0f;



    // No host, nothing to say, or a balloon ALREADY UP: all three mean there
    // is nothing to do, and none of them is an error. The already-up case is
    // why this cannot be re-derived from m_activePopup further down -- by
    // then this function may have just acquired one.
    shows = (m_popupHost != nullptr && m_activePopup == nullptr && !m_text.empty());

    //  The look this tip is drawn with, with a popup or, without a host, in
    //  the window by Paint.
    if (m_activePopup == nullptr)
    {
        TakeLook();
    }

    //  A new tip ends any fade-out the last one began, which would otherwise
    //  hide this one as its fade-in ends.
    if (shows)
    {
        m_fadingOut = false;
    }

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
        TakeLook();

        showParams.ownerHwnd        = owner;
        showParams.anchorRectScreen = GetScreenAnchor();
        showParams.placement        = m_placement;

        //  An anchor as wide as the tip, centered on the pointer and spanning
        //  the pointer's own height, so above it the tip clears the pointer's
        //  tip and below it clears the arrow. The host keeps it on the monitor.
        if (m_followPointer)
        {
            POINT  pointer = {};
            int    gap     = m_scaler.ToPx (s_kPointerGapDip);
            int    below   = DxuiSystemSettings::Instance().GetCursorHeightPx() / 2;
            int    boxW    = 0;
            BOOL   got     = DxuiInputState::GetCursorPos (&pointer);

            IGNORE_RETURN_VALUE (got, TRUE);

            MeasureBoxPx (widthPx, heightPx);
            boxW = (int) std::ceil (widthPx);

            showParams.anchorRectScreen = { pointer.x - boxW / 2, pointer.y - gap, pointer.x - boxW / 2 + boxW, pointer.y + below + m_scaler.ToPx (s_kPointerBelowGapDip) };
            showParams.placement        = DxuiPopupPlacement::Above;
        }

        showParams.flipIfOffscreen  = true;
        showParams.dismiss          = DxuiPopupDismiss::Manual;
        showParams.input            = DxuiPopupInput::PassThrough;
        showParams.shadow           = true;
        showParams.shadowStyle      = GetShadow();
        showParams.cornerRadiusPx   = GetCornerRadiusPx();
        showParams.sizeDip          = MeasureBoxDip();
        showParams.backgroundArgb   = m_bgArgb;

        //  A whole-pixel frame is sized to the pixel, which whole DIPs cannot
        //  always give.
        if (m_isWholePixelFrame)
        {
            MeasureBoxPx (widthPx, heightPx);

            showParams.sizePx = SIZE { (LONG) std::ceil (widthPx), (LONG) std::ceil (heightPx) };
        }

        // A tip fades in rather than appearing. Same switch the menus read,
        // so turning menu animation off turns this off with it. An instant
        // tip appears whole, since it moves from cell to cell with the pointer.
        showParams.revealMs         = (DxuiSystemSettings::Instance().AreMenuAnimationsEnabled() && !m_isInstant)
                                          ? kFadeMs : 0;
        showParams.revealFade       = true;
        showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopup (p, t); };
        //  When the host closes the balloon itself, as when another popup
        //  takes its place, the tip is marked hidden, so the next request
        //  opens it again. A balloon this tip released is no longer the
        //  active one by the time it closes.
        showParams.onClosed         = [this, popup = m_activePopup] ()
        {
            if (m_activePopup == popup)
            {
                m_activePopup = nullptr;
                m_visible     = false;
                m_fadingOut   = false;
            }
        };

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

        //  A new balloon replaces one that was fading out.
        m_fadingOut = false;
        AssertPopupShown();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssertPopupShown
//
//  Debug builds only. Asserts that a balloon just opened or moved is on
//  screen: visible, not fading out, and above its owner window in the
//  z-order. A test-mode popup has no window and is not checked.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::AssertPopupShown() const
{
#if defined(_DEBUG)
    HRESULT  hr    = S_OK;
    HWND     hwnd  = (m_activePopup != nullptr) ? m_activePopup->GetHwnd() : nullptr;
    HWND     owner = (m_popupHost != nullptr) ? GetAncestor (m_popupHost->GetHwnd(), GA_ROOT) : nullptr;
    HWND     next  = hwnd;
    bool     isUp  = false;



    BAIL_OUT_IF (hwnd == nullptr || owner == nullptr, S_OK);

    isUp = IsWindowVisible (hwnd) != FALSE && !m_activePopup->IsFadingOut();
    CBRA (isUp);

    //  The owner comes after the balloon in the z-order.
    while (next != nullptr && next != owner)
    {
        next = GetWindow (next, GW_HWNDNEXT);
    }

    CBRA (next == owner);

Error:
    return;
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteIfTooSmall
//
//  Only a measurement counts: an estimate made without a renderer is not
//  a reason to resize. The balloon is only ever grown, so a balloon drawn
//  again at its new size is never noted again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::NoteIfTooSmall (IDxuiTextRenderer & text, float widthPx, float heightPx) const
{
    float  neededW = 0.0f;
    float  neededH = 0.0f;



    if (!MeasureBoxPx (neededW, neededH, &text))
    {
        return;
    }

    neededW = std::ceil (neededW);
    neededH = std::ceil (neededH);

    if (neededW <= widthPx && neededH <= heightPx)
    {
        return;
    }

    m_neededPx   = SIZE { (LONG) std::max (neededW, widthPx), (LONG) std::max (neededH, heightPx) };
    m_isTooSmall = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GrowIfTooSmall
//
//  Moved to the anchor it was placed against, at the size measured, so a tip
//  placed by the pointer stays by the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::GrowIfTooSmall()
{
    HRESULT  hr = S_OK;



    if (!m_isTooSmall)
    {
        return;
    }

    m_isTooSmall = false;

    if (m_activePopup == nullptr)
    {
        return;
    }

    hr = m_activePopup->MoveToPx (m_activePopup->GetParams().anchorRectScreen, m_neededPx);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureBoxPx
//
//  The balloon's size for the current text, in pixels: the text, rounded up
//  to whole pixels, and the padding on each side.
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
////////////////////////////////////////////////////////////////////////////////

bool DxuiTooltip::MeasureBoxPx (
    float              & widthPx,
    float              & heightPx,
    IDxuiTextRenderer  * text) const
{
    HRESULT                   hr         = S_OK;
    DxuiTooltipBody           body       = GetShownBody();
    float                     fontPx     = body.GetFontPx();
    float                     maxWPx     = m_scaler.ToPxf (body.GetMaxTextWidthDip());
    float                     textWPx    = 0.0f;
    float                     textHPx    = 0.0f;
    bool                      isMeasured = true;
    DxuiTooltipBody::Margins  margins;
    auto                      measure    = [this, text, &body] (const wchar_t * what, float sizePx, float wrapPx, float & w, float & h) -> HRESULT
    {
        if (text != nullptr)
        {
            return text->MeasureStringWrapped (what, sizePx, body.GetFace(), wrapPx, w, h);
        }

        return (m_activePopup != nullptr) ? m_activePopup->MeasureTextWrapped (what, sizePx, body.GetFace(), wrapPx, w, h) : E_FAIL;
    };



    if (body.IsTableTip())
    {
        TableLayout  table;

        hr = LayOutTable (m_text, fontPx, [&measure, fontPx] (const std::wstring & part, float & w, float & h)
        {
            return measure (part.c_str(), fontPx, DxuiTooltipBody::kNoWrapPx, w, h);
        }, table);

        textWPx = table.width + body.GetLineIndentPx (table.lineHeight);
        textHPx = table.lineHeight * (float) table.lines.size();
    }
    else
    {
        hr = measure (m_text.c_str(), fontPx, maxWPx, textWPx, textHPx);
    }

    if (FAILED (hr) || textWPx <= 0.0f)
    {
        float  estWPx   = (float) m_text.size() * fontPx * DxuiTooltipBody::kEstCharWidthEm;
        float  estLines = std::ceil (estWPx / maxWPx);

        isMeasured = false;
        textWPx    = std::min (estWPx, maxWPx);
        textHPx    = std::max (estLines, 1.0f) * fontPx * DxuiTooltipBody::kEstLineHeightEm;
    }

    if (textHPx <= 0.0f)
    {
        textHPx = fontPx * DxuiTooltipBody::kEstLineHeightEm;
    }

    margins  = body.GetContentMargins (GetPadXPx(), GetPadYPx());
    textWPx += body.GetCornerTagPx (measure);
    widthPx  = std::ceil (textWPx) + margins.left + margins.right;
    heightPx = std::ceil (textHPx) + margins.top  + margins.bottom;

    if (body.HasGradient())
    {
        widthPx   = std::max (widthPx, body.GetGradientWidthPx() + margins.left + margins.right);
        heightPx += body.GetGradientExtraPx();
    }

    if (body.HasFrame())
    {
        widthPx = std::max (widthPx, body.GetFrameWidthPx (measure));
    }

    //  A tip that follows the pointer from cell to cell stays at the largest size
    //  it has had since it opened, so a cell whose text is shorter, or whose
    //  last lines are yet to come, doesn't narrow the balloon and widen it
    //  again as the pointer moves.
    if (m_isInstant)
    {
        widthPx           = std::max (widthPx,  m_instantMaxPx.width);
        heightPx          = std::max (heightPx, m_instantMaxPx.height);
        m_instantMaxPx    = { widthPx, heightPx };
    }

    return isMeasured;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureBoxDip
//
//  The balloon's size in DIPs. The popup scales the size back up by the
//  owner DPI, so the trip into DIPs rounds UP -- rounding down would hand
//  back the pixel the measurement exists to keep.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiTooltip::MeasureBoxDip()
{
    UINT   dpi      = m_scaler.GetDpi();
    float  widthPx  = 0.0f;
    float  heightPx = 0.0f;
    SIZE   sizeDip  = {};



    dpi = (dpi == 0) ? (UINT) DxuiDpiScaler::kBaseDpi : dpi;

    MeasureBoxPx (widthPx, heightPx);

    sizeDip.cx = (int) std::ceil (widthPx  * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    sizeDip.cy = (int) std::ceil (heightPx * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);

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
    HRESULT  hr       = S_OK;
    float    widthPx  = 0.0f;
    float    heightPx = 0.0f;



    if (m_activePopup == nullptr)
    {
        return;
    }

    //  A balloon fading out is replaced rather than moved: moved, it ended its
    //  fade open and invisible, and every later tip went to it unseen.
    if (m_activePopup->IsFadingOut())
    {
        ReleaseActivePopup();
        ShowPopup();
        return;
    }

    if (isNewText && m_isWholePixelFrame)
    {
        MeasureBoxPx (widthPx, heightPx);

        hr = m_activePopup->MoveToPx (GetScreenAnchor(), SIZE { (LONG) std::ceil (widthPx), (LONG) std::ceil (heightPx) });
    }
    else if (isNewText)
    {
        hr = m_activePopup->MoveTo (GetScreenAnchor(), MeasureBoxDip());
    }
    else
    {
        hr = m_activePopup->Reposition (GetScreenAnchor());
    }

    //  A balloon that fails to move is replaced by a new one.
    if (FAILED (hr))
    {
        ReleaseActivePopup();
        ShowPopup();
        return;
    }

    AssertPopupShown();
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



    HRESULT          hr        = S_OK;
    DxuiTooltipBody  body      = GetShownBody();
    float            fontPx    = body.GetFontPx();
    float            padX      = GetPadXPx();
    float            padY      = GetPadYPx();
    float            borderPx  = GetBorderPx();
    float            anchorGap = m_scaler.ToPxf (s_kAnchorGapDip);
    float            textW     = 0.0f;
    float            textH     = 0.0f;
    float            width     = 0.0f;
    float            height    = 0.0f;
    float            boxLeft   = 0.0f;
    float            boxTop    = 0.0f;
    RECT             anchor    = {};
    bool             isTable   = body.IsTableTip();
    TableLayout      table;



    if (!m_visible || m_text.empty() || m_activePopup != nullptr)
    {
        return;
    }

    if (isTable)
    {
        hr = body.LayOutTableWith (const_cast<IDxuiTextRenderer &> (text), table);
        IGNORE_RETURN_VALUE (hr, S_OK);

        textW = table.width + body.GetSwatchIndentPx (table.lineHeight);
        textH = table.lineHeight * (float) table.lines.size();
    }
    else
    {
        hr = const_cast<IDxuiTextRenderer &> (text).MeasureStringWrapped (
                 m_text.c_str(), fontPx, body.GetFace(),
                 m_scaler.ToPxf (body.GetMaxTextWidthDip()), textW, textH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

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

    if (isTable)
    {
        body.DrawTable (painter, text, table, boxLeft + padX, boxTop + padY);
        return;
    }

    hr = text.DrawString (m_text.c_str(),
                          boxLeft + padX,
                          boxTop  + padY,
                          width  - padX * 2.0f,
                          height - padY * 2.0f,
                          m_textArgb,
                          fontPx,
                          body.GetFace());
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

    //  A balloon in a popup is measured and drawn at its host's DPI, set by
    //  ShowPopup. The scaler a layout passes can be a zoomed one; set while a
    //  balloon is up, it would draw the text larger than the box it was
    //  measured into.
    if (m_popupHost == nullptr)
    {
        m_scaler.SetDpi (scaler.GetDpi());
    }
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
//  Popup-host render hook. The host has already filled the card with
//  m_bgArgb, passed as showParams.backgroundArgb, so this only draws the
//  border (painter / D3D, under the text) and the text (D2D, composited on
//  top). Coordinates are popup-local pixels with the origin at the balloon's
//  top-left. Around a frame the tip's edge is drawn close to its fill, so
//  the frame is the border that reads.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RenderPopup (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT                   hr       = S_OK;
    RECT                      placed   = {};
    float                     width    = 0.0f;
    float                     height   = 0.0f;
    DxuiTooltipBody           body     = GetShownBody();
    DxuiTooltipBody::Margins  margins  = body.GetContentMargins (GetPadXPx(), GetPadYPx());
    float                     borderPx = GetBorderPx();
    float                     fontPx   = body.GetFontPx();



    if (m_activePopup == nullptr)
    {
        return;
    }

    placed = m_activePopup->GetPlacedRectScreenPx();
    width  = (float) (placed.right  - placed.left);
    height = (float) (placed.bottom - placed.top);

    NoteIfTooSmall (text, width, height);

    //  A tip that follows the pointer is a card with a shadow and no border.
    if (!m_followPointer)
    {
        painter.OutlineRoundedRect (0.0f, 0.0f, width, height, GetCornerRadiusPx(), borderPx, body.GetEdgeArgb());
    }

    if (body.HasFrame())
    {
        body.DrawFrame (painter, text, width, height);
    }

    if (body.HasGradient())
    {
        body.DrawGradient (painter, margins.left, width - margins.left - margins.right, height - margins.bottom);
        height -= body.GetGradientExtraPx();
    }

    if (body.IsTableTip())
    {
        TableLayout  table;

        hr = body.LayOutTableWith (text, table);

        if (SUCCEEDED (hr))
        {
            body.DrawTable     (painter, text, table, margins.left, margins.top);
            body.DrawCornerTag (text, width - margins.right, margins.top, table.lineHeight);
        }

        return;
    }

    body.DrawCornerTag (text, width - margins.right, margins.top, fontPx * DxuiTooltipBody::kEstLineHeightEm);

    hr = text.DrawString (m_text.c_str(),
                          margins.left,
                          margins.top,
                          width  - margins.left - margins.right,
                          height - margins.top  - margins.bottom,
                          m_textArgb,
                          fontPx,
                          body.GetFace());
    IGNORE_RETURN_VALUE (hr, S_OK);
}
