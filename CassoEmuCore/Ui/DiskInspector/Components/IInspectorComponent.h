#pragma once

#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"

class IInspectorComponentHost;
struct InspectorReply;
class InspectorSession;





//  What changes when the window starts on a disk: whether there is one,
//  whether it is another disk than before, and whether it is the same file
//  mounted again, as Reset, Power Cycle and a machine change do.
struct DiskChange
{
    bool      hasDisk     = false;
    bool      isNewDisk   = false;
    bool      isReload    = false;
    uint64_t  lastMediaId = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IInspectorComponent
//
//  One area of the disk inspector: it owns its controls and their state, and
//  the window calls each component in a fixed order for these (FR-162).
//  Every call but Create has a default that does nothing, so a component
//  overrides only the ones it takes part in.
//
////////////////////////////////////////////////////////////////////////////////

class IInspectorComponent
{
public:
    virtual ~IInspectorComponent() = default;

    virtual void          Create     () = 0;
    virtual void          Update     ()                                                                       {}
    virtual void          Tick       (int64_t nowMs)                                                          { (void) nowMs; }
    virtual void          PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) { (void) painter; (void) text; (void) theme; }
    virtual void          PaintOver  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) { (void) painter; (void) text; (void) theme; }
    virtual bool          OnMouse    (const DxuiMouseEvent & ev)                                              { (void) ev; return false; }
    virtual bool          OnKey      (const DxuiKeyEvent & ev)                                                { (void) ev; return false; }
    virtual std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const                                { (void) pointPx; (void) outAnchorPx; return std::wstring(); }
    virtual void          GetPrefs   (DiskInspectorPrefs & inOut) const                                       { (void) inOut; }
    virtual void          ApplyPrefs (const DiskInspectorPrefs & prefs)                                       { (void) prefs; }

    //  Before and after the window starts on a disk; after an analysis
    //  arrives; and the host's replies, true for one the component takes.
    virtual void          OnDiskStarting    (const DiskChange & change)                                       { (void) change; }
    virtual void          OnDiskStarted     (const DiskChange & change)                                       { (void) change; }
    virtual void          OnAnalysisChanged ()                                                                {}
    virtual bool          TakeReply         (const InspectorReply & reply)                                    { (void) reply; return false; }
};
