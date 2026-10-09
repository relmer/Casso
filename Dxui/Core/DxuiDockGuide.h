#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiIconImage.h"
#include "Core/DxuiPaneLayout.h"
#include "Render/DxuiCoverageRaster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuideKind / DxuiDockGuideButton / DxuiDockGuideColors
//
//  The guides Visual Studio shows while a pane is dragged: a small cross over
//  a tool window group, a large one over a document group, whose inner ring
//  of split buttons makes a new tab group beside it, and a box holding one
//  dock button at the middle of each window edge.
//
//  The center button tabs the pane into the group, a dock button docks it on
//  that side, and a split button splits the group on that side. The colors
//  are the theme's DockGuide roles at full strength, as the button under the
//  pointer shows them; every other button shows them fainter.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiDockGuideKind
{
    SmallCross,
    LargeCross,
    Edge,
};

enum class DxuiDockGuideButton
{
    Center,
    DockTop,
    DockBottom,
    DockLeft,
    DockRight,
    SplitTop,
    SplitBottom,
    SplitLeft,
    SplitRight,
};

struct DxuiDockGuideColors
{
    uint32_t  border       = 0;
    uint32_t  fill         = 0;
    uint32_t  buttonBorder = 0;
    uint32_t  buttonFill   = 0;
    uint32_t  glyph        = 0;
    uint32_t  arrow        = 0;

    bool operator== (const DxuiDockGuideColors &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuide
//
//  The size, the buttons and the picture of each guide, measured from Visual
//  Studio at 125%. Every measure is in DIP from the guide's top left. Each
//  position in pixels comes from ToPx of its offset there, so the arms, the
//  buttons, the pictures' frames and the arrows' corners land on whole
//  pixels; a depth or a width (the border ring, the title band, a dot) is
//  scaled exactly, and the diagonals, rounded corners, dots and arrows are
//  anti-aliased. A guide's origin is its top left, in the same pixels as the
//  point it is placed by.
//
//  As Visual Studio draws them, the button under the pointer is opaque, and
//  every other button is drawn at kRestAlpha over the cross, whose own fill
//  and border never fade. An edge guide whose button is not under the
//  pointer is drawn at kRestAlpha whole, its box with its button.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockGuide
{
public:
    //  A button's side, and the distance from one button to the next.
    static constexpr int  kButtonDip = 32;
    static constexpr int  kPitchDip  = 36;

    //  How opaque a button, or an edge guide, is while the pointer is not
    //  on it: Visual Studio's 70%.
    static constexpr int  kRestAlpha = 0xB3;

    static SIZE   GetSizePx         (DxuiDockGuideKind kind, const DxuiDpiScaler & scaler);
    static POINT  GetOrigin         (DxuiDockGuideKind kind, POINT centerPx, const DxuiDpiScaler & scaler);
    static POINT  GetOriginOfButton (DxuiDockGuideKind kind, DxuiDockGuideButton button, const RECT & buttonPx, const DxuiDpiScaler & scaler);
    static RECT   GetButtonRect     (DxuiDockGuideKind kind, DxuiDockGuideButton button, POINT originPx, const DxuiDpiScaler & scaler);
    static bool   IsInside          (DxuiDockGuideKind kind, POINT originPx, POINT point, const DxuiDpiScaler & scaler);

    static std::vector<DxuiDockGuideButton>  GetButtons (DxuiDockGuideKind kind, DxuiDockSide edge);

    static DxuiDockGuideButton  GetDockButton  (DxuiDockSide side);
    static DxuiDockGuideButton  GetSplitButton (DxuiDockSide side);

    //  The picture of a guide, with `hoveredButton`, a DxuiDockGuideButton or
    //  -1 for none, drawn opaque and every other button at kRestAlpha.
    static DxuiIconImage  Render (DxuiDockGuideKind kind, DxuiDockSide edge, int hoveredButton, const DxuiDockGuideColors & colors, const DxuiDpiScaler & scaler);

    //  The parts of the picture, in pixels from the guide's top left.
    static std::vector<DxuiPointF>        GetOutline   (DxuiDockGuideKind kind, const DxuiDpiScaler & scaler);
    static RECT                           GetGlyphRect (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler);
    static float                          GetBandPx    (const DxuiDpiScaler & scaler);
    static std::vector<DxuiPointF>        GetArrow     (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler);
    static std::vector<DxuiCoverageRect>  GetDots      (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler);

private:
    static constexpr int    kInsetDip         = 4;
    static constexpr int    kSmallSlots       = 3;
    static constexpr int    kLargeSlots       = 5;
    static constexpr int    kEdgeSlots        = 1;
    static constexpr int    kArmDip           = kPitchDip + kInsetDip;

    //  How far each chamfer runs along the arms, as Visual Studio's guides
    //  are measured at 125%: 12.5 to 13 px on the small cross, 11.6 on the
    //  large one.
    static constexpr float  kSmallChamferDip  = 10.0f;
    static constexpr float  kLargeChamferDip  = 9.25f;

    //  The cross's border ring is exactly 1 DIP, not snapped to a whole
    //  pixel, and a button's corners are rounded 3.0 to 3.25 px at 125%.
    static constexpr float  kBorderDip        = 1.0f;
    static constexpr float  kButtonRadiusDip  = 2.5f;
    static constexpr int    kGlyphDip         = 24;
    static constexpr int    kGlyphHalfDip     = 12;
    static constexpr float  kBandDip          = 3.0f;
    static constexpr float  kGlyphRadiusDip   = 1.8f;
    static constexpr int    kDotDip           = 1;
    static constexpr int    kDotPitchDip      = 2;
    static constexpr int    kDotNearDip       = 11;
    static constexpr int    kDotFarDip        = 12;
    static constexpr int    kArrowHalfBaseDip = 4;
    static constexpr int    kArrowApexDip     = 22;
    static constexpr int    kArrowBaseDip     = 26;

    static int               GetSlots        (DxuiDockGuideKind kind);
    static bool              TryGetButtonDip (DxuiDockGuideKind kind, DxuiDockGuideButton button, POINT & dip);
    static RECT              GetGlyphDip     (DxuiDockGuideButton button);
    static DxuiCoverageRect  GetButtonBox    (DxuiDockGuideKind kind, DxuiDockGuideButton button, const DxuiDpiScaler & scaler);
    static DxuiCoverageRect  GetBorderHole   (const DxuiCoverageRect & rect);
    static void              PaintGlyph      (DxuiIconImage & image, DxuiDockGuideKind kind, DxuiDockGuideButton button, uint32_t argb, const DxuiDpiScaler & scaler);
    static void              PaintButtons    (DxuiIconImage & image, DxuiDockGuideKind kind, const std::vector<DxuiDockGuideButton> & buttons,
                                              const DxuiDockGuideColors & colors, const DxuiDpiScaler & scaler);
};
