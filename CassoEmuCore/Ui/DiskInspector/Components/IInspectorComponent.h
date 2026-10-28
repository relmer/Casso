#pragma once

#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"

class IInspectorComponentHost;
class InspectorSession;





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
};
