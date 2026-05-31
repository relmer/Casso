#include "Pch.h"

#include "Disk2DebugPanelLayout.h"


namespace
{
    constexpr int  kMargin96             = 8;
    constexpr int  kRowHeight96          = 22;
    constexpr int  kRowGap96             = 4;
    constexpr int  kCheckWidth96         = 110;
    constexpr int  kRadioWidth96         = 78;
    constexpr int  kEditWidth96          = 140;
    constexpr int  kFilterLabelWidth96   = 110;
    constexpr int  kRawQtCheckWidth96    = 170;
    constexpr int  kIgnoredLabelHeight96 = 18;
    constexpr int  kDriveLabelWidth96    = 44;
    constexpr int  kRowLabelWidth96      = 92;
    constexpr int  kButtonWidth96        = 90;
    constexpr int  kButtonHeight96       = 26;



    int Scale (int dipValue, UINT dpi) noexcept
    {
        return MulDiv (dipValue, (int) dpi, 96);
    }



    RECT MakeRect (int x, int y, int w, int h) noexcept
    {
        RECT r;
        r.left   = x;
        r.top    = y;
        r.right  = x + w;
        r.bottom = y + h;
        return r;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeDisk2DebugPanelLayout
//
//  Mirrors Disk2DebugDialog's LayoutControls(). All metrics scaled
//  from the legacy 96-DPI constants. Returns deterministic rects for
//  the panel client area assuming origin (0,0). Caller offsets as
//  needed for chrome insets.
//
////////////////////////////////////////////////////////////////////////////////

PanelLayoutSlots ComputeDisk2DebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    int   topOffsetPx,
    UINT  dpi) noexcept
{
    PanelLayoutSlots slots         = {};
    int              margin        = Scale (kMargin96,             dpi);
    int              rowHeight     = Scale (kRowHeight96,          dpi);
    int              rowGap        = Scale (kRowGap96,             dpi);
    int              checkWidth    = Scale (kCheckWidth96,         dpi);
    int              radioWidth    = Scale (kRadioWidth96,         dpi);
    int              editWidth     = Scale (kEditWidth96,          dpi);
    int              labelWidth    = Scale (kFilterLabelWidth96,   dpi);
    int              driveLblWidth = Scale (kDriveLabelWidth96,    dpi);
    int              rowLblWidth   = Scale (kRowLabelWidth96,      dpi);
    int              rawQtWidth    = Scale (kRawQtCheckWidth96,    dpi);
    int              ignoredHeight = Scale (kIgnoredLabelHeight96, dpi);
    int              buttonWidth   = Scale (kButtonWidth96,        dpi);
    int              buttonHeight  = Scale (kButtonHeight96,       dpi);
    int              x             = 0;
    int              y             = topOffsetPx + margin;
    int              trackEditX    = 0;
    int              sectorEditX   = 0;



    // Row 1: "Disk events:" label + 8 event-type checkboxes.
    x = margin;
    slots.diskEventsLabel = MakeRect (x, y, rowLblWidth, rowHeight);
    x += rowLblWidth + rowGap;
    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        slots.eventTypeChecks[i] = MakeRect (x, y, checkWidth, rowHeight);
        x += checkWidth;
    }
    y += rowHeight + rowGap;

    // Row 2: "Audio events:" label + audio master + 4 sub checks.
    // Label width matches row 1 so the checkbox columns align, and
    // every checkbox on the row is checkWidth wide (matches row 1) so
    // every column lines up — not just the first one.
    x = margin;
    slots.audioEventsLabel = MakeRect (x, y, rowLblWidth, rowHeight);
    x += rowLblWidth + rowGap;
    slots.audioMasterCheck = MakeRect (x, y, checkWidth, rowHeight);
    x += checkWidth;
    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        slots.audioSubChecks[i] = MakeRect (x, y, checkWidth, rowHeight);
        x += checkWidth;
    }
    y += rowHeight + rowGap;

    // Row 3: drive radios + filter label/edit pairs.
    x = margin;
    slots.driveFilterLabel = MakeRect (x, y, driveLblWidth, rowHeight);
    x += driveLblWidth + rowGap;
    for (int i = 0; i < kDriveRadioCount; i++)
    {
        slots.driveRadios[i] = MakeRect (x, y, radioWidth, rowHeight);
        x += radioWidth;
    }
    x += rowGap;
    slots.trackFilterLabel = MakeRect (x, y, labelWidth, rowHeight);
    x += labelWidth + rowGap;
    trackEditX = x;
    slots.trackEdit = MakeRect (x, y, editWidth, rowHeight);
    x += editWidth + rowGap;
    slots.sectorFilterLabel = MakeRect (x, y, labelWidth, rowHeight);
    x += labelWidth + rowGap;
    sectorEditX = x;
    slots.sectorEdit = MakeRect (x, y, editWidth, rowHeight);
    y += rowHeight + rowGap;

    // Row 4: raw-quarter-track checkbox aligned beneath track edit.
    slots.rawQtCheck = MakeRect (trackEditX, y, rawQtWidth, rowHeight);
    y += rowHeight + rowGap;

    // Row 5: invalid feedback labels beneath the two edits.
    int trackInvalidWidth = (sectorEditX - trackEditX) - rowGap;
    if (trackInvalidWidth < 1) { trackInvalidWidth = 1; }
    slots.trackInvalidLabel  = MakeRect (trackEditX,  y, trackInvalidWidth, ignoredHeight);
    slots.sectorInvalidLabel = MakeRect (sectorEditX, y, editWidth,         ignoredHeight);
    y += ignoredHeight + rowGap;

    // Row 6: Pause / Clear buttons.
    slots.pauseButton = MakeRect (margin,                              y, buttonWidth, buttonHeight);
    slots.clearButton = MakeRect (margin + buttonWidth + rowGap,       y, buttonWidth, buttonHeight);
    y += buttonHeight + rowGap;

    // Row 7: ListView fills remainder.
    int lvWidth  = clientWidthPx  - 2 * margin;
    int lvHeight = clientHeightPx - y - margin;
    if (lvWidth  < 1) { lvWidth  = 1; }
    if (lvHeight < 1) { lvHeight = 1; }
    slots.listView = MakeRect (margin, y, lvWidth, lvHeight);

    return slots;
}
