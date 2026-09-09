#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanelLayout
//
//  Pure layout helper for Disk2DebugPanel. Given a client rect size
//  and DPI, returns RECTs for every control slot in the panel.
//  Mirrors the legacy Disk2DebugDialog layout (kRowHeight=22, kMargin=8,
//  kRowGap=4 at 96 DPI) so the new panel is visually familiar.
//
//  Slot order (top to bottom):
//      Row 1: "Disk events:" label + 8 event-type checkboxes
//      Row 2: "Audio events:" label + audio master checkbox + 4 audio sub checkboxes
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
    RECT diskEventsLabel;
    RECT audioEventsLabel;
    RECT eventTypeChecks  [kEventTypeCheckCount];
    RECT audioMasterCheck;
    RECT audioSubChecks   [kAudioSubCheckCount];
    RECT driveRadios      [kDriveRadioCount];
    RECT driveFilterLabel;
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



PanelLayoutSlots ComputeDisk2DebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    int   topOffsetPx,
    UINT  dpi) noexcept;
