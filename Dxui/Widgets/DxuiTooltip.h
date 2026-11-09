#pragma once

#include "Pch.h"

#include "Window/DxuiPopupHost.h"
#include "Core/IDxuiControl.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"


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

struct DxuiTooltipSwatch
{
    int       line = 0;
    uint32_t  argb = 0;
};


class DxuiTooltip : public IDxuiControl
{
public:
    ~DxuiTooltip() override = default;

    // How long a tip with a lifetime stays up before dismissing itself. Matches
    // the OS default: long enough to read two lines.
    static constexpr int  kMaxVisibleMs = 5000;

    // A long tip stays up long enough to read: the system's tip lifetime, or
    // kReadMsPerChar for each character when that is longer, to kMaxReadMs.
    static constexpr int  kReadMsPerChar = 60;
    static constexpr int  kMaxReadMs     = 30000;

    static int  ComputeVisibleMs (size_t textLength, int systemMs);

    //  How far the pointer's image reaches above and below its hot spot, in
    //  pixels: what a tip that follows the pointer must keep clear of.
    struct PointerExtent
    {
        int  aboveHotspotPx = 0;
        int  belowHotspotPx = 0;
    };

    using PointerMeasurer = PointerExtent (*) ();

    //  Where the pointer is, in the owner's client pixels; false when it
    //  cannot be found.
    using PointerLocator  = bool (*) (HWND owner, POINT & outClientPx);

    static bool  LocatePointer (HWND owner, POINT & outClientPx);

    //  The current pointer's extent, read from its image, or the system's
    //  cursor height below the hot spot where the image cannot be read.
    static PointerExtent  MeasurePointerExtent ();

    //  An anchor grown to clear the pointer anywhere inside it, by `extent`
    //  and `gapPx` above and below.
    static RECT  MakePointerClearAnchor (const RECT & anchor, const PointerExtent & extent, int gapPx);

    //  Replaces MeasurePointerExtent, so a test can supply a pointer.
    void  SetPointerMeasurer (PointerMeasurer measurer) { m_pfnMeasurePointer = measurer; }

    //  Replaces LocatePointer, so a test can put the pointer where it wants.
    void  SetPointerLocator  (PointerLocator locator)   { m_pfnLocatePointer = locator; }

    //  Whether a window of this process is in the foreground.
    using ActiveTest = bool (*) ();

    static bool  IsAppActive ();

    //  Replaces IsAppActive for a test. With no popup host and no test, the
    //  foreground has no effect on the tooltip.
    void  SetActiveTest      (ActiveTest test)          { m_pfnIsActive = test; }

    //  The rect the tip is placed against: for a hover tip, the pointer's hot
    //  spot grown to clear the pointer's image, as the system's own tips sit;
    //  for a timed notice, its anchor.
    RECT  GetPlacementAnchor () const;

    // A tip fades in when it appears and out when it goes. Short enough that
    // it never delays reading the tip, long enough that the tip does not
    // appear to blink into place.
    static constexpr int  kFadeMs       = 150;

    void  SetDwellOpenMs  (int ms) { m_dwellOpenMs = ms; }
    void  SetDwellCloseMs (int ms) { m_dwellCloseMs = ms; }
    void  SetFontSizeDip  (float dip) { m_fontDip = dip; }

    //  A fixed-width face, for a tip whose lines are columns.
    void  SetMonospace    (bool mono) { m_monospace = mono; }

    //  Whether a hover tip is hidden after a set time. Off by default, so a tip
    //  is up until the pointer is out of its area or another app is in the
    //  foreground.
    void  SetHasLifetime  (bool hasLifetime) { m_hasLifetime = hasLifetime; }

    //  Text wider than this wraps onto more lines instead of growing the
    //  balloon past the window's edge. A tip of long lines, such as a log's,
    //  is given more.
    static constexpr float  kDefaultMaxTextWidthDip = 340.0f;
    void  SetMaxTextWidthDip (float dip) { m_maxTextWidthDip = dip; }

    //  Explorer's tips: centered on the pointer and just above it, or just
    //  below where there is no room above on its monitor, rather than on the
    //  anchor; and a card with a shadow and no border.
    void  SetFollowPointer (bool follow) { m_followPointer = follow; }

    //  A square of a color beside each of the given lines, for a tip about
    //  colors; with an empty list, no squares are drawn.
    void  SetSwatches     (const std::vector<DxuiTooltipSwatch> & swatches);

    //  Line colors, one for each line from the top; zero, or a line past the
    //  last, is drawn in the tip's text color. A tip with line colors is not
    //  wrapped. Applies from the next RequestShow or RequestShowNow.
    void  SetLineColors   (const std::vector<uint32_t> & argbs);

    //  A grid under the text, one row of cells per row: each column as wide
    //  as its widest cell, the first column's cells at the left and the rest
    //  centered. An empty grid draws nothing. Drawn in a popup only.
    void  SetTable        (const std::vector<std::vector<std::wstring>> & rows);
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetViewportSize (int widthPx, int heightPx) { m_viewportWPx = widthPx; m_viewportHPx = heightPx; }
    void  SetTheme        (const IDxuiTheme & theme)  { m_bgArgb = theme.TooltipBackground(); m_borderArgb = theme.TooltipBorder(); m_textArgb = theme.TooltipForeground(); }

    //
    //  Opt-in popup hosting (FR-054 / FR-061). When a host is wired
    //  up the tooltip renders into a WS_POPUP HWND with
    //  WS_EX_TRANSPARENT | WS_EX_LAYERED so pointer events pass
    //  through to whatever is underneath; dismiss is OnPointerLeave.
    //
    void              SetPopupHost   (DxuiHwndSource * host) { m_popupHost = host; m_pfnIsActive = (m_pfnIsActive != nullptr) ? m_pfnIsActive : IsAppActive; }
    DxuiHwndSource  * GetPopupHost   () const { return m_popupHost;   }
    DxuiPopupHost   * GetActivePopup () const { return m_activePopup; }

    void  RequestShow     (const RECT & anchor, const std::wstring & text, int64_t nowMs);
    void  RequestHide     (int64_t nowMs);

    // Shows at once, with no open dwell and no fade, for a tip that follows
    // the pointer from cell to cell, such as a hex view's address under the
    // pointer. Only this request skips the dwell; the next RequestShow waits
    // as it always does.
    void  RequestShowNow  (const RECT & anchor, const std::wstring & text, int64_t nowMs);

    // Shows immediately (no open dwell) and auto-hides after durationMs.
    // For transient notices where no pointer-leave will arrive to dismiss
    // it -- e.g. entering paddle mode captures the mouse, so the hover that
    // would normally hide the tooltip never fires.
    void  ShowTimed       (const RECT & anchor, const std::wstring & text, int64_t nowMs, int durationMs, DxuiPopupPlacement placement = DxuiPopupPlacement::Below);
    void  Tick            (int64_t nowMs) override;

    //
    //  Synchronously tear down any live popup and reset the dwell
    //  state. Called by the owner before its popup host (and pool) is
    //  destroyed, since the timed RequestHide path would release the
    //  popup too late.
    //
    void  HideImmediate   ();

    bool                 IsVisible () const { return m_visible; }
    bool                 IsPending () const { return m_pending; }

    //  Whether the tip is up: visible, and with a popup host, its balloon
    //  open.
    bool                 IsShown   () const { return m_visible && (m_popupHost == nullptr || m_activePopup != nullptr); }

    // True while a dwell timer (deferred open or timed close) is still
    // pending, or a tip is up that has to be hidden if another app comes to the
    // foreground. A host that idle-blocks calls Tick on a timeout while this is
    // true, rather than sleeping until the next input/frame.
    bool                 WantsTick () const { return m_pending || m_fadingOut
                                                   || (m_visible && (m_hideAtMs != 0 || m_pfnIsActive != nullptr)); }
    const std::wstring & GetText   () const { return m_text;    }
    const RECT         & GetAnchor () const { return m_anchor;  }

    void             Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    const wchar_t  * GetFace () const { return m_monospace ? DxuiTheme::kMonoFace : DxuiTheme::GetUiFace(); }

    //
    //  IDxuiControl overrides — additive shims so DxuiTooltip can
    //  appear in a DxuiPanel tree. Typical hosting is via
    //  DxuiPopupHost (WS_POPUP transparent overlay).
    //
    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring        GetAccessibleName () const override { return m_text; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    //  The room between a tip that follows the pointer and the pointer.
    static constexpr int  kPointerGapDip = 4;

    //  The system's tip lifetime: ten double-click times, as Windows sets
    //  a tooltip control's auto-pop delay.
    static int  GetSystemVisibleMs ();

    //
    //  Acquire + size + show the popup balloon for the current
    //  anchor/text. No-op without a wired host or when a popup is
    //  already up. Releases the popup on Show failure.
    //
    void  ShowPopup          ();

    //
    //  Apply the current text, anchor and swatch to the balloon already up,
    //  or show a new one when there is none to update.
    //
    void     UpdatePopup       ();
    void     AssertPopupShown  () const;
    void     MeasureBox        (RECT & outAnchorScreen, SIZE & outSizeDip);
    float    GetSwatchColumnPx () const;
    void     MeasureTable      (float fontPx, float & outWidthPx, float & outHeightPx);
    int64_t  GetHideAtMs       (int64_t nowMs) const;

    //
    //  Return the live popup to the host pool (hiding its HWND) and
    //  drop the pointer. Safe to call when none is active.
    //
    void  ReleaseActivePopup ();

    //  Drop the tip for good: state cleared and the popup returned to the
    //  pool. Reached when a fade-out finishes, or at once when animations
    //  are off.
    void  FinishHide         ();

    //
    //  Render hook invoked by the popup host (popup-local pixels,
    //  origin top-left). Draws the balloon border + text over the
    //  host's opaque background clear.
    //
    void  RenderPopup        (IDxuiPainter & painter, IDxuiTextRenderer & text) const;

    DxuiDpiScaler                             m_scaler;
    RECT                                      m_anchor            = {};
    std::wstring                              m_text;
    std::wstring                              m_pendingText;
    RECT                                      m_pendingAnchor     = {};
    DxuiPopupPlacement                        m_placement         = DxuiPopupPlacement::Below;
    int64_t                                   m_showAtMs          = 0;
    int64_t                                   m_hideAtMs          = 0;
    int                                       m_dwellOpenMs       = 500;
    int                                       m_dwellCloseMs      = 100;
    float                                     m_fontDip           = 12.0f;
    bool                                      m_monospace         = false;
    std::vector<std::vector<std::wstring>>    m_table;
    std::vector<float>                        m_tableColumnsPx;
    float                                     m_tableTopPx        = 0.0f;
    uint32_t                                  m_bgArgb            = 0xFF2D2D2D;
    uint32_t                                  m_borderArgb        = 0xFF606060;
    uint32_t                                  m_textArgb          = 0xFFE8EEF4;
    int                                       m_viewportWPx       = 0;
    int                                       m_viewportHPx       = 0;
    bool                                      m_visible           = false;
    bool                                      m_pending           = false;
    bool                                      m_fadingOut         = false;
    bool                                      m_isInstant         = false;
    bool                                      m_hasLifetime       = false;
    bool                                      m_followPointer     = false;
    float                                     m_maxTextWidthDip   = kDefaultMaxTextWidthDip;
    ActiveTest                                m_pfnIsActive       = nullptr;
    std::vector<DxuiTooltipSwatch>            m_swatches;
    std::vector<uint32_t>                     m_lineArgbs;
    bool                                      m_isSwatchChanged   = false;
    float                                     m_lineHeightPx      = 0.0f;
    DxuiHwndSource                          * m_popupHost         = nullptr;
    DxuiPopupHost                           * m_activePopup       = nullptr;
    PointerMeasurer                           m_pfnMeasurePointer = MeasurePointerExtent;
    PointerLocator                            m_pfnLocatePointer  = LocatePointer;
    bool                                      m_isPointerTip      = true;
};
