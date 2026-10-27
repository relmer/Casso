#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
#include "Render/IDxuiTextRenderer.h"
#include "Widgets/DxuiScrollbar.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListFlowLayout
//
//  The List view's layout: items flowing down columns, the columns'
//  positions, each item's rect and hit test, and the group blocks.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListFlowLayout
{
public:
    explicit DxuiListFlowLayout (DxuiListView & list);

    static constexpr float  s_kListGroupColumnTrimDip = 10.67f;
    const std::vector<int> &  GetListColumnLefts () const;
    int           FindListColumn          (int x) const;

    bool          HasListGroups           () const;
    const DxuiListViewTypes::ListLayout &  GetListLayout     () const;
    DxuiListViewTypes::ListLayout    BuildListLayout         () const;
    bool          GetListItemRectPx       (const DxuiListViewTypes::ListLayout & layout, int item, RECT & outRect) const;
    static int    FindListBlock           (const DxuiListViewTypes::ListLayout & layout, int x);
    int           HitTestListItem         (const DxuiListViewTypes::ListLayout & layout, int xPx, int yPx) const;

    std::vector<DxuiListViewTypes::ListColumn>  GetListColumns            () const;
    static int               FindColumnOfItem          (const std::vector<DxuiListViewTypes::ListColumn> & cols, int item);

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    mutable DxuiListViewTypes::ListLayout  m_listLayout;
    mutable DxuiListViewTypes::LayoutKey   m_listLayoutKey;
    mutable bool                           m_listLayoutBuilt = false;
    mutable std::vector<int>               m_listColumnLefts;   // List's column edges, one past the last
    mutable std::array<int, 5>             m_listColumnsKey  = { -1, -1, -1, -1, -1 };   // items per column, name widths, rows version, cell width, DPI
};
