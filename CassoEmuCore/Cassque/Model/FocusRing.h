#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FocusStop
//
//  One place Tab can leave the keyboard in the browser window.
//
////////////////////////////////////////////////////////////////////////////////

struct FocusStop
{
    enum class Kind { ToolbarEntry, Tabs, Tree, List, Preview };

    Kind  kind  = Kind::Tree;
    int   entry = -1;     // the toolbar entry's index; -1 for every other kind

    bool operator== (const FocusStop & other) const { return kind == other.kind && entry == other.entry; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  FocusRing
//
//  The Tab order of the browser window, and the step from one stop to the
//  next.
//
//  HEADLESS, SO THE ORDER IS UNIT TESTED. The window supplies the stops that
//  can take focus and applies the result. This class computes the next stop:
//  it skips disabled buttons, wraps at the ends, and restarts from an end
//  when the current stop is no longer in the list.
//
//  The order follows Explorer: the toolbar buttons left to right, the tab
//  strip, the folder tree, the file list, and the preview. The menu bar is
//  excluded; Alt or F10 opens a menu, as in any Windows program, and Explorer
//  has no menu bar.
//
////////////////////////////////////////////////////////////////////////////////

class FocusRing
{
public:
    //  The stops in Tab order, leaving out a toolbar button that cannot be
    //  used right now and the preview while it is hidden.
    static std::vector<FocusStop>  BuildStops (const std::vector<bool> & toolbarEnabled, bool previewVisible);

    //  The stop after `current`, or before it when moving backward, wrapping at
    //  either end. If `current` is not in `stops`, for example a button that was
    //  just disabled, the result is the first stop, or the last when moving
    //  backward.
    static FocusStop  GetNext (const std::vector<FocusStop> & stops, const FocusStop & current, bool forward);
};
