#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate
//
//  Whether the host may write its input (keys, paddles, buttons, the mouse)
//  into the machine's devices. While reverse execution has the machine behind
//  live, the devices hold the state of a recorded position, and a replay may
//  be running on the CPU thread; input from the UI or controller thread would
//  then leak into a recording it never belonged to. The gate is held for
//  exactly that time.
//
//  A writer calls TryEnter and writes only when it returns true, keeping the
//  lock it was handed for as long as it writes; Hold then waits for every
//  such writer to finish, so once Hold returns no host write can land until
//  Release. Writers on any thread; Hold and Release on the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class HostInputGate
{
public:
    bool  TryEnter (std::shared_lock<std::shared_mutex> & lock);
    void  Hold     ();
    void  Release  ();
    bool  IsHeld   () const;

private:
    mutable std::shared_mutex  m_mutex;
    bool                       m_isHeld = false;
};
