#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTracker
//
//  Tells a move of a window by its title bar from a resize by its border,
//  both of which run the system's move loop, and says when either ends.
//
//  A TICK WHOSE WINDOW KEEPS ITS SIZE IS A MOVE. The first tick with the
//  button down starts the drag at the window's size; each tick at that size
//  reports a move, and the first at another size cancels the drag, which
//  then reports nothing more.
//
//  A DRAG THAT REPORTED A MOVE ALWAYS ENDS. The end of the move loop, or the
//  first poll after the button is up, reports Ended at the starting size and
//  Canceled at any other, so whoever showed something for the moves can
//  take it down again.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiCaptionDragTracker
{
public:
    enum class Event
    {
        None,
        Moved,
        Ended,
        Canceled,
    };

    //  One tick of the system's move loop.
    Event  OnTick    (bool buttonDown, SIZE windowSize);

    //  The move loop ended.
    Event  OnLoopEnd (SIZE windowSize);

    //  Once a frame, for a loop whose end was not reported.
    Event  OnPoll    (bool buttonDown, SIZE windowSize);

    //  The window moved to a monitor at another scale, which resized it:
    //  `windowSize` is its size now, and still a move's.
    void   Rebase    (SIZE windowSize);

    bool   IsDragging () const { return m_dragging; }

private:
    Event  Finish    (SIZE windowSize);

    bool   m_dragging = false;
    bool   m_reported = false;
    bool   m_resized  = false;
    SIZE   m_size     = {};
};
