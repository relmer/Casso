#pragma once

#include "Pch.h"

#include "Net/FirewallCheck.h"
#include "Seams/IHostFirewall.h"
#include "Seams/INetworkChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallMonitor
//
//  One program's firewall verdicts, from a snapshot read again only when
//  something can have changed them: an address or interface change, a
//  network's category, or Invalidate after the firewall is changed. Every
//  call is on one thread, the one COM is initialized on for the firewall.
//
//  Before the first read, and after a read that fails, every port is
//  admitted, so no warning is drawn from what is not known.
//
////////////////////////////////////////////////////////////////////////////////

class FirewallMonitor
{
public:
    FirewallMonitor (IHostFirewall & firewall, const INetworkChanges * changes, FirewallProgram program);

    //  Reads the snapshot again when it is out of date; true when it did.
    bool             Update     ();

    //  Marks the snapshot out of date, for the next Update.
    void             Invalidate () { m_stale = true; }

    FirewallVerdict  Check      (HostSocketKind protocol, uint16_t port) const;

    //  Changes with each read, so a view compares it with the one it drew.
    uint32_t         GetVersion () const { return m_version; }

    const FirewallSnapshot &  GetSnapshot () const { return m_snapshot; }
    const FirewallProgram  &  GetProgram  () const { return m_program; }

private:
    IHostFirewall          & m_firewall;
    const INetworkChanges  * m_changes     = nullptr;
    FirewallProgram          m_program;
    FirewallSnapshot         m_snapshot;
    FirewallProfile          m_seenActive  = FirewallProfile::None;
    uint32_t                 m_seenChanges = 0;
    uint32_t                 m_version     = 0;
    bool                     m_stale       = true;
};
