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
//  text wrapped to its column, and the category, track, sector and cell.
//  Selecting a finding goes to it.
//
//  Under the list, Repair disk image... opens Repair's confirmation with
//  every change checked while the volume can be repaired, and the reason it
//  can't takes its place while it is damaged but can't be.
//
////////////////////////////////////////////////////////////////////////////////

class IssuesView : public InspectorTableView
{
public:
    using RepairFn = std::function<void (const std::set<std::wstring> & chosen)>;

    static constexpr int     kFilterDip         = 28;
    static constexpr int     kSeverityColumnDip = 72;
    static constexpr int     kSeverityIconDip   = 14;
    static constexpr int     kButtonBandDip     = 52;      // the band under the list for Repair disk image...
    static constexpr int     kButtonWideDip     = 200;
    static constexpr int     kButtonHighDip     = 32;
    static constexpr size_t  kFindingColumn     = 1;

    explicit IssuesView (InspectorViewContext & context);

    //  Builds the rows again from the analysis, keeping the filter and sort.
    void  Refresh ();

    //  The plan Repair disk image... offers, the caller's; null for none.
    void  SetRepairPlan (const RepairPlan * plan) { m_plan = plan; }
    void  SetOnRepair   (RepairFn repair)         { m_repair = std::move (repair); }

    //  The finding at that index of the analysis's findings selected and
    //  scrolled into view. Its category is turned back on in the filter if it
    //  was off.
    void  ShowFinding (int finding);

    //  Where a row's severity icon is, empty out of view, and Repair disk
    //  image...; for a test.
    RECT                GetSeverityRect (int row) const;
    const DxuiButton  & GetRepairButton () const { return m_repairButton; }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

protected:
    int   GetTopHeightPx () const override;
    void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top) override;

private:
    void  OnSort         (int column);
    RECT  GetToggleRect  (int category) const;
    bool  HasButtonBand  () const;

    uint32_t                                         m_mask         = InspectorTables::GetAllCategories();
    int                                              m_sortColumn   = -1;
    bool                                             m_isDescending = false;
    std::array<int, InspectorTables::kCategoryCount> m_counts       = {};
    const RepairPlan                               * m_plan         = nullptr;
    RepairFn                                         m_repair;
    DxuiButton                                       m_repairButton { L"Repair disk image..." };
    RECT                                             m_bandPx       = {};
};
