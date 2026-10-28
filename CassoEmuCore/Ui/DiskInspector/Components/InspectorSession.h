#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Seams/Win32Clipboard.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
#include "Ui/DiskInspector/ComparisonSession.h"
#include "Ui/DiskInspector/FindSession.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"
#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





//  Which view the keys act on: the one pressed last (FR-029, FR-034, FR-043).
enum class KeyTarget
{
    Platter,
    Strip,
    SectorRow,
    SectorData,
    Nibbles,
    Fields,
    FluxTiming,
    Tracks,
    Findings,
    Image,
    FileList,
    Differences,
    FindBar,
    FindResults,
};


//  The track tabs, then the disk tabs, in the tab strip's order.
enum TrackTab
{
    kTabSectorData,
    kTabNibbles,
    kTabFields,
    kTabFluxTiming,
    kTrackTabCount,
};


enum DiskTab
{
    kTabTracks,
    kTabFindings,
    kTabFileMap,
    kTabImage,
    kTabDifferences,
    kTabFindResults,
    kDiskTabCount,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession
//
//  The inspector's state that more than one of its components uses: the
//  disk's analysis, the selection and view context, the analysis scheduler,
//  the comparison, find and pending edits, the host and the drive. The
//  window has one, and each component accesses it by reference (FR-162).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorSession
{
public:
    InspectorSession  ();
    InspectorSession  (const InspectorSession &)             = delete;
    InspectorSession & operator= (const InspectorSession &)  = delete;

    std::wstring     GetFileName      () const;
    bool             IsWindowDiskA    () const;
    bool             IsWriteProtected () const { return context.drive != nullptr && context.drive->writeProtect.Any(); }
    const FileMap  * GetFileMap       () const;
    bool             IsSectorImage    () const;

    using Levels = std::array<std::shared_ptr<const PlatterRenderer::Levels>, DiskImage::kQuarterTrackCount>;

    const CassoTheme                    * theme            = nullptr;
    IDiskInspectorHost                  * host             = nullptr;
    NullDiskInspectorHost                 nullHost;
    int                                   drive            = 0;
    uint64_t                              pendingRequest   = 0;
    uint64_t                              requestedMediaId = UINT64_MAX;
    DiskAnalysis                          analysis;
    InspectorViewModel                    model;
    InspectorViewContext                  context;
    std::unique_ptr<AnalysisScheduler>    scheduler;
    ComparisonSession                     comparison;
    ComparisonSource                      sourceA;
    InspectorViewModel                    modelB;
    InspectorViewContext                  contextB;
    vector<int>                           diffCells;
    Levels                                levels;
    Levels                                timingLevels;
    std::set<int>                         changedSlots;
    DxuiDpiScaler                         scaler;
    DxuiDpiScaler                         hostScaler;
    std::shared_ptr<const DriveHeadState> headState;
    vector<uint64_t>                      writeCounts;
    std::set<int>                         writtenSlots;
    KeyTarget                             keyTarget        = KeyTarget::Platter;
    Win32Clipboard                        clipboard;
    FindSession                           find;
    FindMarks                             findMarks;
    bool                                  isFindOpen       = false;
    EditController                        edits;
    std::set<int>                         appliedSlots;
    std::wstring                          editNote;
    std::wstring                          statusNote;
    bool                                  isDiskGroup      = false;
    int                                   trackTab         = 0;
    int                                   diskTab          = 0;
    bool                                  isTablesDirty    = true;
    bool                                  isLayoutStale    = false;
    int                                   mapIndex         = 0;
    int                                   mapFile          = -1;
    bool                                  is35             = false;
    std::wstring                          openError;
};
