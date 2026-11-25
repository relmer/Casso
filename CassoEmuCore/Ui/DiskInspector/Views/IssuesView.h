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
//  category's count. A row has the severity's icon, the finding's text on
//  one line, and the category, track, sector and cell. The list scrolls
//  sideways when the columns are wider than the view.
//  Selecting a finding goes to it.
//
//  Over the list, while the volume is damaged, an infobar has the first
//  finding, and Repair... opens Repair's confirmation with
//  every change checked. While the volume can't be repaired, the infobar
//  has the reason in place of the button.
//
////////////////////////////////////////////////////////////////////////////////

class IssuesView : public InspectorTableView
{
public:
    using RepairFn = std::function<void (const std::set<std::wstring> & chosen)>;

    static constexpr int     kFilterDip         = 28;
    static constexpr int     kSeverityIconDip   = 14;
    static constexpr int     kSeverityColumnDip = 28;      // the severity column, as wide as its icon and a margin
    static constexpr size_t  kCellColumn        = 5;
    static constexpr int     kBannerGapDip      = 6;
    static constexpr size_t  kFindingColumn     = 1;

    explicit IssuesView (InspectorViewContext & context);

    //  Builds the rows again from the analysis, keeping the filter and sort.
    void  Refresh ();

    //  The plan Repair... offers, the caller's; null for none.
    void  SetRepairPlan (const RepairPlan * plan) { m_plan = plan; }
    void  SetOnRepair   (RepairFn repair)         { m_repair = std::move (repair); }

    //  The finding at that index of the analysis's findings selected and
    //  scrolled into view. Its category is turned back on in the filter if it
    //  was off.
    void  ShowFinding (int finding);

    //  Where a row's severity icon is, empty out of view, and the infobar;
    //  for a test.
    RECT                    GetSeverityRect (int row) const;
    DxuiInfoBanner        & GetBanner       () { return m_banner; }
    const DxuiInfoBanner  & GetBanner       () const { return m_banner; }

    //  The infobar's text: the first finding and, when Repair
    //  isn't available, the reason.
    static std::wstring  FormatBanner (const DiskAnalysis & analysis, const RepairPlan & plan);

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

protected:
    int   GetTopHeightPx () const override;
    void  PaintTop       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top) override;

private:
    void  OnSort         (int column);
    RECT  GetToggleRect  (int category) const;
    bool  HasBanner      () const;
    void  UpdateBanner   ();

    uint32_t                                         m_mask         = InspectorTables::GetAllCategories();
    int                                              m_sortColumn   = -1;
    bool                                             m_isDescending = false;
    std::array<int, InspectorTables::kCategoryCount> m_counts       = {};
    const RepairPlan                               * m_plan         = nullptr;
    RepairFn                                         m_repair;
    DxuiInfoBanner                                   m_banner;
    int                                              m_bannerPx     = 0;       // the infobar's height and the gap under it
};
