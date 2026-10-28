#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class DifferencesTab;
class FilesView;
class IInspectorComponentHost;
class InspectorSession;
class TrackStripView;
struct TrackAnalysis;





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart
//
//  Comparing two disks: the comparison bar with A's and B's sources and its
//  buttons, B's view of the selected track linked to A's, stepping through
//  the differences, and "Compare with..." (FR-117 to FR-124).
//
////////////////////////////////////////////////////////////////////////////////

class ComparisonPart : public IInspectorComponent
{
public:
    ComparisonPart (InspectorSession & session, IInspectorComponentHost & host, DifferencesTab & diffsTab, TrackStripView & stripB, FilesView & files);

    void          Create     () override;
    void          Update     () override;
    void          PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const override;
    void          OnDiskStarting (const DiskChange & change) override;

    void  Layout           (const RECT & bar, const DxuiDpiScaler & scaler);
    void  TakeComparison   ();
    void  SyncSideB        ();
    void  OpenCompare      ();
    void  StopComparing    ();
    void  EndComparison    (const std::wstring & why);
    void  StepDifference   (int step);
    void  SwapSides        ();
    void  OpenBSettings    ();

private:
    void  CheckComparedSources ();
    void  SelectDifference     (const Difference & difference);
    void  PaintComparisonBar   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);

    InspectorSession           & m_session;
    IInspectorComponentHost    & m_host;
    DifferencesTab             & m_diffsTab;
    TrackStripView             & m_stripB;
    FilesView                  & m_files;
    DxuiButton                 * m_prevDiff          = nullptr;
    DxuiButton                 * m_nextDiff          = nullptr;
    DxuiButton                 * m_swapButton        = nullptr;
    DxuiButton                 * m_bSettingsButton   = nullptr;
    DxuiButton                 * m_stopButton        = nullptr;
    RECT                         m_barPx             = {};
    uint64_t                     m_comparisonVersion = 0;
    int                          m_diffIndex         = -1;
    uint64_t                     m_modelBDisk        = 0;
    vector<Difference>           m_nibbleDiffs;
    const TrackAnalysis        * m_diffsOfA          = nullptr;
    const TrackAnalysis        * m_diffsOfB          = nullptr;
    double                       m_bOffset           = 0.0;
    double                       m_linkStartB        = -1.0;
    double                       m_linkSpanB         = 0.0;
    int64_t                      m_sourceCheckMs     = 0;
};
