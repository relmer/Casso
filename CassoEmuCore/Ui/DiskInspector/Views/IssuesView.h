#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Ui/DiskInspector/InspectorTableView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView
//
//  Every finding on the disk (FR-048), most severe first, under a row of
//  category toggles that filter the list (FR-049); each toggle has its
//  category's count. A row has the severity's icon, the finding's whole
//  text wrapped to its column, the category, track, sector and cell, and
//  the Repair button. Selecting a finding goes to it.
//
//  A finding a change of the repair plan sets right has a Repair button at
//  the end of its row, which opens Repair's confirmation with those changes
//  checked; "Repair disk image..." at the right of the toggles opens it with
//  every change checked.
//
////////////////////////////////////////////////////////////////////////////////

class IssuesView : public InspectorTableView
{
public:
    using RepairFn = std::function<void (const std::set<std::wstring> & chosen)>;

    static constexpr int     kFilterDip         = 28;
    static constexpr int     kRepairColumnDip   = 76;
    static constexpr int     kRepairAllDip      = 150;
    static constexpr int     kSeverityColumnDip = 72;
    static constexpr int     kSeverityIconDip   = 14;
    static constexpr int     kLineDip           = 22;      // a row's first line, where its icon and button are
    static constexpr size_t  kFindingColumn     = 1;

    explicit IssuesView (InspectorViewContext & context);

    //  Builds the rows again from the analysis, keeping the filter and sort.
    void  Refresh ();

    //  The plan whose changes the Repair buttons offer, the caller's; null for
    //  none.
    void  SetRepairPlan (const RepairPlan * plan) { m_plan = plan; }
    void  SetOnRepair   (RepairFn repair)         { m_repair = std::move (repair); }

    //  The finding at that index of the analysis's findings selected and
    //  scrolled into view. Its category is turned back on in the filter if it
    //  was off.
    void  ShowFinding (int finding);

    //  The changes that set the finding right, empty when none does.
    std::set<std::wstring>  GetFixingKeys (int finding) const;

    //  Where a row's Repair button is, empty for a row without one or out of
    //  view, and where "Repair disk image..." is; for a test.
    RECT  GetRepairRect    (int row) const;
    RECT  GetSeverityRect  (int row) const;
    RECT  GetRepairAllRect () const;

    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

protected:
    int   GetTopHeightPx () const override;
    void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top) override;

private:
    void  OnSort         (int column);
    RECT  GetToggleRect  (int category) const;
    void  PaintButton    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & rect, LPCWSTR label, bool isHot) const;

    uint32_t                                         m_mask         = InspectorTables::GetAllCategories();
    int                                              m_sortColumn   = -1;
    bool                                             m_isDescending = false;
    std::array<int, InspectorTables::kCategoryCount> m_counts       = {};
    const RepairPlan                               * m_plan         = nullptr;
    RepairFn                                         m_repair;
    int                                              m_hotRow       = -1;     // the row whose button the pointer is over; -2 for "Repair disk image..."
    int                                              m_pressedRow   = -1;
};
