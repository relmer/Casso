#include "Pch.h"

#include "Net/FirewallCheck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheck::Evaluate
//
//  With no network, no traffic arrives, so the port is admitted. On a
//  profile the firewall is off for, every port is admitted. The public
//  profile is evaluated first, so it is the one reported when two profiles
//  have the same verdict: on a public network both remedies are offered.
//
////////////////////////////////////////////////////////////////////////////////

FirewallVerdict FirewallCheck::Evaluate (const FirewallSnapshot & snapshot, const FirewallProgram & program, HostSocketKind protocol, uint16_t port)
{
    FirewallVerdict  verdict;



    for (FirewallProfile profile : { FirewallProfile::Public, FirewallProfile::Private, FirewallProfile::Domain })
    {
        FirewallVerdict  here;
        bool             blocked   = false;
        bool             allowed   = false;
        bool             elsewhere = false;

        if ((snapshot.active & profile) == FirewallProfile::None || (snapshot.enabled & profile) == FirewallProfile::None)
        {
            continue;
        }

        for (const FirewallRule & rule : snapshot.rules)
        {
            bool  covers = (rule.profiles & profile) != FirewallProfile::None;

            if (!rule.enabled || !Applies (rule, program, protocol, port))
            {
                continue;
            }

            if (!rule.allow && covers && !blocked)
            {
                blocked   = true;
                here.rule = rule.name;
            }

            allowed   = allowed   || (rule.allow && covers);
            elsewhere = elsewhere || (rule.allow && !covers);
        }

        here.profile   = profile;
        here.admission = blocked   ? FirewallAdmission::Refused
                       : allowed   ? FirewallAdmission::Admitted
                       : elsewhere ? FirewallAdmission::OtherProfile
                                   : FirewallAdmission::NoRule;

        if (GetSeverity (here.admission) > GetSeverity (verdict.admission))
        {
            verdict = here;
        }
    }

    return verdict;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheck::MatchesPort
//
//  Entries are separated by commas, each a port, a range such as 6500-6510,
//  or * for every port.
//
////////////////////////////////////////////////////////////////////////////////

bool FirewallCheck::MatchesPort (const std::wstring & localPorts, uint16_t port)
{
    bool    matches = localPorts.empty();
    size_t  start   = 0;



    while (!matches && start <= localPorts.size())
    {
        size_t        comma  = localPorts.find (L',', start);
        std::wstring  entry  = localPorts.substr (start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
        size_t        dash   = entry.find (L'-');
        bool          digits = !entry.empty() && entry.find_first_not_of (L"0123456789-") == std::wstring::npos;
        unsigned      low    = 0;
        unsigned      high   = 0;

        if (entry == L"*" || entry == L"Any")
        {
            matches = true;
        }
        else if (digits && dash != 0 && dash != entry.size() - 1)
        {
            low     = (unsigned) _wtoi (entry.c_str());
            high    = (dash == std::wstring::npos) ? low : (unsigned) _wtoi (entry.c_str() + dash + 1);
            matches = port >= low && port <= high;
        }

        start = (comma == std::wstring::npos) ? localPorts.size() + 1 : comma + 1;
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheck::Applies
//
////////////////////////////////////////////////////////////////////////////////

bool FirewallCheck::Applies (const FirewallRule & rule, const FirewallProgram & program, HostSocketKind protocol, uint16_t port)
{
    bool  container   = rule.application.empty() && !rule.owner.empty();
    bool  application = rule.application.empty() || _wcsicmp (rule.application.c_str(), program.path.c_str()) == 0;
    bool  package     = rule.package.empty()     || _wcsicmp (rule.package.c_str(),     program.package.c_str()) == 0;
    bool  kind        = rule.anyProtocol         || rule.protocol == protocol;



    return !container && application && package && kind && MatchesPort (rule.localPorts, port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallCheck::GetSeverity
//
//  A refusal first, since a rule has to be removed; then no rule, which
//  raises a prompt; then a rule on another profile.
//
////////////////////////////////////////////////////////////////////////////////

int FirewallCheck::GetSeverity (FirewallAdmission admission)
{
    int  severity = 0;



    switch (admission)
    {
        case FirewallAdmission::Refused:       severity = 3;  break;
        case FirewallAdmission::NoRule:        severity = 2;  break;
        case FirewallAdmission::OtherProfile:  severity = 1;  break;
        default:                               severity = 0;  break;
    }

    return severity;
}
