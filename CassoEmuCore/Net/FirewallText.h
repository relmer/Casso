#pragma once

#include "Pch.h"

#include "Net/FirewallCheck.h"
#include "Net/FirewallRemedy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FirewallText
//
//  The words for a verdict that is not admitted, shared by Casso Explorer's
//  connections and Casso's Network settings page, so the text in both is
//  the same: which of the three causes it is, and on which kind of network.
//
////////////////////////////////////////////////////////////////////////////////

class FirewallText
{
public:
    //  As "Windows Firewall blocks UDP port 6510 for Casso Explorer on this
    //  public network (rule "cassoexplorer.exe")."; empty for an admitted
    //  verdict.
    static std::wstring  DescribeCause   (const FirewallVerdict & verdict, const std::wstring & program, const FirewallPort & port);

    //  The two remedies' button labels.
    static constexpr const wchar_t *  kAllowAction   = L"Allow in Windows Firewall";
    static constexpr const wchar_t *  kPrivateAction = L"Make this network private";

    //  What follows the cause on a public network, offering both remedies.
    static constexpr const wchar_t *  kPublicChoice  = L"Allow the port on public networks, or make this network private if it is one you trust, such as your home network.";

    static const wchar_t *  GetProfileName (FirewallProfile profile);
};
