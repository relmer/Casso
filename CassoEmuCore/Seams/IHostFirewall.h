#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallProfile
//
//  Windows Firewall's profiles as bits, the values NET_FW_PROFILE_TYPE2 has.
//  A network's category selects its profile: a domain network the domain
//  profile, a private one the private profile, and every other network,
//  a new one included, the public profile.
//
////////////////////////////////////////////////////////////////////////////////

enum class FirewallProfile : uint32_t
{
    None    = 0,
    Domain  = 0x1,
    Private = 0x2,
    Public  = 0x4,
    All     = 0x7,
};

constexpr FirewallProfile operator| (FirewallProfile a, FirewallProfile b) { return (FirewallProfile) ((uint32_t) a | (uint32_t) b); }
constexpr FirewallProfile operator& (FirewallProfile a, FirewallProfile b) { return (FirewallProfile) ((uint32_t) a & (uint32_t) b); }





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallPort
//
//  A protocol and a port on the PC that other computers send to. A listener
//  that moves to a later port when its port is in use has a range: basePort
//  and lastPort are the range's ends, and port is the one it uses now. An
//  allow rule for the port covers the whole range.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallPort
{
    HostSocketKind  protocol = HostSocketKind::Tcp;
    uint16_t        port     = 0;
    uint16_t        lastPort = 0;   // the range's last port, or 0 for port alone
    uint16_t        basePort = 0;   // the range's first port, or 0 for port alone

    //  The range an allow rule covers.
    uint16_t  GetFirstPort () const { return basePort == 0 ? port : basePort; }
    uint16_t  GetLastPort  () const { return lastPort == 0 ? port : lastPort; }

    bool operator== (const FirewallPort & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRule
//
//  One inbound rule. An empty application or package applies to every
//  program, and an empty port list to every port. A port list Windows writes
//  as a keyword, such as RPC, matches none of Casso's ports. A rule with an
//  owner and no application is a store app's capability rule, which applies
//  to app containers only, so to no Casso program.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallRule
{
    std::wstring     name;
    std::wstring     application;               // the program's path
    std::wstring     package;                   // the package's SID, as S-1-15-2-...
    std::wstring     owner;                     // the user's SID, for a store app's rule
    bool             anyProtocol = true;
    HostSocketKind   protocol    = HostSocketKind::Tcp;
    std::wstring     localPorts;                // as Windows lists them: "6400", "6500-6510,6600", "*"
    FirewallProfile  profiles    = FirewallProfile::All;
    bool             allow       = true;
    bool             enabled     = true;

    //  False for a rule limited to some local or remote addresses or some
    //  interfaces, such as Tailscale's for the PC's Tailscale address. Such a
    //  rule doesn't admit what arrives from the LAN, so its allow doesn't
    //  admit a port. Its block counts.
    bool             anyAddress  = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallSnapshot
//
//  What the firewall has at one moment: the profiles of the networks the PC
//  is on, the profiles the firewall is on for, and the inbound rules.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallSnapshot
{
    FirewallProfile            active  = FirewallProfile::None;
    FirewallProfile            enabled = FirewallProfile::All;
    std::vector<FirewallRule>  rules;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostFirewall
//
//  Windows Firewall, read. Changing it takes elevation, so it is changed only
//  by the helper the user starts.
//
////////////////////////////////////////////////////////////////////////////////

class IHostFirewall
{
public:
    virtual ~IHostFirewall () = default;

    virtual HRESULT  Read               (FirewallSnapshot & outSnapshot) = 0;

    //  The active profiles alone, cheap enough to read on every poll; they
    //  change when a network's category does, with no address change.
    virtual HRESULT  ReadActiveProfiles (FirewallProfile & outActive) = 0;
};
