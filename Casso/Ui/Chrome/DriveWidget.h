#pragma once

#include "Pch.h"

#include "CassoTheme.h"
#include "Core/IDxuiControl.h"
#include "LedIndicator.h"
#include "../DriveWidgetState.h"





class IDriveCommandSink;


enum class DriveWidgetRegion
{
    None,
    Body,
    Eject,
};


//
//  DriveWidget is Casso-specific (skeuomorphic Apple Disk II). The
//  IDxuiControl Paint signature uses an IDxuiTheme, but the widget
//  expects that theme is actually a CassoTheme and static_casts to
//  read drive-body / bezel / label palette fields. A debug
//  dynamic_cast guard in Paint pins the contract.
//
class DriveWidget : public IDxuiControl
{
public:
    DriveWidget  ();
    ~DriveWidget () override = default;

    void               Initialize      (int slot, int drive, IDriveCommandSink * pSink);

    // Hide the whole widget: a machine with no Disk ][ controller must show
    // NO disk UI at all. Zeroing the rects alone is not enough -- Paint has no
    // bounds guard and would still stamp the "IN USE" label + LED at stale /
    // origin positions -- so also latch m_hidden, which makes Paint early-out.
    // Layout (positioning the widget for a machine that HAS a controller)
    // clears the latch.
    void               Hide            ()
    {
        m_bodyRect  = {};
        m_faceRect  = {};
        m_slotRect  = {};
        m_ejectRect = {};
        m_labelRect = {};
        m_hidden    = true;
    }

    void               SetPerspectiveSkewPx (int skewPx) { m_perspectiveSkewPx = skewPx; }
    void               SetCompact      (bool compact)    { m_compact = compact; }
    bool               IsCompact       () const          { return m_compact; }
    void               SetFocused      (bool focused)    { m_focused = focused; }
    bool               IsFocused       () const          { return m_focused; }
    void               SyncFromState   (const DriveWidgetState & state);

    // Marquee hover trigger. The shell calls this each mouse move with
    // whether the pointer is over the widget; a fresh enter restarts the
    // one-shot basename scroll so the full filename can be re-read on
    // demand. Owns the enter-edge detection so a stationary hover doesn't
    // continuously re-trigger.
    void               UpdateMarqueeHover (bool inside, int64_t nowMs)
    {
        if (inside && !m_marqueeHovered)
        {
            m_marqueeStartMs = nowMs;
        }

        m_marqueeHovered = inside;
    }

    void               Paint           (IDxuiPainter        & painter,
                                        IDxuiTextRenderer   & text,
                                        const IDxuiTheme    & theme) override;

    //
    //  IDxuiControl::Layout — treats boundsDip.left / boundsDip.top as
    //  the widget's anchor and computes its own body / face / slot /
    //  eject / label rects from per-DPI metrics scaled off
    //  scaler.Dpi(). boundsDip.right / bottom are ignored (the widget
    //  has an intrinsic size; SetBounds is overwritten with the
    //  computed OuterRect at the end).
    //
    void               Layout          (const RECT          & boundsDip,
                                        const DxuiDpiScaler & scaler) override;

    DriveWidgetRegion  HitTest         (int x, int y) const;
    HRESULT            OnDrop          (const std::wstring & path);

    // Write-protect breakdown of the mounted image, refreshed by
    // SyncFromState. The shell reads it to decide whether to surface the
    // hover tooltip and to compose the source-specific message.
    const WriteProtectInfo & WriteProtect () const { return m_state.writeProtect; }
    bool               IsWriteProtected () const { return m_state.writeProtect.Any(); }

    RECT               BodyRect        () const { return m_bodyRect; }
    RECT               OuterRect       () const
    {
        RECT  r = m_bodyRect;
        if (m_labelRect.bottom > r.bottom) { r.bottom = m_labelRect.bottom; }
        return r;
    }

    RECT               EjectRect       () const { return m_ejectRect; }
    LedState           Led             () const { return m_led.GetState(); }
    int                Drive           () const { return m_drive; }

private:
    // Widget geometry, palette, and the primitive-drawing helpers that
    // consume it. Every reader is a DriveWidget method, so the whole block
    // belongs to the class rather than to the translation unit.
    static constexpr int              kBaseDpi            = 96;
    static constexpr int              kBodyWidthPx        = 220;
    static constexpr int              kBodyHeightPx       = 160;
    static constexpr int              kFaceplateHeightPx  = 104;
    static constexpr int              kCaseBackInsetPx    = 30;
    static constexpr int              kLabelPadPx         = 10;
    static constexpr float            kLabelFontDip       = 13.0f;
    static constexpr float            kInUseFontDip       = 10.0f;
    static constexpr int              kSlotInsetPx        = 22;
    static constexpr int              kSlotHeightPx       = 6;
    static constexpr int              kSlotCenterYPx      = 50;
    static constexpr int              kDoorWidthPx        = 72;
    static constexpr int              kDoorHeightPx       = 44;
    static constexpr int              kDoorTravelPx       = 32;
    static constexpr int              kNotchWidthPx       = 28;
    static constexpr int              kNotchHeightPx      = 8;
    static constexpr int              kLedCenterYPx       = 84;
    static constexpr int              kInUseGapPx         = 4;
    static constexpr int              kInUseWidthPx       = 56;
    static constexpr int              kRidgeCountPx       = 2;
    static constexpr int              kVentCountPx        = 9;   // matches real Disk II side-vent count
    static constexpr int              kVentSlotHeightPx   = 1;   // each vent is 1 px tall (scaled by DPI)
    static constexpr int              kVentSlotGapPx      = 2;   // vertical gap between vents
    static constexpr int              kCassowaryWidthPx   = 28;
    static constexpr int              kCassowaryHeightPx  = 42;
    static constexpr int              kCassowaryMarginPx  = 6;
    static constexpr int              kLabelStripHeightPx = 18;
    static constexpr int              kLabelStripGapPx    = 2;
    static constexpr float            kBasenameFontDip    = 11.0f;
    static constexpr const wchar_t  * kFontFamily         = DxuiTheme::kBodyFace;

    // Marquee timing for an overflowing basename label. The hold delay is
    // both the lead-in before a freshly mounted disk first scrolls and the
    // pause between replays while the pointer lingers over the widget.
    static constexpr int64_t kMarqueeHoldMs         = 2000;
    static constexpr float   kMarqueeSpeedDipPerSec = 45.0f;
    static constexpr float   kMarqueeGapDip         = 25.0f;

    // Compact paint-path dimensions. The compact widget is a flat
    // rounded card with "DRIVE N" on the left and the status LED on
    // the right -- no 3D case top, no door, no cassowary. Total
    // height is sized so the drive bar can shrink the bottom inset
    // dramatically when the active theme requests compact drives.
    static constexpr int     kCompactBodyWidthPx  = 140;
    static constexpr int     kCompactBodyHeightPx = 40;
    static constexpr int     kCompactPadPx        = 10;
    static constexpr int     kCompactCornerPx     = 4;
    static constexpr float   kCompactFontDip      = 12.0f;

    // Write-protect padlock badge. A small brass lock stamped on the
    // drive face (skeuomorphic) or beside the status LED (compact)
    // whenever the mounted disk is write-protected by any source. Kept
    // deliberately understated -- it reads as "locked" without competing
    // with the LED for attention.
    static constexpr int      kWpBadgeWidthPx   = 13;
    static constexpr int      kWpBadgeHeightPx  = 15;
    static constexpr uint32_t kWpBadgeFillArgb  = 0xFFD8B76A;   // warm brass body
    static constexpr uint32_t kWpBadgeShadeArgb = 0xFF7A6026;   // darker brass edge / shackle
    static constexpr uint32_t kWpBadgeHoleArgb  = 0xFF2A2109;   // keyhole

    // Damaged-image badge, shown in the padlock's place when the mounted
    // image's stored checksum did not match its contents. A deliberately
    // different mark, not a variant of the padlock: an ordinary write-protect
    // is a setting the user chose and can clear, while this is the file
    // itself being wrong, and the two must not read as the same state. Amber
    // warning triangle, which is louder than the brass on purpose.
    static constexpr int      kDamageBadgeWidthPx  = 15;
    static constexpr int      kDamageBadgeHeightPx = 14;
    static constexpr uint32_t kDamageFillArgb      = 0xFFE8A317;   // amber body
    static constexpr uint32_t kDamageEdgeArgb      = 0xFF7A4E00;   // darker amber edge
    static constexpr uint32_t kDamageMarkArgb      = 0xFF241500;   // exclamation mark

    static bool  RectContains (const RECT & rect, int x, int y);
    static int   Scale        (int value, UINT dpi);
    static float Clamp01      (float v);

    // Fills a trapezoid with parallel horizontal front and back edges
    // by stacking 1-px horizontal scanlines whose widths interpolate
    // linearly from front to back. Used for the receding case top.
    static void  FillTrapezoidApprox (IDxuiPainter & painter,
                                      float frontLeft,  float frontRight,
                                      float backLeft,   float backRight,
                                      float frontY,     float backY,
                                      uint32_t argb);

    // Draws a horizontal ridge line on the case top at fractional depth
    // (0=front, 1=back), respecting the trapezoid's perspective taper.
    static void  DrawCaseRidge (DxuiPainter & painter,
                                float frontLeft, float frontRight,
                                float backLeft,  float backRight,
                                float frontY,    float backY,
                                float depthT,
                                uint32_t argb);

    // Draws a small padlock (shackle arch + body + keyhole) inside the
    // given box using flat fills, matching the widget's primitive-drawn
    // house style. Used as the write-protect indicator.
    static void  DrawPadlock (IDxuiPainter & painter,
                              float left, float top, float w, float h,
                              uint32_t fill, uint32_t shade, uint32_t hole);

    // Draws a warning triangle with an exclamation mark inside the given
    // box, in the same flat-fill house style as the padlock. Used for a
    // damaged image, where "write-protected" would understate the problem.
    static void  DrawDamageBadge (IDxuiPainter & painter,
                                  float left, float top, float w, float h,
                                  uint32_t fill, uint32_t edge, uint32_t mark);

    void                PaintBasenameLabel (IDxuiTextRenderer & text,
                                            const CassoTheme & theme,
                                            UINT                dpi);

    int                  m_slot              = 6;
    int                  m_drive             = 0;
    IDriveCommandSink  * m_sink              = nullptr;
    RECT                 m_bodyRect          = {};
    RECT                 m_faceRect          = {};
    RECT                 m_slotRect          = {};
    RECT                 m_ejectRect         = {};
    RECT                 m_labelRect         = {};
    LedIndicator         m_led;
    DriveWidgetState     m_state;
    UINT                 m_dpi               = 96;
    int                  m_perspectiveSkewPx = 0;
    bool                 m_compact           = false;
    bool                 m_focused           = false;

    // Latched by Hide(), cleared by Layout(). When set, Paint draws nothing so
    // a controller-less machine shows no drive body, LED, or "IN USE" text.
    bool                m_hidden            = false;

    // Marquee state for the mounted-disk basename label. m_marqueeStartMs
    // is when scroll motion begins (in the future during a mount's lead-in
    // delay; "now" on a hover enter). m_marqueePath detects remounts;
    // m_marqueeHovered debounces the hover trigger and gates the replay.
    std::wstring        m_marqueePath;
    int64_t             m_marqueeStartMs    = 0;
    bool                m_marqueeHovered    = false;
};
