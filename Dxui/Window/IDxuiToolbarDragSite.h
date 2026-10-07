#pragma once

#include "Pch.h"
#include "Widgets/DxuiToolbarDock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiToolbarDragSite
//
//  What a DxuiToolbarDragSession drives: the toolbar, its dock bands and the
//  window it floats in. The session decides what a drag does; the site
//  reports where things are and makes the changes the session decides on.
//  Every point is in screen pixels.
//
//  Where a toolbar docks is decided by the pointer alone, never by where
//  the toolbar's own ends happen to be.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiToolbarDragSite
{
public:
    virtual ~IDxuiToolbarDragSite() = default;

    //  The docked toolbar under a drag: whether the pointer has pulled far
    //  enough from its edge's bands to tear it off, and otherwise following
    //  the pointer along and among them.
    virtual bool  IsPulledOut     (POINT screenPx)                                      = 0;
    virtual void  SlideDocked     (POINT screenPx)                                      = 0;

    //  The toolbar floated in a window at `rectPx`, lying down or standing
    //  up; false when no window could be made, and it stays docked.
    virtual bool  TryTearOff      (const RECT & rectPx, bool vertical)                  = 0;

    //  The floating window: the DPI it has (the docked toolbar's window's
    //  before it has one), its size at `dpi`, how far in from each end of
    //  it the toolbar starts, and the orientation the pointer gives it.
    virtual UINT  GetFloatDpi     ()                                                    = 0;
    virtual SIZE  GetFloatSizePx  (bool vertical, UINT dpi)                             = 0;
    virtual int   GetFloatInsetPx (UINT dpi)                                            = 0;
    virtual bool  PickVertical    (POINT screenPx, bool current)                        = 0;
    virtual void  PlaceFloat      (const RECT & rectPx, bool vertical)                  = 0;

    //  Where a release at the pointer docks the toolbar, `grabAlongDip` from
    //  its start being where it is held, and showing that place, or nothing
    //  for `show` false, while the button is still down.
    virtual bool  TryPickDrop     (POINT screenPx, int grabAlongDip, DxuiToolbarDock & outDock, bool & outNewBand) = 0;
    virtual void  ShowDropPreview (bool show, const DxuiToolbarDock & dock, bool isNewBand) = 0;

    //  The end of the drag: a floating toolbar released over a band docks
    //  there, released anywhere else stays where it is; a docked one stays
    //  where it was carried; a canceled drag puts everything back where it
    //  was when the drag began.
    virtual void  DropDocked      (const DxuiToolbarDock & dock, bool isNewBand)        = 0;
    virtual void  LeaveFloating   ()                                                    = 0;
    virtual void  PutDown         ()                                                    = 0;
    virtual void  Restore         ()                                                    = 0;
};
