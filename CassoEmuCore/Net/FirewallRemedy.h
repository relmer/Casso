#pragma once

#include "Pch.h"

#include "Net/FirewallCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallPort
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallPort
{
    HostSocketKind  protocol = HostSocketKind::Tcp;
    uint16_t        port     = 0;

    bool operator== (const FirewallPort & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallPlan
//
//  What the elevated helper changes for one program: each port is admitted
//  on the profiles, by disabling the block rules that apply to it and adding
//  or widening an allow rule for it alone. With markPrivate, the PC's public
//  networks are marked private first.
//
////////////////////////////////////////////////////////////////////////////////

struct FirewallPlan
{
    std::wstring               program;                         // the program's path
    std::wstring               title;                           // the program in each rule's name, as "Casso Explorer"
    FirewallProfile            profiles    = FirewallProfile::None;
    std::vector<FirewallPort>  ports;
    bool                       markPrivate = false;

    bool operator== (const FirewallPlan & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy
//
//  The plan for a program's ports, its form on the helper's command line,
//  and which rules the helper changes.
//
////////////////////////////////////////////////////////////////////////////////

class FirewallRemedy
{
public:
    //  The ports that are not admitted on every active profile, and those
    //  profiles. With markPrivate, the private profile replaces the public
    //  one, since the networks are about to be private.
    static FirewallPlan   MakePlan        (const FirewallSnapshot & snapshot, const FirewallProgram & program, const std::wstring & title,
                                           const std::vector<FirewallPort> & ports, bool markPrivate);

    //  The allow rule's name for a port, as "Casso Explorer, UDP 6510".
    static std::wstring   MakeRuleName    (const FirewallPlan & plan, const FirewallPort & port);

    //  Whether a rule is an enabled block rule the plan disables for a port.
    static bool           IsBlocking      (const FirewallRule & rule, const FirewallPlan & plan, const FirewallPort & port);

    //  Whether a rule is the plan's allow rule for a port, to widen.
    static bool           IsPlanRule      (const FirewallRule & rule, const FirewallPlan & plan, const FirewallPort & port);

    //  The plan as one command-line argument, and back. Text that is not a
    //  plan fails with ERROR_INVALID_DATA.
    static std::wstring   FormatArguments (const FirewallPlan & plan);
    static HRESULT        ParseArguments  (const std::wstring & text, FirewallPlan & outPlan);

    //  The rules' group in Windows Defender Firewall's list.
    static constexpr const wchar_t *  kGrouping = L"Casso";
};
