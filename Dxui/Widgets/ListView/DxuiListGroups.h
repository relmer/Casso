#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListGroups
//
//  Groups and lines: the rows as lines under their groups' headers, each
//  group collapsed or open and sliding between, a line's row and group,
//  and the group header's hit tests and paint.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListGroups
{
public:
    explicit DxuiListGroups (DxuiListView & list);

    void                        SetGroups          (std::vector<DxuiListViewTypes::Group> groups);

    void                        SetGroupCollapsed      (int group, bool collapsed);
    void                        SetAllGroupsCollapsed  (bool collapsed);
    bool                        IsGroupCollapsed       (int group) const;

    bool                        IsGroupSliding         ();

    int                         HitTestGroupHeader (int xPx, int yPx) const;

    void                      SelectGroup     (int group);

    //  A press on a group's header, and the header under the pointer.
    bool                      OnPress    (int lx, int ly);
    void                      OnHover    (int lx, int ly) { m_hoverGroup = HitTestGroupHeader (lx, ly); }
    void                      ClearHover () { m_hoverGroup = -1; }
    static constexpr int    s_kGroupChevronCenterDip = 12;   // a group header's chevron, from the list's left, as Explorer's
    static constexpr float  s_kGroupChevronArmDip    = 3.5f;   // half its height
    static constexpr int    s_kListGroupLabelDip     = 8;    // List's, which has no chevron before it
    void  BuildLines     () const;
    int   GetLineOfGroup (int group) const;
    void  EnsureLineVisible (int line);
    bool  HitTestGroupChevron (int xPx, int group) const;
    int          GetGroupOfRow          (int row) const;
    int   GetLineCount   () const;
    int   GetLineOfRow   (int row) const;
    int   GetRowOfLine   (int line) const;     // -1 on a header line
    int   GetGroupOfLine (int line) const;     // the group whose header is on the line, or -1
    int   HitTestLine    (int xPx, int yPx) const;

    void  PaintGroupHeader (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, int group, float x, float rowX, float ry, float layoutW) const;

    static constexpr int64_t  s_kGroupSlideMs = 250;

    static float            EaseGroupSlide     (float t);
    float                   GetGroupSlideShown () const;
    void                    GetGroupRowSpan    (int group, int & start, int & end) const;
    std::vector<DxuiListViewTypes::LineSpot>   PlaceDataLines     (float top, int firstLine, int lastLine, float & clipTop, float & clipH) const;

    //  The group opening or closing, or -1; and ending its slide at once.
    int   GetSlidingGroup () const { return m_groupSlide.group; }
    void  EndGroupSlide   () { m_groupSlide = {}; }

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    //  Each line's row, or -(group + 1) for a header; and each row's line, or
    //  -1 under a collapsed header. Rebuilt when the groups or the row count
    //  change.
    mutable std::vector<int>   m_lines;
    mutable std::vector<int>   m_rowLines;
    mutable bool               m_linesDirty        = true;
    mutable int                m_linesRowCount     = -1;

    std::vector<DxuiListViewTypes::Group>  m_groups;
    int                                    m_hoverGroup      = -1;
    DxuiListViewTypes::GroupSlide          m_groupSlide;
    int                                    m_lastHeaderGroup = -1;   // the header pressed last, which a second press opens or closes
    int64_t                                m_lastHeaderMs    = 0;
};
