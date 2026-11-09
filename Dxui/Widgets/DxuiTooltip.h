#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/DxuiShadow.h"
#include "Window/DxuiPopupHost.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiTheme.h"


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

    // How long a tip with a lifetime stays up before dismissing itself.
    // Matches the OS default: long enough to read two lines.
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

    //  Replaces LocatePointer, so a test can put the pointer anywhere.
    void  SetPointerLocator  (PointerLocator locator)   { m_pfnLocatePointer = locator; }

    //  Whether the pointer is over a window of this process. A window under
    //  the pointer receives its moves whether or not its app is in front.
    using ActiveTest = bool (*) ();

    static bool  IsPointerOverApp ();

    //  Replaces IsPointerOverApp for a test: for this tip, or as the default
    //  for every tip made after it, since a test's pointer is never over a
    //  window of the test's process.
    static void  SetDefaultActiveTest (ActiveTest test)  { s_pfnDefaultIsActive = test; }

    void  SetActiveTest      (ActiveTest test)          { m_pfnIsActive = test; }

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
    float GetFontSizeDip  () const    { return m_fontDip; }

    //  A fixed-width face, for a tip of code or bytes. A tip whose columns
    //  are separated by tabs lines them up in either face (TableLayout).
    void  SetMonospace    (bool mono) { m_monospace = mono; }

    //  Whether a hover tip is hidden after a set time. Off by default, so a tip
    //  is up until the pointer is out of its area or another app is in the
    //  foreground.
    void  SetHasLifetime  (bool hasLifetime) { m_hasLifetime = hasLifetime; }

    //  A tip with a tab in its text is a table: the tab-separated parts of
    //  each line start at the same columns, one em apart at the widest, so
    //  they line up in a proportional face as they would in a fixed one. A
    //  line with no tab spans the table and adds to no column.
    struct TableLayout
    {
        std::vector<std::vector<std::wstring>>  lines;
        std::vector<float>                      columnX;
        std::vector<float>                      columnW;
        float                                   width      = 0.0f;
        float                                   lineHeight = 0.0f;
    };

    using MeasureFn = std::function<HRESULT (const std::wstring & part, float & widthPx, float & heightPx)>;

    static bool     IsTable     (const std::wstring & text) { return text.find (L'\t') != std::wstring::npos; }

    //  A table's line colors, one for each line from the top; zero, or a line
    //  past the last, is drawn in the tip's text color. Set before the
    //  request it goes with; a change of colors raises the tip again.
    void  SetLineColors (std::vector<uint32_t> argbs) { m_lineArgbs = std::move (argbs); }

    //  A square of color before each line, for a key to colors drawn
    //  elsewhere: one for each line from the top, zero for none. Every line
    //  is indented by the squares' width, and the tip is laid out as a
    //  table. Set before the request it goes with, as the line colors are.
    void  SetLineSwatches (std::vector<uint32_t> argbs) { m_lineSwatches = std::move (argbs); }

    //  A color scale before each of the given lines, from its first color on
    //  the left to its last, for a key to scales drawn elsewhere: one for each
    //  line from the top, none for a line without one. A line with a scale is
    //  indented by the scale's width, and the tip is laid out as a table. Set
    //  before the request it goes with, as the line colors are.
    void  SetLineScales   (std::vector<std::vector<uint32_t>> scales) { m_lineScales = std::move (scales); }

    //  How wide a line's scale is drawn, which a color key's scales match.
    static constexpr float  kLineScaleWidthDip = 64.0f;

    //  A color scale drawn under the text, its stops spread evenly from left
    //  to right, with an arrow under it at `mark`, from 0 at the left to 1 at
    //  the right; a mark outside that range has no arrow. No stops, no scale.
    //  Set before the request it goes with, as the line colors are.
    void  SetGradient     (std::vector<uint32_t> stops, float mark) { m_gradient = std::move (stops); m_gradientMark = mark; }

    //  The scale's color at a fraction of its width, 0 to 1.
    static uint32_t  GetGradientColor (const std::vector<uint32_t> & stops, float at);

    //  A rounded outline around everything in the tip, with its label on the
    //  top edge from the left, as a group of controls is labeled: in `argb`,
    //  or the tip's border and text colors for 0, each moved until its
    //  contrast with the tip's background is kMinTextContrast or
    //  kMinLineContrast. An
    //  empty label has no frame. Set before the request it goes with, as the
    //  line colors are.
    void  SetFrame        (std::wstring label, uint32_t argb) { m_frameLabel = std::move (label); m_frameArgb = argb; }

    //  A word at the right end of the first line, such as a mark on what the
    //  tip is about, in `argb` or the text color for 0, moved until its
    //  contrast with the background is kMinTextContrast. The tip is widened
    //  by it, so a mark that comes and goes doesn't add a line. Empty for none.
    void            SetCornerTag (std::wstring tag, uint32_t argb)   { m_cornerTag = std::move (tag); m_cornerTagArgb = argb; }
    static HRESULT  LayOutTable  (const std::wstring & text, float gapPx, const MeasureFn & measure, TableLayout & table);
    //  Text wider than this wraps onto more lines instead of growing the
    //  balloon past the window's edge; a fixed-width tip wraps at more than
    //  twice this. A tip of long lines, such as a log's, is given more.
    static constexpr float  kDefaultMaxTextWidthDip = 340.0f;
    void  SetMaxTextWidthDip (float dip) { m_maxTextWidthDip = dip; }
    void  SetDpi             (UINT dpi) { m_scaler.SetDpi (dpi); }
    void  SetViewportSize    (int widthPx, int heightPx) { m_viewportWPx = widthPx; m_viewportHPx = heightPx; }
    void  SetTheme           (const IDxuiTheme & theme)  { m_bgArgb = theme.TooltipBackground(); m_borderArgb = theme.TooltipBorder(); m_textArgb = theme.TooltipForeground(); }

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

    //  Explorer's tips: centered on the pointer and just above it, or just
    //  below where there is no room above on its monitor, rather than on the
    //  anchor; and a card with a shadow and no border.
    void  SetFollowPointer (bool follow) { m_followPointer = follow; }

    //  The same switch as SetHasLifetime, for a tip such as an info tip's
    //  that is up until RequestHide or HideImmediate takes it down.
    void  SetExpires      (bool expires) { m_hasLifetime = expires; }
    //  Which side of its anchor a balloon opens on; below unless set.
    void  SetPlacement    (DxuiPopupPlacement placement) { m_placement = placement; }

    //  Opens at once against the anchor, not the pointer, with no hide time, as
    //  a label that follows a mark. A repeat with new text or a new anchor
    //  moves the balloon that is up rather than opening another.
    void              ShowAt         (const RECT & anchor, const std::wstring & text);
    DxuiHwndSource  * GetPopupHost   () const { return m_popupHost;   }
    DxuiPopupHost   * GetActivePopup () const { return m_activePopup; }

    // A hover tip for `anchor`, raised after the open dwell. It is placed by
    // the pointer where the pointer came to rest, as the system's tips are,
    // or with isAtPointer false against the anchor, as a tip stacked under
    // another is. A move within the anchor with the same text does not move
    // it.
    void  RequestShow     (const RECT & anchor, const std::wstring & text, int64_t nowMs, bool isAtPointer = true);
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

    //  A visible tip that is not fading out closes no sooner than atMs.
    void  ExtendVisible   (int64_t atMs);

    //  True when a screen point is on the tip's popup.
    bool  IsOnPopup       (POINT screenPt) const;

    bool                 IsVisible () const { return m_visible; }

    //  How many times a balloon window has been raised. A tip that follows
    //  the pointer raises one and then moves it.
    int                  GetShowCount () const { return m_showCount; }

    // True while a dwell timer (deferred open or timed close) is pending or
    // the balloon is fading in. While it is true, a host that idle-blocks
    // calls Tick on a timeout rather than sleeping until the next input or
    // frame. Unticked, a fade-in stops at transparent, and a hover tip with no
    // lifetime is invisible until the pointer moves off it.
    bool                 WantsTick () const { return m_pending || m_fadingOut
                                                   || (m_visible && m_hideAtMs != 0)
                                                   || (m_activePopup != nullptr && m_activePopup->IsRevealing()); }

    //  The anchor a tip waiting out its open dwell is to be placed against.
    const RECT         & GetPendingAnchor() const { return m_pendingAnchor; }

    const std::wstring & GetText   () const { return m_text;    }
    const RECT         & GetAnchor () const { return m_anchor;  }

    void             Paint              (IDxuiPainter & painter, IDxuiTextRenderer & text) const;
    const wchar_t  * GetFace            () const { return m_monospace ? DxuiTheme::kMonoFace : DxuiTheme::GetUiFace(); }
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
    static constexpr int    kPointerGapDip     = 4;
    static constexpr float  kGradientGapDip    = 6.0f;
    static constexpr float  kGradientHeightDip = 10.0f;

    static constexpr float  kGradientArrowDip = 6.0f;
    static constexpr float  kGradientWidthDip = 200.0f;
    static constexpr int    kGradientSlices   = 64;

    static inline ActiveTest  s_pfnDefaultIsActive = IsPointerOverApp;

    float  GetGradientExtraPx () const;
    void   DrawGradient       (IDxuiPainter & painter, float left, float width, float bottom) const;

    //  The frame: its label's size, its inset from the tip's edge on every
    //  side, the gap between it and the content, and the lowest contrast of
    //  its label and line, WCAG's minimums for text and a control's border.
    static constexpr float  kFrameLabelScale  = 0.85f;
    static constexpr float  kFrameInsetDip    = 4.0f;
    static constexpr float  kFrameGapDip      = 6.0f;
    static constexpr float  kFrameCornerDip   = 4.0f;
    static constexpr float  kFrameMix         = 0.6f;
    static constexpr float  kFramedEdgeMix    = 0.15f;   // the tip's edge around a frame, close to its fill
    static constexpr float  kFrameLineEm      = 1.4f;
    static constexpr float  kMinTextContrast  = 4.5f;
    static constexpr float  kMinLineContrast  = 3.0f;

    //  The room around the content on each side: the tip's padding, or in a
    //  frame its inset and gap, with its label's line above.
    struct Margins
    {
        float  left   = 0.0f;
        float  top    = 0.0f;
        float  right  = 0.0f;
        float  bottom = 0.0f;
    };

    //  The label's line is drawn centered on the frame's top edge; its
    //  letters extend about kFrameCapEm of the label's size above and below
    //  the middle. The frame's top edge is kFrameInsetDip below the top of
    //  the letters, and its other edges are as far from the tip's edge as
    //  its top edge is.
    static constexpr float  kFrameCapEm = 0.36f;

    Margins  GetContentMargins () const;
    float    GetFrameLabelPx   () const { return m_scaler.ToPxf (m_fontDip) * kFrameLabelScale; }
    float    GetFrameLabelHPx  () const { return std::ceil (GetFrameLabelPx() * kFrameLineEm); }
    float    GetFrameEdgePx    () const { return std::round (m_scaler.ToPxf (kFrameInsetDip) + GetFrameLabelPx() * kFrameCapEm); }
    float    GetCornerTagPx    (IDxuiTextRenderer * text) const;
    void     DrawCornerTag     (IDxuiTextRenderer & text, float right, float top, float lineHeight) const;
    void     DrawFrame         (IDxuiPainter & painter, IDxuiTextRenderer & text, float width, float height) const;


    //  The system's tip lifetime: ten double-click times, as Windows sets
    //  a tooltip control's auto-pop delay.
    static int  GetSystemVisibleMs ();

    //  The time a tip opened at nowMs closes by itself, or 0 for never.
    int64_t     GetLifetimeEnd (int64_t nowMs) const;

    //
    //  Acquire + size + show the popup balloon for the current
    //  anchor/text. No-op without a wired host or when a popup is
    //  already up. Releases the popup on Show failure.
    //
    void  ShowPopup          ();

    //  The pointer's image, read once as a tip comes up.
    void  MeasurePointerForPlacement ();

    //  A hover tip's hide time: zero, for none, unless it has a lifetime.
    int64_t  GetHoverHideAtMs (int64_t nowMs) const;

    //  The balloon's size for the current text, the anchor in screen
    //  pixels, and the balloon that is up moved to the anchor in place.
    void  MeasureBoxPx       (float & widthPx, float & heightPx);
    SIZE  MeasureBoxDip      ();
    RECT  GetScreenAnchor    () const;
    void  MovePopup          (bool isNewText);

    //  Whether the colors, swatches, scales or gradient set differ from
    //  those drawn, and the ones set taken as the ones drawn.
    bool  IsNewLook          () const;
    void  TakeLook           ();

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

    //  The table's parts drawn from its top-left, in pixels.
    void  DrawTable          (IDxuiPainter & painter, IDxuiTextRenderer & text, const TableLayout & table, float left, float top) const;
    bool  IsTableTip         () const { return IsTable (m_text) || !m_lineSwatches.empty() || !m_lineScales.empty(); }
    float GetSwatchIndentPx  (float lineHeight) const;
    float GetLineIndentPx    (float lineHeight) const;
    void  DrawLineScale      (IDxuiPainter & painter, const std::vector<uint32_t> & stops, float left, float top, float heightPx) const;

    //  The table laid out with `text` measuring the parts at the tip's font.
    HRESULT  LayOutTableWith (IDxuiTextRenderer & text, TableLayout & table) const;

    DxuiDpiScaler                         m_scaler;
    RECT                                  m_anchor             = {};
    std::wstring                          m_text;
    std::wstring                          m_pendingText;
    RECT                                  m_pendingAnchor      = {};
    bool                                  m_isPointerTip       = true;
    bool                                  m_isNextPointerTip   = true;
    int64_t                               m_showAtMs           = 0;
    int64_t                               m_hideAtMs           = 0;
    int                                   m_dwellOpenMs        = 500;
    int                                   m_dwellCloseMs       = 100;
    float                                 m_fontDip            = 12.0f;
    bool                                  m_monospace          = false;
    float                                 m_maxTextWidthDip    = 0.0f;   // 0 for the default
    bool                                  m_followPointer      = false;
    uint32_t                              m_bgArgb             = 0xFF2D2D2D;
    uint32_t                              m_borderArgb         = 0xFF606060;
    uint32_t                              m_textArgb           = 0xFFE8EEF4;
    float                                 m_cornerDip          = DxuiTheme::kOverlayCornerRadiusDip;
    bool                                  m_isWholePixelFrame  = false;
    DxuiShadow::Style                     m_shadowOnDark;
    DxuiShadow::Style                     m_shadowOnLight;
    int                                   m_viewportWPx        = 0;
    int                                   m_viewportHPx        = 0;
    bool                                  m_visible            = false;
    bool                                  m_pending            = false;
    bool                                  m_fadingOut          = false;
    bool                                  m_isInstant          = false;
    DxuiHwndSource                      * m_popupHost          = nullptr;
    DxuiPopupHost                       * m_activePopup        = nullptr;
    PointerMeasurer                       m_pfnMeasurePointer  = MeasurePointerExtent;
    PointerLocator                        m_pfnLocatePointer   = LocatePointer;
    ActiveTest                            m_pfnIsActive        = s_pfnDefaultIsActive;
    DxuiPopupPlacement                    m_placement          = DxuiPopupPlacement::Below;
    bool                                  m_hasLifetime        = false;
    PointerExtent                         m_pointerExtent      = {};
    bool                                  m_hasPointerExtent   = false;
    int                                   m_showCount          = 0;
    std::vector<uint32_t>                 m_lineArgbs;
    std::vector<uint32_t>                 m_shownLineArgbs;
    std::vector<uint32_t>                 m_lineSwatches;
    std::vector<uint32_t>                 m_shownLineSwatches;
    std::vector<std::vector<uint32_t>>    m_lineScales;
    std::vector<std::vector<uint32_t>>    m_shownLineScales;
    std::vector<uint32_t>                 m_gradient;
    std::vector<uint32_t>                 m_shownGradient;
    float                                 m_gradientMark       = -1.0f;
    float                                 m_shownGradientMark  = -1.0f;
    std::wstring                          m_cornerTag;
    std::wstring                          m_shownCornerTag;
    uint32_t                              m_cornerTagArgb      = 0;
    uint32_t                              m_shownCornerTagArgb = 0;
    std::wstring                          m_frameLabel;
    std::wstring                          m_shownFrameLabel;
    uint32_t                              m_frameArgb          = 0;
    uint32_t                              m_shownFrameArgb     = 0;
};
