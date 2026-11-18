#pragma once

#include "Pch.h"
#include "Render/DxuiCoverageRaster.h"
#include "Core/DxuiDockDropZones.h"
#include "Core/DxuiDockGuide.h"
#include "Core/DxuiIconImage.h"


class DxuiDockGroups;
class DxuiDockPanes;
class DxuiDpiScaler;
class DxuiPaneLayout;
class IDxuiDockDragHost;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDragMark
//
//  One thing a drag shows over the site: the area the hovered target would
//  give the panes, a hovered strip's tint and gap, or a drop guide. A mark
//  with an `image` is that picture, its top left at the rectangle's top left
//  and the rectangle its bounds. Otherwise `outlinePx` of 0 fills the
//  rectangle, and any other width outlines it, in dots of that size when
//  `dotted`.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockDragMark
{
    RECT                                  rect      = {};
    uint32_t                              argb      = 0;
    int                                   outlinePx = 0;
    bool                                  dotted    = false;
    std::shared_ptr<const DxuiIconImage>  image;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockDrag
//
//  A pane, or a whole group, dragged over a dock site: the drop zones and
//  guides drawn for it, the target under the pointer, and the drop. A drop
//  on a group's tabs or title bar goes into the group, at a gap opened where
//  the tab would land; elsewhere it goes to the guide button under the
//  pointer.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockDrag
{
public:
    //  The gap a hovered strip opens for the dropped tab, and the tab a tab
    //  drop's shade shows at the start of the target's tabs: Visual Studio's
    //  125 px at 125%.
    static constexpr int  kInsertGapDip  = 96;
    static constexpr int  kPreviewTabDip = 100;

    DxuiDockDrag (IDxuiDockDragHost     & host,
                  DxuiPaneLayout        & layout,
                  const DxuiDpiScaler   & scaler,
                  const DxuiDockGroups  & groups,
                  const DxuiDockPanes   & panes) : m_host (host), m_layout (layout), m_scaler (scaler), m_groups (groups), m_sitePanes (panes) {}

    void  Begin       (const std::vector<std::wstring> & panes, const std::wstring & active);
    bool  End         (POINT pointDip);
    void  Cancel      ();
    void  Track       (POINT pointDip);
    void  ClearTarget ();

    bool                               IsDragging       () const { return !m_pane.empty(); }
    const std::wstring               & GetPane          () const { return m_pane; }
    const std::vector<std::wstring>  & GetPanes         () const { return m_panes; }
    bool                               IsPointerDrag    () const { return m_isPointerDrag; }
    void                               SetPointerDrag   (bool on) { m_isPointerDrag = on; }
    int                                GetStripGroup    () const { return m_stripGroup; }
    int                                GetStripIndex    () const { return m_stripIndex; }
    const DxuiDockDropZone           * GetHoveredZone   () const;
    std::vector<DxuiDockDragMark>      GetMarks         (const IDxuiTheme & theme) const;

    //  The filled rectangles that draw an outlined mark, solid or dotted.
    static std::vector<RECT>  GetOutlineStrips (const DxuiDockDragMark & mark);

private:
    //  Guide pictures already drawn, for each look of each guide.
    struct GuideKey
    {
        DxuiDockGuideKind              kind      = DxuiDockGuideKind::SmallCross;
        DxuiDockSide                   edge      = DxuiDockSide::Left;
        int                            hovered   = -1;
        UINT                           dpi       = 0;
        DxuiDockGuideColors            colors;
        std::vector<DxuiCoverageRect>  shades;
        uint32_t                       shadeArgb = 0;

        bool operator== (const GuideKey &) const = default;
    };

    struct GuideImage
    {
        GuideKey                              key;
        std::shared_ptr<const DxuiIconImage>  image;
    };

    static constexpr size_t  kGuideCacheMax = 32;

    void   UpdateStripTarget (POINT pointDip);
    void   ClearStripTarget  ();
    bool   DropOnStrip       (int group, int index);
    bool   DropOnZone        (const DxuiDockDropZone & zone, DxuiPaneLayout & layout) const;
    void   SetDropPreview    (DxuiDockDropZone & zone) const;
    void   SetTabDropPreview (DxuiDockDropZone & zone) const;
    void   UpdateCompass     (POINT pointDip);
    int    HitTestShown      (POINT pointDip) const;
    bool   TryGetCompass     (int group, DxuiDockGuideKind & kind, POINT & origin) const;

    //  A guide's picture, kept so every frame draws the same buffer, with the
    //  drop's shade laid over it within `shades`, given from its top left.
    std::shared_ptr<const DxuiIconImage>  GetGuideImage (DxuiDockGuideKind kind, DxuiDockSide edge, int hovered,
                                                         const std::vector<DxuiCoverageRect> & shades, const IDxuiTheme & theme) const;

    static bool  Contains (const RECT & rect, POINT point);

    IDxuiDockDragHost              & m_host;
    DxuiPaneLayout                 & m_layout;
    const DxuiDpiScaler            & m_scaler;
    const DxuiDockGroups           & m_groups;
    const DxuiDockPanes            & m_sitePanes;
    std::wstring                     m_pane;
    std::vector<std::wstring>        m_panes;
    int                              m_stripGroup    = -1;
    int                              m_stripIndex    = -1;
    std::vector<DxuiDockDropZone>    m_zones;
    int                              m_hoverZone     = -1;
    int                              m_compassGroup  = -1;
    bool                             m_isPointerDrag = false;
    mutable std::vector<GuideImage>  m_guideImages;
};
