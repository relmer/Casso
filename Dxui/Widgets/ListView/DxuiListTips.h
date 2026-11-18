#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
struct DxuiTip;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListTips
//
//  The tips over Details' headings and cells: the column under a point,
//  the tip the heading or cell has, and where a cell's text is drawn, for a
//  host that lays a tip over it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListTips
{
public:
    explicit DxuiListTips (DxuiListView & list);

    int   HitTestColumn        (int xPx) const;
    bool  TryGetCellTip        (POINT pointPx, DxuiTip & tip) const;
    bool  TryGetCellTextClipPx (int row, size_t column, float & left, float & right) const;

private:
    DxuiListView  & m_list;
};