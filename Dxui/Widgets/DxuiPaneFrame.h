#pragma once

#include "Pch.h"
#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFramePhase
//
//  When a frame part is drawn. UNDER parts go down with the group's own
//  paint, ahead of its title and tabs. JOINS go down after the tabs, so the
//  selected tab flares into the line over a hovered neighbor's fill, as
//  Visual Studio draws it. OVER parts are drawn after every sibling has
//  painted, so they lie over the pane's controls.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiPaneFramePhase
{
    Under,
    Joins,
    Over,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameRole
//
//  Which of the frame's colors a part is drawn in.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiPaneFrameRole
{
    Gap,
    Band,
    Content,
    Outline,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameShape
//
//  How a part is drawn: a plain fill, a rounded fill, or a rounded ring.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiPaneFrameShape
{
    Rect,
    RoundedFill,
    Ring,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFramePart
//
//  One piece of a pane's frame, in device pixels. A clipped part is drawn
//  inside its clip only, which is how a quarter of a ring or one corner of a
//  rounded fill is drawn.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPaneFramePart
{
    DxuiPaneFrameShape  shape     = DxuiPaneFrameShape::Rect;
    DxuiPaneFramePhase  phase     = DxuiPaneFramePhase::Under;
    DxuiPaneFrameRole   role      = DxuiPaneFrameRole::Outline;
    bool                clipped   = false;
    RECT                clip      = {};
    float               x         = 0.0f;
    float               y         = 0.0f;
    float               width     = 0.0f;
    float               height    = 0.0f;
    float               radius    = 0.0f;
    float               thickness = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameSpec
//
//  What a frame is built from, in device pixels: the pane's outer rect, its
//  kind, the depth of its title and tab band, the selected tab's span along
//  the band and whether a scroll arrow cuts it off on either side, and the
//  outline's width and outer corner radius.
//
//  A floating pane's corners can be its window's own, which Windows rounds:
//  `windowCorners` holds those corners, as DxuiPaneFrame's kCorner flags,
//  and `windowCornerPx` the window's radius there.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPaneFrameSpec
{
    RECT  pane           = {};
    bool  toolWindow     = false;
    long  titlePx        = 0;
    long  bandPx         = 0;
    bool  hasSelected    = false;
    long  selLeft        = 0;
    long  selRight       = 0;
    bool  openLeft       = false;
    bool  openRight      = false;
    long  linePx         = 1;
    long  cornerPx       = 0;
    UINT  windowCorners  = 0;
    long  windowCornerPx = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrameColors
//
//  The color of each role.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPaneFrameColors
{
    uint32_t  gap     = 0;
    uint32_t  band    = 0;
    uint32_t  content = 0;
    uint32_t  outline = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame
//
//  A docked pane's frame as Visual Studio draws it: one rounded outline
//  around the pane and its selected tab, a tab band across the pane's full
//  width with rounded outer corners, a concave join where the selected tab
//  meets the line along the band, square corners where that line meets the
//  pane's sides, and the gap color outside every rounded corner.
//
//  Every edge is a whole pixel. Every rounded piece is a rounded fill or a
//  ring clipped to the box it belongs in, so no piece paints over another
//  to correct it.
//
//  A corner the pane shares with a rounded window takes the window's radius,
//  so the outline follows the window's edge rather than leaving it, and has
//  no gap color outside it: the window ends there.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPaneFrame
{
public:
    static std::vector<DxuiPaneFramePart>  Build       (const DxuiPaneFrameSpec & spec);
    static RECT                            GetBodyRect (const DxuiPaneFrameSpec & spec);
    static void                            Paint       (IDxuiPainter                          & painter,
                                                        const std::vector<DxuiPaneFramePart>  & parts,
                                                        DxuiPaneFramePhase                      phase,
                                                        const DxuiPaneFrameColors             & colors);

    //  How near a side of the pane a selected tab must end to be drawn flush.
    static long  GetFlushReachPx (long cornerPx, long linePx);

    //  The outer radius a pane of this size is drawn with: none for a pane
    //  too small to round.
    static long  GetCornerPx     (const RECT & pane, long cornerPx);

    //  A pane's corners, for DxuiPaneFrameSpec::windowCorners.
    static constexpr UINT  kCornerTopLeft     = 0x1;
    static constexpr UINT  kCornerTopRight    = 0x2;
    static constexpr UINT  kCornerBottomLeft  = 0x4;
    static constexpr UINT  kCornerBottomRight = 0x8;

private:
    struct Geometry;

    static Geometry  MakeGeometry     (const DxuiPaneFrameSpec & spec);
    static void      BuildDocument    (std::vector<DxuiPaneFramePart> & parts, const Geometry & g);
    static void      BuildToolWindow  (std::vector<DxuiPaneFramePart> & parts, const Geometry & g);
    static void      AddLineRuns      (std::vector<DxuiPaneFramePart> & parts, const Geometry & g);
    static void      AddJoins         (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, DxuiPaneFramePhase phase);
    static void      AddTabOutline    (std::vector<DxuiPaneFramePart> & parts, const Geometry & g);
    static RECT      GetTabRows       (const Geometry & g, long left, long right, long distance, long extent);
    static void      AddRect          (std::vector<DxuiPaneFramePart> & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & rect);
    static void      AddRoundedFill   (std::vector<DxuiPaneFramePart> & parts, DxuiPaneFrameRole role, const RECT & clip, const RECT & rect, long radius);
    static void      AddRing          (std::vector<DxuiPaneFramePart> & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & clip,
                                       const RECT & rect, long radius, long thickness);
    static void      AddQuarterRing   (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, long cx, long cy, const RECT & box);
    static void      AddFillet        (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, long cx, long cy, const RECT & box);
    static void      AddCap           (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, UINT corner);
    static void      AddGapBox        (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, UINT corner);
    static void      AddCornerFill    (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, DxuiPaneFrameRole role, const RECT & clip, bool atTop);
    static void      AddCornerRing    (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, UINT corner);
    static RECT      GetCornerBox     (const Geometry & g, UINT corner);
    static long      GetPaneCornerPx  (const Geometry & g, UINT corner);
    static bool      IsWindowCorner   (const Geometry & g, UINT corner);
    static bool      IsEmpty          (const RECT & rect);
};
