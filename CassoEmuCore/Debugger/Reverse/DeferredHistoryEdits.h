#pragma once

#include "Pch.h"

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeferredHistoryEdits
//
//  The debugger edits deferred behind live until the user answers whether
//  to discard the recorded future, in the order they were made. The first
//  one deferred posts the question; the rest share its answer. Used from the
//  thread that runs the machine and the window's thread, so every call takes
//  the lock.
//
////////////////////////////////////////////////////////////////////////////////

class DeferredHistoryEdits
{
public:
    //  True when no edit was deferred before this one, so the question is due.
    bool                       Defer   (const DebugCommand & command);

    bool                       HasAny  () const;
    std::vector<DebugCommand>  TakeAll ();
    void                       Clear   ();

private:
    mutable std::mutex         m_lock;
    std::vector<DebugCommand>  m_edits;
};
