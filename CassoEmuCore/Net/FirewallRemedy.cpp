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

            for (uint32_t number = port.GetFirstPort(); number <= port.GetLastPort(); number++)
            {
                if (!FirewallCheck::Evaluate (one, program, port.protocol, (uint16_t) number).IsAdmitted())
                {
                    plan.profiles = plan.profiles | profile;
                    needed        = true;
                }
            }
        }

        //  The plan has the range alone, from its first port.
        if (needed)
        {
            plan.ports.push_back ({ port.protocol, port.GetFirstPort(), port.GetLastPort() == port.GetFirstPort() ? (uint16_t) 0 : port.GetLastPort() });
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
    return std::format (L"{}, {} {}", plan.title, port.protocol == HostSocketKind::Udp ? L"UDP" : L"TCP", FormatPorts (port));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::FormatPorts
//
//  As Windows Firewall lists a rule's ports: "6510", or "6502-6509" for a
//  range.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallRemedy::FormatPorts (const FirewallPort & port)
{
    bool  range = port.GetLastPort() != port.GetFirstPort();



    return range ? std::format (L"{}-{}", port.GetFirstPort(), port.GetLastPort()) : std::to_wstring (port.GetFirstPort());
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
    bool             applies = false;



    program.path = plan.program;

    for (uint32_t number = port.GetFirstPort(); !applies && number <= port.GetLastPort(); number++)
    {
        applies = FirewallCheck::Applies (rule, program, port.protocol, (uint16_t) number);
    }

    return rule.enabled && !rule.allow && covers && applies;
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
//  the ports, each U or T and its number or its range, as T6502-6509.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallRemedy::FormatArguments (const FirewallPlan & plan)
{
    std::wstring  ports;



    for (const FirewallPort & port : plan.ports)
    {
        ports += std::format (L"{}{}{}", ports.empty() ? L"" : L",", port.protocol == HostSocketKind::Udp ? L"U" : L"T", FormatPorts (port));
    }

    return std::format (L"1|{}|{}|{}|{}|{}", (uint32_t) plan.profiles, plan.markPrivate ? 1 : 0, plan.title, plan.program, ports);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallRemedy::ParseArguments
//
//  The helper runs elevated, so every field is checked before the plan is
//  used: a known version, profiles among the three, and each port a protocol
//  and a number from 1 to 65535, and a range's end no lower than its start.
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
        size_t         dash   = entry.find (L'-');
        std::wstring   first  = entry.substr (0, dash);
        std::wstring   last   = (dash == std::wstring::npos) ? std::wstring() : entry.substr (dash + 1);
        unsigned long  number = 0;
        unsigned long  end    = 0;
        FirewallPort   port;

        valid = first.size() >= 2 && (first[0] == L'U' || first[0] == L'T') && first.find_first_not_of (L"0123456789", 1) == std::wstring::npos
             && (dash == std::wstring::npos || (!last.empty() && last.find_first_not_of (L"0123456789") == std::wstring::npos));
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        number = wcstoul (first.c_str() + 1, nullptr, 10);
        end    = (dash == std::wstring::npos) ? number : wcstoul (last.c_str(), nullptr, 10);
        valid  = number >= 1 && number <= 65535 && end >= number && end <= 65535;
        CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        port.protocol = (first[0] == L'U') ? HostSocketKind::Udp : HostSocketKind::Tcp;
        port.port     = (uint16_t) number;
        port.lastPort = (end == number) ? (uint16_t) 0 : (uint16_t) end;
        plan.ports.push_back (port);

        start = (comma == std::wstring::npos) ? fields[5].size() + 1 : comma + 1;
    }

    outPlan = std::move (plan);

Error:
    return hr;
}
