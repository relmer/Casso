#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/ComparisonSession.h"
#include "Ui/DiskInspector/InspectorTableView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab
//
//  Every difference the comparison lists (FR-121), under a row of group
//  toggles that each give their count and filter the list, as the Findings
//  tab's categories do, and a row with the three options that leave
//  differences out (FR-119, FR-120). With none, a line says "No
//  differences". Selecting a difference goes to it.
//
////////////////////////////////////////////////////////////////////////////////

class DifferencesTab : public InspectorTableView
{
public:
    static constexpr int  kRowDip = 28;

    DifferencesTab (InspectorViewContext & context, ComparisonSession & session);

    //  Builds the rows again from the comparison, keeping the filter and sort.
    void  Refresh ();

    //  The listed difference a row refers to, or null.
    const Difference *  GetDifference (const TableRow & row) const;

    bool  OnMouse (const DxuiMouseEvent & ev) override;

protected:
    int   GetTopHeightPx () const override;
    void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top) override;

private:
    void  OnSort        (int column);
    RECT  GetToggleRect (int group) const;
    RECT  GetOptionRect (int option) const;
    bool  IsOptionOn    (int option) const;
    void  ToggleOption  (int option);

    static constexpr int  kOptionCount = 3;

    ComparisonSession                               & m_session;
    uint32_t                                          m_mask         = ComparisonText::GetAllGroups();
    int                                               m_sortColumn   = -1;
    bool                                              m_isDescending = false;
    std::array<int, ComparisonText::kGroupCount>      m_counts       = {};
};
