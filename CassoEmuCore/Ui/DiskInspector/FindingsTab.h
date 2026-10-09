#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorTableView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindingsTab
//
//  Every finding on the disk (FR-048), in quarter-track and cell order until
//  a column header sorts it, under a row of category toggles that each give
//  their count and filter the list (FR-049). Selecting a finding goes to it.
//
////////////////////////////////////////////////////////////////////////////////

class FindingsTab : public InspectorTableView
{
public:
    static constexpr int  kFilterDip = 28;

    explicit FindingsTab (InspectorViewContext & context);

    //  Builds the rows again from the analysis, keeping the filter and sort.
    void  Refresh ();

    bool  OnMouse (const DxuiMouseEvent & ev) override;

protected:
    int   GetTopHeightPx () const override;
    void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top) override;

private:
    void  OnSort         (int column);
    RECT  GetToggleRect  (int category) const;

    uint32_t                                         m_mask         = InspectorTables::GetAllCategories();
    int                                              m_sortColumn   = -1;
    bool                                             m_isDescending = false;
    std::array<int, InspectorTables::kCategoryCount> m_counts       = {};
};
