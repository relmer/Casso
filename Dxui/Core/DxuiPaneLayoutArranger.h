#pragma once

#include "Pch.h"
#include "DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger
//
//  Lays a layout's tree out over an area: the rect of each group with a
//  visible pane, and the position of each split with visible panes on both
//  sides. Callbacks from the application report which panes are visible and
//  each pane's minimum size.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPaneLayoutArranger
{
public:
    using Node      = DxuiPaneLayoutNode;
    using GroupRect = DxuiPaneLayout::GroupRect;
    using SplitRect = DxuiPaneLayout::SplitRect;
    using ShownFn   = DxuiPaneLayout::ShownFn;
    using MinSizeFn = DxuiPaneLayout::MinSizeFn;

    DxuiPaneLayoutArranger (const ShownFn & shown, const MinSizeFn & minSize) : m_shown (shown), m_minSize (minSize) {}

    std::vector<GroupRect>  Arrange       (const Node * root, const RECT & areaDip) const;
    std::vector<SplitRect>  ArrangeSplits (const Node * root, const RECT & areaDip) const;

private:
    bool  IsShown          (const std::wstring & pane) const { return !m_shown || m_shown (pane); }
    bool  HasShown         (const Node * node) const;
    SIZE  GetMinimum       (const Node * node) const;
    long  GetSplitPosition (const Node * node, const RECT & area) const;
    void  ArrangeNode      (const Node * node, const RECT & area, std::vector<GroupRect> & out) const;
    void  ArrangeSplitsIn  (const Node * node, const RECT & area, const std::wstring & path, std::vector<SplitRect> & out) const;

    const ShownFn    & m_shown;
    const MinSizeFn  & m_minSize;
};
