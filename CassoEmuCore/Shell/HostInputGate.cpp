#include "Pch.h"

#include "Shell/HostInputGate.h"





thread_local std::vector<std::pair<const HostInputGate *, int>>  HostInputGate::t_entered;





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate::TryEnter
//
//  Enters the gate into entry and returns whether the host may write. The
//  caller writes only on true, with entry in scope; the mutex is taken shared
//  only by a thread's first Entry.
//
////////////////////////////////////////////////////////////////////////////////

bool HostInputGate::TryEnter (Entry & entry)
{
    auto  found = std::find_if (t_entered.begin(), t_entered.end(), [this] (const auto & held) { return held.first == this; });



    entry.Leave();

    if (found != t_entered.end())
    {
        found->second++;
    }
    else
    {
        m_mutex.lock_shared();
        t_entered.emplace_back (this, 1);
    }

    entry.m_gate = this;

    return !m_isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate::Entry::Leave
//
//  The mutex is released by the thread's last Entry.
//
////////////////////////////////////////////////////////////////////////////////

void HostInputGate::Entry::Leave()
{
    HostInputGate  * gate  = m_gate;
    auto             found = t_entered.end();



    if (gate == nullptr)
    {
        return;
    }

    m_gate = nullptr;
    found  = std::find_if (t_entered.begin(), t_entered.end(), [gate] (const auto & held) { return held.first == gate; });

    if (found != t_entered.end() && --found->second == 0)
    {
        t_entered.erase (found);
        gate->m_mutex.unlock_shared();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate::Hold
//
////////////////////////////////////////////////////////////////////////////////

void HostInputGate::Hold()
{
    std::unique_lock<std::shared_mutex>  lock (m_mutex);



    m_isHeld = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate::Release
//
////////////////////////////////////////////////////////////////////////////////

void HostInputGate::Release()
{
    std::unique_lock<std::shared_mutex>  lock (m_mutex);



    m_isHeld = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostInputGate::IsHeld
//
////////////////////////////////////////////////////////////////////////////////

bool HostInputGate::IsHeld() const
{
    std::shared_lock<std::shared_mutex>  lock (m_mutex);



    return m_isHeld;
}





