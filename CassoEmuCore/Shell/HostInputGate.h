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
//  A writer calls TryEnter and writes only when it returns true, with the
//  Entry in scope for as long as it writes; Hold then waits for every
//  such writer to finish, so once Hold returns no host write can land until
//  Release. Writers on any thread; Hold and Release on the CPU thread.
//
//  A thread that has entered may enter again, as a key's write does when
//  the game-port mixer applies it on the same thread: the second Entry
//  counts against the first rather than taking the mutex shared again, which
//  would wait behind a Hold already waiting for the first and deadlock.
//
////////////////////////////////////////////////////////////////////////////////

class HostInputGate
{
public:
    //  The gate entered, until Leave or destruction. Used on one thread.
    class Entry
    {
    public:
        Entry  () = default;
        ~Entry ()                           { Leave(); }

        Entry             (const Entry &) = delete;
        Entry & operator= (const Entry &) = delete;

        void  Leave ();

    private:
        friend class HostInputGate;

        HostInputGate  * m_gate = nullptr;
    };

    bool  TryEnter (Entry & entry);
    void  Hold     ();
    void  Release  ();
    bool  IsHeld   () const;

private:
    //  The gates this thread has entered, each with its count of Entries.
    static thread_local std::vector<std::pair<const HostInputGate *, int>>  t_entered;

    mutable std::shared_mutex  m_mutex;
    bool                       m_isHeld = false;
};
