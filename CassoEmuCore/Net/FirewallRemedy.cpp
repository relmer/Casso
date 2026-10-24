#include "Pch.h"

#include "Net/FirewallRemedy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::MakePlan
//
//  Each target profile is evaluated alone, so a port admitted on the private
//  profile and blocked on the public one is admitted on the public one only.
//
////////////////////////////////////////////////////////////////////////////////

FirewallPlan FirewallRemedy::MakePlan (const FirewallSnapshot & snapshot, const FirewallProgram & program, const std::wstring & title,
                                       const std::vector<FirewallPort> & ports, bool markPrivate)
{
    FirewallPlan      plan;
    bool              onPublic = (snapshot.active & FirewallProfile::Public) != FirewallProfile::None;
    FirewallProfile   targets  = snapshot.active;
    FirewallSnapshot  one      = snapshot;



    plan.program     = program.path;
    plan.title       = title;
    plan.markPrivate = markPrivate && onPublic;

    if (plan.markPrivate)
    {
        targets = (FirewallProfile) (((uint32_t) targets & ~(uint32_t) FirewallProfile::Public) | (uint32_t) FirewallProfile::Private);
    }

    for (const FirewallPort & port : ports)
    {
        bool  needed = false;

        for (FirewallProfile profile : { FirewallProfile::Domain, FirewallProfile::Private, FirewallProfile::Public })
        {
            if ((targets & profile) == FirewallProfile::None)
            {
                continue;
            }

            one.active = profile;

            if (!FirewallCheck::Evaluate (one, program, port.protocol, port.port).IsAdmitted())
            {
                plan.profiles = plan.profiles | profile;
                needed        = true;
            }
        }

        if (needed)
        {
            plan.ports.push_back (port);
        }
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::MakeRuleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallRemedy::MakeRuleName (const FirewallPlan & plan, const FirewallPort & port)
{
    return std::format (L"{}, {} {}", plan.title, port.protocol == HostSocketKind::Udp ? L"UDP" : L"TCP", port.port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::IsBlocking
//
////////////////////////////////////////////////////////////////////////////////

bool FirewallRemedy::IsBlocking (const FirewallRule & rule, const FirewallPlan & plan, const FirewallPort & port)
{
    FirewallProgram  program;
    bool             covers  = (rule.profiles & plan.profiles) != FirewallProfile::None;



    program.path = plan.program;

    return rule.enabled && !rule.allow && covers && FirewallCheck::Applies (rule, program, port.protocol, port.port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::IsPlanRule
//
////////////////////////////////////////////////////////////////////////////////

bool FirewallRemedy::IsPlanRule (const FirewallRule & rule, const FirewallPlan & plan, const FirewallPort & port)
{
    return rule.allow
        && rule.name == MakeRuleName (plan, port)
        && _wcsicmp (rule.application.c_str(), plan.program.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::FormatArguments
//
//  Fields separated by |, which no path or title has: a format version, the
//  profiles, whether to mark networks private, the title, the program, and
//  the ports, each U or T and its number.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallRemedy::FormatArguments (const FirewallPlan & plan)
{
    std::wstring  ports;



    for (const FirewallPort & port : plan.ports)
    {
        ports += std::format (L"{}{}{}", ports.empty() ? L"" : L",", port.protocol == HostSocketKind::Udp ? L"U" : L"T", port.port);
    }

    return std::format (L"1|{}|{}|{}|{}|{}", (uint32_t) plan.profiles, plan.markPrivate ? 1 : 0, plan.title, plan.program, ports);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::ParseArguments
//
//  The helper runs elevated, so every field is checked before the plan is
//  used: a known version, profiles among the three, and each port a protocol
//  and a number from 1 to 65535.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FirewallRemedy::ParseArguments (const std::wstring & text, FirewallPlan & outPlan)
{
    HRESULT                    hr       = S_OK;
    std::vector<std::wstring>  fields;
    size_t                     start    = 0;
    unsigned long              profiles = 0;
    FirewallPlan               plan;
    bool                       valid    = false;



    while (start <= text.size())
    {
        size_t  bar = text.find (L'|', start);

        fields.push_back (text.substr (start, bar == std::wstring::npos ? std::wstring::npos : bar - start));
        start = (bar == std::wstring::npos) ? text.size() + 1 : bar + 1;
    }

    valid = fields.size() == 6 && fields[0] == L"1" && (fields[2] == L"0" || fields[2] == L"1") && !fields[3].empty() && !fields[4].empty()
         && !fields[1].empty() && fields[1].find_first_not_of (L"0123456789") == std::wstring::npos;
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    profiles = wcstoul (fields[1].c_str(), nullptr, 10);
    valid    = profiles <= (unsigned long) FirewallProfile::All;
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    plan.profiles    = (FirewallProfile) profiles;
    plan.markPrivate = fields[2] == L"1";
    plan.title       = fields[3];
    plan.program     = fields[4];

    start = 0;

    while (!fields[5].empty() && start <= fields[5].size())
    {
        size_t         comma  = fields[5].find (L',', start);
        std::wstring   entry  = fields[5].substr (start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
        unsigned long  number = 0;
        FirewallPort   port;

        valid = entry.size() >= 2 && (entry[0] == L'U' || entry[0] == L'T') && entry.find_first_not_of (L"0123456789", 1) == std::wstring::npos;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        number = wcstoul (entry.c_str() + 1, nullptr, 10);
        valid  = number >= 1 && number <= 65535;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        port.protocol = (entry[0] == L'U') ? HostSocketKind::Udp : HostSocketKind::Tcp;
        port.port     = (uint16_t) number;
        plan.ports.push_back (port);

        start = (comma == std::wstring::npos) ? fields[5].size() + 1 : comma + 1;
    }

    outPlan = std::move (plan);

Error:
    return hr;
}
