#pragma once

#include "Pch.h"
#include "CassoTheme.h"
#include "../TapeDeckState.h"





enum class TapeDeckRegion
{
    None,
    Name,
    Rewind,
    FastForward,
    Play,
    Stop,
    Record,
    Eject,
    Counter,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeckWidget
//
//  The flat 2D cassette recorder in the drive band, built the way the drive
//  widgets are: a "TAPE" caption to the left, the tape's name as the control
//  that opens the picker, and a rail under it that fills with the position
//  through the tape. Under the rail, and no wider than it, sit the transport
//  buttons and the tape counter; clicking the counter sets the position. The
//  skeuomorphic theme draws the recorder in the desk scene instead.
//
////////////////////////////////////////////////////////////////////////////////

class TapeDeckWidget : public IDxuiControl
{
public:
    void            Hide           ();
    void            SyncFromView   (const TapeDeckView & view) { m_view = view; }
    bool            UpdateHover    (int x, int y);

    void            Paint          (IDxuiPainter        & painter,
                                    IDxuiTextRenderer   & text,
                                    const IDxuiTheme    & theme) override;
    void            Layout         (const RECT          & boundsDip,
                                    const DxuiDpiScaler & scaler) override;

    TapeDeckRegion  HitTest        (int x, int y) const;
    RECT            GetOuterRect   () const;
    RECT            GetNameRect    () const { return m_nameRect; }
    RECT            GetCounterRect () const { return m_counterRect; }
    RECT            GetButtonRect  (TapeDeckRegion region) const;
    bool            IsHidden       () const { return m_hidden; }
    TapeDeckRegion  GetHover       () const { return m_hover; }
    bool            IsMagnifying   () const;

    static bool          IsRegionEnabled (TapeDeckRegion region, const TapeDeckView & view);
    static std::wstring  FormatTime      (double seconds);
    static std::wstring  FormatCounter   (const TapeDeckView & view);
    static bool          ParseTime       (const std::wstring & text, double & seconds);
    static float         GetProgress     (const TapeDeckView & view);
    static std::wstring  GetDisplayName  (const TapeDeckView & view);

    // How far a name too long for its row has scrolled at nowMs, for a scroll
    // that began at startMs and travels one name-plus-gap period. Zero before
    // it begins and once it has finished, which is where the name rests.
    static float         GetMarqueeOffset (int64_t nowMs, int64_t startMs, float periodPx, float speedPxPerSec);

    // The dock's magnification for a control whose center is `distance` from
    // the pointer: kMagnifyMax right under it, easing to 1 at `reach`.
    static float         GetMagnification (float distance, float reach);

    static constexpr float  kMagnifyMax = 2.0f;

    // The transport keys left to right, as the RQ-309DS has them.
    static TapeDeckRegion  GetButtonRegion (size_t index);

    // What the control at `index` does, as its label says it -- the buttons
    // left to right, then the counter.
    static const wchar_t * GetControlLabel (size_t index);

    static constexpr size_t  kButtonCount = 6;

private:
    // A control as drawn this moment. Kept in floating point all the way to
    // the painter: rounded to whole pixels, a control growing a fraction of a
    // pixel a frame stood still and then jumped, and the magnification
    // stuttered.
    struct ControlBox
    {
        float  left   = 0.0f;
        float  top    = 0.0f;
        float  right  = 0.0f;
        float  bottom = 0.0f;

        bool  Contains (int x, int y) const { return (float) x >= left && (float) x < right && (float) y >= top && (float) y < bottom; }
    };

    static constexpr int              kBaseDpi           = 96;
    static constexpr const wchar_t  * kFontFamily        = DxuiTheme::kBodyFace;
    static constexpr const wchar_t  * kCaption           = L"TAPE";
    static constexpr const wchar_t  * kEmptyLabel        = L"(empty)";

    // Matched to the drive widgets so the band reads as one row of devices.
    static constexpr int    kNameWidthPx       = 140;
    static constexpr int    kNameHeightPx      = 20;
    static constexpr int    kRailGapPx         = 3;
    static constexpr int    kRailHeightPx      = 5;
    static constexpr int    kCaptionWidthPx    = 44;
    static constexpr int    kCaptionGapXPx     = 6;
    static constexpr int    kCaptionHeightPx   = 14;
    static constexpr int    kCaptionDescentPx  = 2;
    static constexpr float  kCaptionFontDip    = 9.0f;
    static constexpr float  kNameFontDip       = 11.0f;
    static constexpr float  kCounterFontDip    = 9.0f;

    // The drive widgets' marquee timing, so a long tape name scrolls exactly
    // as a long disk name does: at once when the pointer arrives, then again
    // after a hold for as long as it stays.
    static constexpr int64_t  kMarqueeHoldMs         = 2000;

    // The controls magnify like the macOS dock as the pointer nears them,
    // reaching this many button pitches either side, and fade in and out this fast.
    static constexpr float    kMagnifyReachPitches   = 2.0f;
    static constexpr int64_t  kMagnifyFadeMs         = 140;
    static constexpr float    kMarqueeSpeedDipPerSec = 45.0f;
    static constexpr float    kMarqueeGapDip         = 25.0f;
    // The transport row and counter, under the rail and within its width.
    static constexpr int    kControlsGapYPx    = 5;
    static constexpr int    kButtonSizePx      = 16;
    static constexpr int    kButtonGapPx       = 2;
    static constexpr int    kCounterGapXPx     = 4;
    static constexpr int    kLabelStripPx      = 13;      // under the controls, for the nearest one's name
    static constexpr float  kLabelFontDip      = 8.5f;
    static constexpr int    kLabelWidthPx      = 90;
    static constexpr float  kMarkInsetRatio    = 0.28f;   // of the button, around each drawn mark
    static constexpr float  kDisabledAlpha     = 0.30f;
    static constexpr float  kArmedAlpha        = 0.55f;
    static constexpr float  kRailTrackAlpha    = 0.14f;
    static constexpr float  kRailFillAlpha     = 0.60f;

    static constexpr uint32_t  kRecordRedArgb = 0xFFE04848;

    static int   Scale          (int value, UINT dpi);
    static bool     IsPointInRect  (const RECT & rect, int x, int y);
    static int64_t  GetNowMs       ();

    void      PaintRail    (IDxuiPainter & painter, const CassoTheme & theme);
    void      PaintButtons (IDxuiPainter & painter, const CassoTheme & theme, const ControlBox (& rects)[kButtonCount + 1]);
    float     GetPresence  (int64_t nowMs) const;
    void      ComputeControlRects (int64_t nowMs, ControlBox (& rects)[kButtonCount + 1], float (& scales)[kButtonCount + 1]) const;
    bool      IsNearControls (int x, int y) const;
    void      PaintMark    (IDxuiPainter & painter, TapeDeckRegion region, const ControlBox & box, uint32_t argb);
    uint32_t  GetMarkColor (TapeDeckRegion region, const CassoTheme & theme) const;
    void      PaintName    (IDxuiTextRenderer & text, const std::wstring & name, uint32_t argb);

    TapeDeckView    m_view;
    RECT            m_captionRect           = {};
    RECT            m_nameRect              = {};
    RECT            m_bandRect              = {};
    RECT            m_railRect              = {};
    RECT            m_counterRect           = {};
    RECT            m_buttons[kButtonCount] = {};
    TapeDeckRegion  m_hover                 = TapeDeckRegion::None;
    UINT            m_dpi                   = kBaseDpi;
    bool            m_hidden                = true;
    std::wstring    m_marqueeName;                          // the name the schedule below is for
    int64_t         m_marqueeStartMs        = 0;            // when its scroll begins
    int             m_mouseX                = INT_MIN / 2;  // the pointer, for the magnification
    int             m_mouseY                = INT_MIN / 2;
    bool            m_isNear                = false;        // the pointer is over the controls
    float           m_presenceFrom          = 0.0f;         // the magnification's strength when that last changed
    int64_t         m_presenceMs            = 0;            // and when
};
