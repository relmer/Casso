#pragma once

#include "Pch.h"
#include "Window/DxuiCaptionDragTracker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarMoveTracker
//
//  A floating toolbar window's drag by its grab handle, from the move asked
//  for to the end of the system's move loop that carries it, reported as a
//  caption drag is.
//
//  A PLACEMENT BY THE PROGRAM IS NEITHER A DROP NOR A RESIZE. The window
//  holding a toolbar torn off its dock is made, placed and fitted to the
//  toolbar while the button is still down, and is fitted again whenever the
//  toolbar's length changes, so its owner sets its size during the drag. A
//  move or size of the window outside the loop ends the drag, and a size
//  change inside it cancels the drag, only when the program did not make it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarMoveTracker
{
public:
    using Event = DxuiCaptionDragTracker::Event;

    //  A move by the grab handle was asked for; the loop starts later.
    void   Begin      ();

    //  One tick of the system's move loop.
    Event  OnTick     (bool buttonDown, SIZE windowSize);

    //  The window was moved or sized outside the loop, or the loop ended.
    Event  OnPlaced   (SIZE windowSize);

    //  Once a frame, for a loop whose end was not reported, or a move asked
    //  for after the button came up, which never starts its loop.
    Event  OnPoll     (bool buttonDown, SIZE windowSize);

    //  The program is about to move or size the window, and has: it is now
    //  `windowSize`, and a drag under way goes on at that size.
    void   BeginPlace ();
    void   EndPlace   (SIZE windowSize);

    //  The window moved to a monitor at another scale, which resized it.
    void   Rebase     (SIZE windowSize);

    bool   IsMoving   () const { return m_isMoving; }

private:
    DxuiCaptionDragTracker  m_drag;
    bool                    m_isMoving  = false;
    bool                    m_isPlacing = false;
};
