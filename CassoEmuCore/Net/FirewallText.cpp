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
