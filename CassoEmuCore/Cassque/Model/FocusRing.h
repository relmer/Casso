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
//  The order Tab walks the browser window in, and the step from one stop to
//  the next.
//
//  HEADLESS, SO THE ORDER IS TESTED RATHER THAN CLICKED THROUGH. The window
//  says which stops can take focus right now and applies the answer; which
//  stop comes next -- skipping a button that cannot be used, wrapping at the
//  ends, where a walk starts when focus is somewhere the ring no longer holds
//  -- is decided here.
//
//  The order is Explorer's reading order: the toolbar's buttons left to right,
//  the tab strip, the folder tree, the file list, and the preview. The menu
//  bar is not in it. A menu is reached with Alt or F10, as in any Windows
//  program, and Explorer has none.
//
////////////////////////////////////////////////////////////////////////////////

class FocusRing
{
public:
    //  The stops in Tab order, leaving out a toolbar button that cannot be
    //  used right now and the preview while it is hidden.
    static std::vector<FocusStop>  BuildStops (const std::vector<bool> & toolbarEnabled, bool previewVisible);

    //  The stop after `current`, or before it walking backward, wrapping at
    //  either end. A current stop the ring no longer holds -- a button that was
    //  just disabled -- starts the walk from the front, or from the back.
    static FocusStop  GetNext (const std::vector<FocusStop> & stops, const FocusStop & current, bool forward);
};
