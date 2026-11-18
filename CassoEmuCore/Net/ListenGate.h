#pragma once

#include "Pch.h"

#include "Net/FirewallCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListenGate
//
//  Whether the firewall admits a port, for the code that opens a listener on
//  the PC's addresses. A Windows Firewall prompt appears the first time a
//  program listens with no rule for it, so a port the firewall does not
//  admit is not opened at all. The program's window offers Allow instead,
//  and the port opens once the helper has admitted it.
//
//  A copy of one program's firewall snapshot, set on the thread that reads
//  the firewall and checked on any thread. Until a snapshot is set, every
//  port is admitted, as nothing is known to stop it.
//
////////////////////////////////////////////////////////////////////////////////

class ListenGate
{
public:
    void  Set (const FirewallSnapshot & snapshot, const FirewallProgram & program);

    bool             Admits     (HostSocketKind protocol, uint16_t port) const;
    FirewallVerdict  Check      (HostSocketKind protocol, uint16_t port) const;

    //  Changes with each Set, so a listener deferred for a port can be opened
    //  again when the verdicts may have changed.
    uint32_t         GetVersion () const { return m_version.load(); }

private:
    mutable std::mutex     m_lock;
    FirewallSnapshot       m_snapshot;
    FirewallProgram        m_program;
    bool                   m_known   = false;
    std::atomic<uint32_t>  m_version { 0 };
};
