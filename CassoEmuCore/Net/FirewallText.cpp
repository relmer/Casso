#include "Pch.h"

#include "Net/FirewallText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallText::DescribeCause
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallText::DescribeCause (const FirewallVerdict & verdict, const std::wstring & program, const FirewallPort & port)
{
    const wchar_t  * protocol = (port.protocol == HostSocketKind::Udp) ? L"UDP" : L"TCP";
    const wchar_t  * network  = GetProfileName (verdict.profile);
    std::wstring     text;



    switch (verdict.admission)
    {
        case FirewallAdmission::Refused:
            text = verdict.rule.empty()
                 ? std::format (L"Windows Firewall blocks {} port {} for {} on this {} network.", protocol, port.port, program, network)
                 : std::format (L"Windows Firewall blocks {} port {} for {} on this {} network (rule \"{}\").", protocol, port.port, program, network, verdict.rule);
            break;

        case FirewallAdmission::NoRule:
            text = std::format (L"Windows Firewall has no rule allowing {} port {} for {} on this {} network, so it blocks the port.",
                                protocol, port.port, program, network);
            break;

        case FirewallAdmission::OtherProfile:
            text = std::format (L"Windows Firewall allows {} port {} for {} on other kinds of network, but not on this {} one.",
                                protocol, port.port, program, network);
            break;

        default:
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallText::DescribeBlocked
//
//  The ports in the order given, separated by commas, with "and" before the
//  last.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FirewallText::DescribeBlocked (const std::wstring & program, const std::vector<FirewallPort> & ports)
{
    std::wstring  list;
    std::wstring  text;



    for (size_t i = 0; i < ports.size(); i++)
    {
        const wchar_t  * separator = (i == 0) ? L"" : (i + 1 == ports.size()) ? L" and " : L", ";

        list += std::format (L"{}{} port {}", separator, ports[i].protocol == HostSocketKind::Udp ? L"UDP" : L"TCP", ports[i].port);
    }

    if (!ports.empty())
    {
        text = std::format (L"Windows Firewall is blocking {} from listening on {}.", program, list);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallText::GetProfileName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * FirewallText::GetProfileName (FirewallProfile profile)
{
    const wchar_t  * name = L"public";



    switch (profile)
    {
        case FirewallProfile::Domain:   name = L"domain";   break;
        case FirewallProfile::Private:  name = L"private";  break;
        default:                        name = L"public";   break;
    }

    return name;
}
