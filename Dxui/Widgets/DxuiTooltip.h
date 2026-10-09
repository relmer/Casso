#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/DxuiShadow.h"
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

class DxuiTooltip : public IDxuiControl
{
public:
    ~DxuiTooltip() override = default;

    // How long a hover tip stays up before dismissing itself. Matches the
    // OS default: long enough to read two lines, short enough that a parked
    // pointer does not leave a panel sitting over the control it describes.
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

    //  The current pointer's extent, read from its image, or the system's
    //  cursor height below the hot spot where the image cannot be read.
    static PointerExtent  MeasurePointerExtent ();

    //  An anchor grown to clear the pointer anywhere inside it, by `extent`
    //  and `gapPx` above and below.
    static RECT  MakePointerClearAnchor (const RECT & anchor, const PointerExtent & extent, int gapPx);

    //  Replaces MeasurePointerExtent, so a test can supply a pointer.
    void  SetPointerMeasurer (PointerMeasurer measurer) { m_pfnMeasurePointer = measurer; }

    //  The rect the tip is placed against: the anchor, or for a tip that
    //  follows the pointer, the anchor grown to clear the pointer's image.
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
    void  SetDpi          (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetViewportSize (int widthPx, int heightPx) { m_viewportWPx = widthPx; m_viewportHPx = heightPx; }
    void  SetTheme        (const IDxuiTheme & theme)  { m_bgArgb = theme.TooltipBackground(); m_borderArgb = theme.TooltipBorder(); m_textArgb = theme.TooltipForeground(); }

    //  The colors SetTheme took.
    uint32_t  GetBackgroundArgb () const { return m_bgArgb;     }
    uint32_t  GetBorderArgb     () const { return m_borderArgb; }
    uint32_t  GetTextArgb       () const { return m_textArgb;   }

    //  The balloon's frame. By default it is a flyout's: the overlay corner
    //  radius, a one-DIP border inside the padding, and a menu's shadow, as
    //  File Explorer's tips are. ApplyVisualStudioLook sets Visual Studio's.
    void  SetCornerRadiusDip    (float dip)   { m_cornerDip = dip; }
    void  SetWholePixelFrame    (bool on)     { m_isWholePixelFrame = on; }
    void  SetShadow             (const DxuiShadow::Style & onDark, const DxuiShadow::Style & onLight) { m_shadowOnDark = onDark; m_shadowOnLight = onLight; }
    void  ApplyVisualStudioLook ();

    //  The frame in pixels at the tip's DPI: the border, the room from the
    //  balloon's edge to its text, the corner radius, and the shadow under
    //  the fill the theme gives.
    float                       GetBorderPx       () const;
    float                       GetPadXPx         () const;
    float                       GetPadYPx         () const;
    float                       GetCornerRadiusPx () const;
    const DxuiShadow::Style &   GetShadow         () const;

    //
    //  Opt-in popup hosting (FR-054 / FR-061). When a host is wired
    //  up the tooltip renders into a WS_POPUP HWND with
    //  WS_EX_TRANSPARENT | WS_EX_LAYERED so pointer events pass
    //  through to whatever is underneath; dismiss is OnPointerLeave.
    //
    void  SetPopupHost    (DxuiHwndSource * host) { m_popupHost = host; }
    DxuiHwndSource *  GetPopupHost   () const { return m_popupHost;   }
    DxuiPopupHost  *  GetActivePopup () const { return m_activePopup; }

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

    //  How many times a balloon window has been raised. A tip that follows
    //  the pointer raises one and then moves it.
    int                  GetShowCount () const { return m_showCount; }

    // True while a dwell timer (deferred open or timed close) is still
    // pending, so a host that idle-blocks knows to keep calling Tick on a
    // timeout rather than sleeping until the next input/frame.
    bool                 WantsTick () const { return m_pending || m_fadingOut
                                                   || (m_visible && m_hideAtMs != 0); }
    const std::wstring & GetText   () const { return m_text;    }
    const RECT         & GetAnchor () const { return m_anchor;  }

    void  Paint           (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    const wchar_t *  GetFace () const { return m_monospace ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace; }
    float            GetMaxTextWidthDip () const;

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

    //  The balloon's size for the current text, the anchor in screen
    //  pixels, and the balloon that is up moved to the anchor in place.
    void  MeasureBoxPx       (float & widthPx, float & heightPx);
    SIZE  MeasureBoxDip      ();
    RECT  GetScreenAnchor    () const;
    void  MovePopup          (bool isNewText);

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

    DxuiDpiScaler      m_scaler;
    RECT               m_anchor            = {};
    std::wstring       m_text;
    std::wstring       m_pendingText;
    RECT               m_pendingAnchor     = {};
    int64_t            m_showAtMs          = 0;
    int64_t            m_hideAtMs          = 0;
    int                m_dwellOpenMs       = 500;
    int                m_dwellCloseMs      = 100;
    float              m_fontDip           = 12.0f;
    bool               m_monospace         = false;
    uint32_t           m_bgArgb            = 0xFF2D2D2D;
    uint32_t           m_borderArgb        = 0xFF606060;
    uint32_t           m_textArgb          = 0xFFE8EEF4;
    float              m_cornerDip         = DxuiTheme::kOverlayCornerRadiusDip;
    bool               m_isWholePixelFrame = false;
    DxuiShadow::Style  m_shadowOnDark;
    DxuiShadow::Style  m_shadowOnLight;
    int                m_viewportWPx       = 0;
    int                m_viewportHPx       = 0;
    bool               m_visible           = false;
    bool               m_pending           = false;
    bool               m_fadingOut         = false;
    bool               m_isInstant         = false;
    DxuiHwndSource   * m_popupHost         = nullptr;
    DxuiPopupHost    * m_activePopup       = nullptr;
    PointerMeasurer    m_pfnMeasurePointer = MeasurePointerExtent;
    PointerExtent      m_pointerExtent     = {};
    bool               m_hasPointerExtent  = false;
    int                m_showCount         = 0;
};
