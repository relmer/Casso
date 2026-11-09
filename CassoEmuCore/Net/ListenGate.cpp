#include "Pch.h"

#include "Net/ListenGate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListenGate::Set
//
////////////////////////////////////////////////////////////////////////////////

void ListenGate::Set (const FirewallSnapshot & snapshot, const FirewallProgram & program)
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_snapshot = snapshot;
        m_program  = program;
        m_known    = true;
    }

    m_version++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListenGate::Admits
//
////////////////////////////////////////////////////////////////////////////////

bool ListenGate::Admits (HostSocketKind protocol, uint16_t port) const
{
    return Check (protocol, port).IsAdmitted();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListenGate::Check
//
////////////////////////////////////////////////////////////////////////////////

FirewallVerdict ListenGate::Check (HostSocketKind protocol, uint16_t port) const
{
    std::lock_guard<std::mutex>  guard (m_lock);
    FirewallVerdict              verdict;



    if (m_known)
    {
        verdict = FirewallCheck::Evaluate (m_snapshot, m_program, protocol, port);
    }

    return verdict;
}
