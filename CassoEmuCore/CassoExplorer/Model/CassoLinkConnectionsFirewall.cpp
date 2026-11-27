#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Seams/ILinkAnnouncements.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ApplyFirewall
//
//  A Wi-Fi modem whose port the gate did not admit opens on its session's next attempt.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ApplyFirewall()
{
    if (m_firewall != nullptr)
    {
        m_gate.Set (m_firewall->GetSnapshot(), m_firewall->GetProgram());
    }

    ApplyListening();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ApplyListening
//
//  With the port bound by another program without reuse, no Apple is listed
//  from an announcement; nothing else depends on them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ApplyListening()
{
    HRESULT  hr = S_OK;



    if (m_announcements != nullptr)
    {
        hr = m_announcements->SetListening (IsSearching() && IsAnnounceAdmitted());
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsAnnounceAdmitted
//
//  Until the firewall is read, nothing is known to admit the port.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsAnnounceAdmitted() const
{
    return m_firewall == nullptr || (m_firewall->GetVersion() != 0 && m_firewall->Check (HostSocketKind::Udp, LinkProgram::kAnnouncePort).IsAdmitted());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsSearching
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsSearching() const
{
    return std::any_of (m_saved.begin(), m_saved.end(), [] (const CassoLinkSettings & settings) { return settings.IsSearch(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetInboundPort
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::GetInboundPort (const CassoLinkSettings & settings, FirewallPort & outPort)
{
    bool  inbound = settings.IsSearch() || settings.transport == LinkTransport::WifiModem;



    if (inbound)
    {
        outPort.protocol = settings.IsSearch() ? HostSocketKind::Udp : HostSocketKind::Tcp;
        outPort.port     = settings.IsSearch() ? LinkProgram::kAnnouncePort : CassoLinkSettings::kModemPort;
    }

    return inbound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::AddFirewall
//
//  The cause replaces the connection's state in the list, since none of the
//  state's instructions can work until the port is allowed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::AddFirewall (const CassoLinkSettings & settings, LinkTreeEntry & entry) const
{
    FirewallPort     port;
    FirewallVerdict  verdict;



    if (m_firewall == nullptr || !GetInboundPort (settings, port))
    {
        return;
    }

    verdict = m_firewall->Check (port.protocol, port.port);

    if (verdict.IsAdmitted())
    {
        return;
    }

    entry.firewall       = FirewallText::DescribeBlocked (kFirewallTitle, { port });
    entry.firewallPublic = verdict.IsOnPublic();
    entry.status         = std::format (L"{}{}{}", entry.firewall, entry.firewallPublic ? L" " : L"",
                                        entry.firewallPublic ? FirewallText::kPublicChoice : L"");
}
