#pragma once

#include "Pch.h"

#include "Seams/IHostFirewall.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallProgram
//
//  A program as the firewall's rules identify it: its path, and its
//  package's SID when it runs from a package.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallProgram
{
    std::wstring  path;
    std::wstring  package;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallAdmission
//
//  Whether inbound traffic to a port arrives at a program on a profile:
//  admitted; blocked by a block rule; no rule for it at all, for which a
//  prompt appears; or allowed only on profiles the PC is not on.
//
////////////////////////////////////////////////////////////////////////////////

enum class FirewallAdmission
{
    Admitted,
    Refused,
    NoRule,
    OtherProfile,
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallVerdict
//
//  The admission, the active profile it is for, and for a refusal, the
//  block rule's name.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallVerdict
{
    FirewallAdmission  admission = FirewallAdmission::Admitted;
    FirewallProfile    profile   = FirewallProfile::None;
    std::wstring       rule;

    bool  IsAdmitted () const { return admission == FirewallAdmission::Admitted; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheck
//
//  Evaluates a snapshot's inbound rules for one program, protocol and port,
//  as Windows Firewall does: a block rule overrides an allow rule, and with
//  neither the default inbound action blocks. With the PC on more than one
//  profile, the verdict is the worst of them.
//
////////////////////////////////////////////////////////////////////////////////

class FirewallCheck
{
public:
    static FirewallVerdict  Evaluate    (const FirewallSnapshot & snapshot, const FirewallProgram & program, HostSocketKind protocol, uint16_t port);

    //  Whether a rule's port list, as Windows lists it, has the port.
    static bool             MatchesPort (const std::wstring & localPorts, uint16_t port);

private:
    static bool             Applies     (const FirewallRule & rule, const FirewallProgram & program, HostSocketKind protocol, uint16_t port);
    static int              GetSeverity (FirewallAdmission admission);
};
