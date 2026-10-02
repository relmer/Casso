#pragma once

#include "Pch.h"
#include "CassoTheme.h"
#include "../TapeDeckState.h"





enum class TapeDeckRegion
{
    None,
    Name,
    Rewind,
    Play,
    Stop,
    Record,
    Eject,
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeckWidget
//
//  The flat 2D cassette recorder in the drive band, built the way the drive
//  widgets are: a "TAPE" caption to the left, the tape's name as the control
//  that opens the picker, and a rail under it. The rail fills with the
//  position through the tape. A row of transport buttons and an "m:ss / m:ss"
//  readout sit to the right of the name. The skeuomorphic theme draws the
//  recorder in the desk scene instead.
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
    RECT            GetButtonRect  (TapeDeckRegion region) const;
    bool            IsHidden       () const { return m_hidden; }

    static bool          IsRegionEnabled (TapeDeckRegion region, const TapeDeckView & view);
    static std::wstring  FormatTime      (double seconds);
    static std::wstring  FormatReadout   (const TapeDeckView & view);
    static float         GetProgress     (const TapeDeckView & view);
    static std::wstring  GetDisplayName  (const TapeDeckView & view);

    // How far a name too long for its row has scrolled at nowMs, for a scroll
    // that began at startMs and travels one name-plus-gap period. Zero before
    // it begins and once it has finished, which is where the name rests.
    static float         GetMarqueeOffset (int64_t nowMs, int64_t startMs, float periodPx, float speedPxPerSec);

    static constexpr size_t  kButtonCount = 5;

private:
    static constexpr int              kBaseDpi           = 96;
    static constexpr const wchar_t  * kFontFamily        = DxuiTheme::kBodyFace;
    static constexpr const wchar_t  * kCaption           = L"TAPE";
    static constexpr const wchar_t  * kEmptyLabel        = L"(empty)";

    // Matched to the drive widgets so the band reads as one row of devices.
    static constexpr int    kNameWidthPx       = 140;
    static constexpr int    kNameHeightPx      = 20;
    static constexpr int    kRailGapPx         = 3;
    static constexpr int    kRailHeightPx      = 5;
    static constexpr int    kBottomPadPx       = 8;
    static constexpr int    kCaptionWidthPx    = 44;
    static constexpr int    kCaptionGapXPx     = 6;
    static constexpr int    kCaptionHeightPx   = 14;
    static constexpr int    kCaptionDescentPx  = 2;
    static constexpr float  kCaptionFontDip    = 9.0f;
    static constexpr float  kNameFontDip       = 11.0f;
    static constexpr float  kReadoutFontDip    = 9.0f;

    // The drive widgets' marquee timing, so a long tape name scrolls exactly
    // as a long disk name does: a hold before the first scroll and between
    // replays while the pointer stays over the name.
    static constexpr int64_t  kMarqueeHoldMs         = 2000;
    static constexpr float    kMarqueeSpeedDipPerSec = 45.0f;
    static constexpr float    kMarqueeGapDip         = 25.0f;
    // The transport row, right of the name.
    static constexpr int    kButtonsGapXPx     = 8;
    static constexpr int    kButtonSizePx      = 20;
    static constexpr int    kButtonGapPx       = 2;
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
    void      PaintButtons (IDxuiPainter & painter, const CassoTheme & theme);
    void      PaintMark    (IDxuiPainter & painter, TapeDeckRegion region, const RECT & box, uint32_t argb);
    uint32_t  GetMarkColor (TapeDeckRegion region, const CassoTheme & theme) const;
    void      PaintName    (IDxuiTextRenderer & text, const std::wstring & name, uint32_t argb);
    static TapeDeckRegion  GetButtonRegion (size_t index);

    TapeDeckView    m_view;
    RECT            m_captionRect           = {};
    RECT            m_nameRect              = {};
    RECT            m_bandRect              = {};
    RECT            m_railRect              = {};
    RECT            m_readoutRect           = {};
    RECT            m_buttons[kButtonCount] = {};
    TapeDeckRegion  m_hover                 = TapeDeckRegion::None;
    UINT            m_dpi                   = kBaseDpi;
    bool            m_hidden                = true;
    std::wstring    m_marqueeName;                          // the name the schedule below is for
    int64_t         m_marqueeStartMs        = 0;            // when its scroll begins
};
