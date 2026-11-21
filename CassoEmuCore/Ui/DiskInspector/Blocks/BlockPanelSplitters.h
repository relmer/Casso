#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters
//
//  The four splitters of block mode's panel: between the tree and the
//  middle column, between the middle column and the map, between the list
//  and the block, and between the hex view and the fields. Each splitter's
//  place is a share of the width or height it divides. A drag moves the
//  share, within its range.
//
////////////////////////////////////////////////////////////////////////////////

class BlockPanelSplitters
{
public:
    enum class Bar { None, Tree, Map, List, Hex };

    static constexpr float  kMinSplit  = 0.45f;   // the map's left edge, a share of the width
    static constexpr float  kMaxSplit  = 0.85f;
    static constexpr float  kSplit     = 0.62f;   // the map's left edge at first
    static constexpr float  kTreeShare = 0.18f;   // the tree's share of the width at first
    static constexpr float  kMinTree   = 0.08f;
    static constexpr float  kMaxTree   = 0.35f;
    static constexpr float  kHexShare  = 0.60f;   // the hex view's share of the width beside the fields at first
    static constexpr float  kMinHex    = 0.30f;
    static constexpr float  kMaxHex    = 0.85f;
    static constexpr float  kListShare = 0.45f;   // the list's share of the middle column's height at first
    static constexpr float  kMinList   = 0.15f;
    static constexpr float  kMaxList   = 0.80f;

    //  The x of the tree's right edge and the map's left edge in `bounds`,
    //  with the shares clamped to their ranges first.
    int  PlaceColumns (const RECT & bounds, int & outMapX);

    //  The y of the list's bottom edge in `column`, and the x of the hex
    //  view's right edge, with the share clamped first.
    int  PlaceListEnd (const RECT & column);
    int  PlaceHexEnd  (const RECT & column);

    void          SetBarRect  (Bar bar, const RECT & rect);
    const RECT  & GetBarRect  (Bar bar) const;
    Bar           GetDragged  () const { return m_dragged; }

    //  Sets the hex view's share, within `minShare` and kMaxHex.
    void  FitHex (float share, float minShare);

    //  A press on a bar starts its drag, a move drags it, and a release ends
    //  it. `list` is the list's bounds, the width the hex bar divides and the
    //  top of the height the list bar divides; `footPx` is the bottom of that
    //  height. True when the event is taken; `outIsMoved` is set when a
    //  share changes.
    bool  OnMouse (const DxuiMouseEvent & ev, const RECT & bounds, const RECT & list, int footPx, bool & outIsMoved);

    //  The resize cursor over a bar or during a drag, or null.
    LPCWSTR  GetCursor (POINT pointPx) const;

    void  Paint (IDxuiPainter & painter, const IDxuiTheme & theme) const;

private:
    static size_t  GetIndex (Bar bar) { return (size_t) bar - 1; }

    float                 m_split     = kSplit;
    float                 m_treeShare = kTreeShare;
    float                 m_listShare = kListShare;
    float                 m_hexShare  = kHexShare;
    std::array<RECT, 4>   m_bars      = {};
    Bar                   m_dragged   = Bar::None;
};
