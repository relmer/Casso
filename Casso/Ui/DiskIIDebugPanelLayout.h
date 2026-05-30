#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanelLayout
//
//  Pure layout helper for DiskIIDebugPanel. Given a client rect size
//  and DPI, returns RECTs for every control slot in the panel.
//  Mirrors the legacy DiskIIDebugDialog layout (kRowHeight=22, kMargin=8,
//  kRowGap=4 at 96 DPI) so the new panel is visually familiar.
//
//  Slot order (top to bottom):
//      Row 1: 8 event-type checkboxes
//      Row 2: audio master checkbox + 4 audio sub checkboxes
//      Row 3: 3 drive radios + track label + track edit + sector label + sector edit
//      Row 4: raw-quarter-track checkbox (aligned under track edit)
//      Row 5: track-invalid + sector-invalid feedback labels
//      Row 6: Pause + Clear buttons
//      Row 7: ListView fills remainder
//
//  Free function (no class state). Pure / no Win32 calls; easily
//  unit-testable. RECT type comes from Windows.h.
//
////////////////////////////////////////////////////////////////////////////////



constexpr int  kEventTypeCheckCount = 8;
constexpr int  kAudioSubCheckCount  = 4;
constexpr int  kDriveRadioCount     = 3;



struct PanelLayoutSlots
{
    RECT eventTypeChecks  [kEventTypeCheckCount];
    RECT audioMasterCheck;
    RECT audioSubChecks   [kAudioSubCheckCount];
    RECT driveRadios      [kDriveRadioCount];
    RECT trackFilterLabel;
    RECT trackEdit;
    RECT trackInvalidLabel;
    RECT sectorFilterLabel;
    RECT sectorEdit;
    RECT sectorInvalidLabel;
    RECT rawQtCheck;
    RECT pauseButton;
    RECT clearButton;
    RECT listView;
};



PanelLayoutSlots ComputeDiskIIDebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    UINT  dpi) noexcept;
