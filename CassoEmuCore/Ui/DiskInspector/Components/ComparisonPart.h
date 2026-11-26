#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Ui/DiskInspector/ComparisonSession.h"
#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class DifferencesView;
class FilesView;
class IInspectorComponentHost;
class InspectorSession;
class TrackStripView;
struct ComparisonSource;
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
    ComparisonPart (InspectorSession & session, IInspectorComponentHost & host, DifferencesView & diffsTab, TrackStripView & stripB, FilesView & files);

    void          Create         () override;
    void          Update         () override;
    void          PaintUnder     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    void          PaintOver      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool          OnMouse        (const DxuiMouseEvent & ev) override;
    std::wstring  GetTip         (POINT pointPx, RECT & outAnchorPx) const override;
    void          GetPrefs       (DiskInspectorPrefs & inOut) const override;
    void          OnDiskStarting (const DiskChange & change) override;

    void  Layout           (const RECT & bar, const RECT & notice, const DxuiDpiScaler & scaler);

    //  The height of the comparison's infobar and the margin above it, or 0
    //  while it is hidden.
    int   GetNoticeHeightPx (int widthPx, const DxuiDpiScaler & scaler) const;
    void  TakeComparison    ();
    void  SyncSideB         ();
    void  OpenCompare       ();
    void  CompareWithFile   (const std::wstring & path);
    void  StopComparing     ();
    void  EndComparison     (const std::wstring & why);
    void  StepDifference    (int step);
    void  SwapSides         ();
    void  OpenBSettings     ();

    std::wstring  GetImageName (int side) const;

private:
    void  CheckComparedSources ();
    void  StartComparison      (const ComparisonSource & a, const ComparisonSource & b);
    void  SelectDifference     (const Difference & difference);
    void  PaintComparisonBar   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintSwapArrow       (IDxuiPainter & painter, const IDxuiTheme & theme);
    void  MaximizeForCompare   ();
    void  RestoreAfterCompare  ();
    void  DefaultFilesOverlay  ();
    void  LinkSelections       (const TrackAnalysis * trackA, const TrackAnalysis * trackB);
    void  ShowSelectionOnStrip ();
    void  UpdateSectorsNote    ();
    void  PaintSectorsNote     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);

    std::wstring         GetPath     (int side) const;
    static std::wstring  GetLeafName (const std::wstring & path);
    static int           FindSector  (const TrackAnalysis & track, int sector);

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    DifferencesView          & m_diffsView;
    TrackStripView           & m_stripB;
    FilesView                & m_files;
    DxuiButton               * m_prevDiff          = nullptr;
    DxuiButton               * m_nextDiff          = nullptr;
    DxuiButton               * m_swapButton        = nullptr;
    DxuiButton               * m_stopButton        = nullptr;
    RECT                       m_barPx             = {};
    uint64_t                   m_comparisonVersion = 0;
    int                        m_diffIndex         = -1;
    uint64_t                   m_modelBDisk        = 0;
    const TrackAnalysis      * m_modelBTrack       = nullptr;    // B's track analysis the view model last took
    vector<Difference>         m_nibbleDiffs;
    const TrackAnalysis      * m_diffsOfA          = nullptr;
    const TrackAnalysis      * m_diffsOfB          = nullptr;
    double                     m_bOffset           = 0.0;
    double                     m_linkStartB        = -1.0;
    double                     m_linkSpanB         = 0.0;
    int64_t                    m_sourceCheckMs     = 0;
    std::array<RECT, 2>        m_labelPx           = {};
    int                        m_focusedSide       = ComparisonSession::kSideA;
    float                      m_cursorX           = -1.0f;
    bool                       m_isMaximizedHere   = false;
    bool                       m_isFilesDefaulted  = false;
    bool                       m_isFirstShown      = false;
    int                        m_rotation          = 0;
    std::pair<int, int>        m_linkedSelA        = { -1, 0 };
    std::pair<int, int>        m_linkedSelB        = { -1, 0 };
    int                        m_linkedSecA        = -1;
    int                        m_linkedSecB        = -1;
    DxuiInfoBar                m_sectorsNote;
    bool                       m_isSectorsNoted    = false;
};
