#include "Pch.h"

#include "Net/FirewallMonitor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallMonitor::FirewallMonitor
//
////////////////////////////////////////////////////////////////////////////////

FirewallMonitor::FirewallMonitor (IHostFirewall & firewall, const INetworkChanges * changes, FirewallProgram program) :
    m_firewall (firewall),
    m_changes  (changes),
    m_program  (std::move (program))
{
    m_seenChanges = (m_changes != nullptr) ? m_changes->GetChangeCount() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallMonitor::Update
//
////////////////////////////////////////////////////////////////////////////////

bool FirewallMonitor::Update()
{
    HRESULT           hrActive = S_OK;
    HRESULT           hrRead   = S_OK;
    FirewallProfile   active   = FirewallProfile::None;
    FirewallSnapshot  snapshot;
    uint32_t          changes  = (m_changes != nullptr) ? m_changes->GetChangeCount() : m_seenChanges;
    bool              stale    = m_stale || changes != m_seenChanges;



    hrActive = m_firewall.ReadActiveProfiles (active);
    stale    = stale || (SUCCEEDED (hrActive) && active != m_seenActive);

    if (stale)
    {
        hrRead = m_firewall.Read (snapshot);

        m_snapshot    = SUCCEEDED (hrRead) ? std::move (snapshot) : FirewallSnapshot();
        m_seenActive  = SUCCEEDED (hrActive) ? active : m_seenActive;
        m_seenChanges = changes;
        m_stale       = false;
        m_version++;
    }

    return stale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallMonitor::Check
//
////////////////////////////////////////////////////////////////////////////////

FirewallVerdict FirewallMonitor::Check (HostSocketKind protocol, uint16_t port) const
{
    return FirewallCheck::Evaluate (m_snapshot, m_program, protocol, port);
}
