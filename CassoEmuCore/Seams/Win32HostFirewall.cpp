#include "Pch.h"

#include "Seams/Win32HostFirewall.h"
#include "Update/Sha256Digest.h"

#pragma comment(lib, "userenv.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::Read
//
//  A rule that cannot be read is left out rather than failing the read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::Read (FirewallSnapshot & outSnapshot)
{
    HRESULT                                hr       = S_OK;
    Microsoft::WRL::ComPtr<INetFwPolicy2>  policy;
    Microsoft::WRL::ComPtr<INetFwRules>    rules;
    Microsoft::WRL::ComPtr<IUnknown>       unknown;
    Microsoft::WRL::ComPtr<IEnumVARIANT>   items;
    VARIANT_BOOL                           on       = VARIANT_FALSE;
    FirewallSnapshot                       snapshot;



    snapshot.enabled = FirewallProfile::None;

    hr = CoCreateInstance (__uuidof (NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&policy));
    CHR (hr);

    hr = ReadConnectedProfiles (policy.Get(), snapshot.active);
    CHR (hr);

    for (FirewallProfile profile : { FirewallProfile::Domain, FirewallProfile::Private, FirewallProfile::Public })
    {
        hr = policy->get_FirewallEnabled ((NET_FW_PROFILE_TYPE2) profile, &on);
        CHR (hr);

        snapshot.enabled = (on == VARIANT_TRUE) ? (snapshot.enabled | profile) : snapshot.enabled;
    }

    hr = policy->get_Rules (&rules);
    CHR (hr);

    hr = rules->get__NewEnum (&unknown);
    CHR (hr);

    hr = unknown.As (&items);
    CHR (hr);

    for (;;)
    {
        VARIANT                              item     = {};
        ULONG                                fetched  = 0;
        Microsoft::WRL::ComPtr<INetFwRule>   rule;
        FirewallRule                         read;
        bool                                 inbound  = false;
        HRESULT                              hrRule   = S_OK;

        VariantInit (&item);

        if (items->Next (1, &item, &fetched) != S_OK || fetched == 0)
        {
            break;
        }

        hrRule = (item.vt == VT_DISPATCH && item.pdispVal != nullptr) ? item.pdispVal->QueryInterface (IID_PPV_ARGS (&rule)) : E_NOINTERFACE;

        if (SUCCEEDED (hrRule))
        {
            hrRule = ReadRule (rule.Get(), inbound, read);
        }

        if (SUCCEEDED (hrRule) && inbound)
        {
            snapshot.rules.push_back (std::move (read));
        }

        VariantClear (&item);
    }

    outSnapshot = std::move (snapshot);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::ReadActiveProfiles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::ReadActiveProfiles (FirewallProfile & outActive)
{
    HRESULT                                hr     = S_OK;
    Microsoft::WRL::ComPtr<INetFwPolicy2>  policy;



    hr = CoCreateInstance (__uuidof (NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&policy));
    CHR (hr);

    hr = ReadConnectedProfiles (policy.Get(), outActive);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::ReadConnectedProfiles
//
//  The firewall's current profiles include the public profile for any
//  interface with no network identified, such as Hyper-V's Default Switch,
//  which no other computer on the LAN sends through. The connected networks'
//  categories are the networks traffic arrives on. When the network list
//  cannot be read or has no connected network, the firewall's current
//  profiles are used.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::ReadConnectedProfiles (INetFwPolicy2 * policy, FirewallProfile & outActive)
{
    HRESULT                                       hr        = S_OK;
    HRESULT                                       hrList    = S_OK;
    Microsoft::WRL::ComPtr<INetworkListManager>   manager;
    Microsoft::WRL::ComPtr<IEnumNetworks>         networks;
    FirewallProfile                               connected = FirewallProfile::None;
    long                                          current   = 0;



    hrList = CoCreateInstance (CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS (&manager));

    if (SUCCEEDED (hrList))
    {
        hrList = manager->GetNetworks (NLM_ENUM_NETWORK_CONNECTED, &networks);
    }

    for (;;)
    {
        Microsoft::WRL::ComPtr<INetwork>  network;
        ULONG                             fetched    = 0;
        NLM_NETWORK_CATEGORY              category   = NLM_NETWORK_CATEGORY_PUBLIC;
        HRESULT                           hrCategory = S_OK;

        if (FAILED (hrList) || networks->Next (1, &network, &fetched) != S_OK || fetched == 0)
        {
            break;
        }

        hrCategory = network->GetCategory (&category);

        if (SUCCEEDED (hrCategory))
        {
            connected = connected | GetCategoryProfile (category);
        }
    }

    if (connected == FirewallProfile::None)
    {
        hr = policy->get_CurrentProfileTypes (&current);
        CHR (hr);

        connected = (FirewallProfile) ((uint32_t) current & (uint32_t) FirewallProfile::All);
    }

    outActive = connected;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::GetCategoryProfile
//
////////////////////////////////////////////////////////////////////////////////

FirewallProfile Win32HostFirewall::GetCategoryProfile (NLM_NETWORK_CATEGORY category)
{
    FirewallProfile  profile = FirewallProfile::Public;



    if (category == NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED)
    {
        profile = FirewallProfile::Domain;
    }
    else if (category == NLM_NETWORK_CATEGORY_PRIVATE)
    {
        profile = FirewallProfile::Private;
    }

    return profile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::ReadRule
//
//  A rule's path can be written with environment variables, as
//  %ProgramFiles%, so it is expanded to compare with a program's path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::ReadRule (INetFwRule * rule, bool & outInbound, FirewallRule & outRule)
{
    HRESULT                               hr                 = S_OK;
    HRESULT                               hrRule3            = S_OK;
    Microsoft::WRL::ComPtr<INetFwRule3>   rule3;
    NET_FW_RULE_DIRECTION                 direction          = NET_FW_RULE_DIR_IN;
    NET_FW_ACTION                         action             = NET_FW_ACTION_ALLOW;
    long                                  protocol           = NET_FW_IP_PROTOCOL_ANY;
    long                                  profiles           = 0;
    VARIANT_BOOL                          enabled            = VARIANT_FALSE;
    BSTR                                  text               = nullptr;
    std::wstring                          path;
    wchar_t                               expanded[MAX_PATH] = {};
    DWORD                                 length             = 0;



    outInbound = false;

    hr = rule->get_Direction (&direction);
    CHR (hr);

    BAIL_OUT_IF (direction != NET_FW_RULE_DIR_IN, S_OK);

    hr = rule->get_Protocol (&protocol);
    CHR (hr);

    BAIL_OUT_IF (protocol != NET_FW_IP_PROTOCOL_ANY && protocol != NET_FW_IP_PROTOCOL_TCP && protocol != NET_FW_IP_PROTOCOL_UDP, S_OK);

    outRule.anyProtocol = protocol == NET_FW_IP_PROTOCOL_ANY;
    outRule.protocol    = (protocol == NET_FW_IP_PROTOCOL_UDP) ? HostSocketKind::Udp : HostSocketKind::Tcp;

    hr = rule->get_Name (&text);
    CHR (hr);
    outRule.name = TakeString (text);

    hr = rule->get_ApplicationName (&text);
    CHR (hr);
    path = TakeString (text);

    length              = path.empty() ? 0 : ExpandEnvironmentStringsW (path.c_str(), expanded, ARRAYSIZE (expanded));
    outRule.application = (length > 0 && length <= ARRAYSIZE (expanded)) ? std::wstring (expanded) : path;

    hrRule3 = rule->QueryInterface (IID_PPV_ARGS (&rule3));

    if (SUCCEEDED (hrRule3))
    {
        hr = rule3->get_LocalAppPackageId (&text);
        CHR (hr);
        outRule.package = TakeString (text);

        hr = rule3->get_LocalUserOwner (&text);
        CHR (hr);
        outRule.owner = TakeString (text);
    }

    hr = rule->get_LocalPorts (&text);
    CHR (hr);
    outRule.localPorts = TakeString (text);

    hr = rule->get_Profiles (&profiles);
    CHR (hr);
    outRule.profiles = (FirewallProfile) ((uint32_t) profiles & (uint32_t) FirewallProfile::All);

    hr = rule->get_Action (&action);
    CHR (hr);
    outRule.allow = action == NET_FW_ACTION_ALLOW;

    hr = rule->get_Enabled (&enabled);
    CHR (hr);
    outRule.enabled = enabled == VARIANT_TRUE;

    hr = ReadAnyAddress (rule, outRule.anyAddress);
    CHR (hr);

    outInbound = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::ReadAnyAddress
//
//  Whether a rule covers every local and remote address and every interface:
//  "*" for each address list, no interface list, and "All" for the interface
//  types.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::ReadAnyAddress (INetFwRule * rule, bool & outAny)
{
    HRESULT       hr         = S_OK;
    BSTR          text       = nullptr;
    VARIANT       interfaces = {};
    std::wstring  local;
    std::wstring  remote;
    std::wstring  types;



    VariantInit (&interfaces);

    hr = rule->get_LocalAddresses (&text);
    CHR (hr);
    local = TakeString (text);

    hr = rule->get_RemoteAddresses (&text);
    CHR (hr);
    remote = TakeString (text);

    hr = rule->get_InterfaceTypes (&text);
    CHR (hr);
    types = TakeString (text);

    hr = rule->get_Interfaces (&interfaces);
    CHR (hr);

    outAny = (local.empty() || local == L"*") && (remote.empty() || remote == L"*")
          && (types.empty() || _wcsicmp (types.c_str(), L"All") == 0)
          && (interfaces.vt == VT_EMPTY || interfaces.vt == VT_NULL);

Error:
    VariantClear (&interfaces);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::TakeString
//
//  The string is freed; a null BSTR is the empty string.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32HostFirewall::TakeString (BSTR text)
{
    std::wstring  taken = (text != nullptr) ? std::wstring (text, SysStringLen (text)) : std::wstring();



    SysFreeString (text);

    return taken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::ReadThisProgram
//
////////////////////////////////////////////////////////////////////////////////

FirewallProgram Win32HostFirewall::ReadThisProgram()
{
    FirewallProgram  program;
    HRESULT          hr = GetThisProgram (program);



    IGNORE_RETURN_VALUE (hr, S_OK);

    return program;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::GetThisProgram
//
//  A packaged process's capability rules are written for its package's SID,
//  which is derived from its family name. An unpackaged process doesn't have one.
//  The path is returned even when the SID can't be made: the rules a
//  package declares for its programs are written for their paths, and with
//  no path every port reads as blocked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::GetThisProgram (FirewallProgram & outProgram)
{
    HRESULT  hr                                         = S_OK;
    wchar_t  path[MAX_PATH]                             = {};
    wchar_t  family[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1] = {};
    UINT32   familyLength                               = ARRAYSIZE (family);
    DWORD    length                                     = 0;
    bool     fits                                       = false;
    LONG     packaged                                   = ERROR_SUCCESS;



    outProgram = FirewallProgram();

    length = GetModuleFileNameW (nullptr, path, ARRAYSIZE (path));
    fits   = length > 0 && length < ARRAYSIZE (path);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    outProgram.path = path;

    packaged = GetCurrentPackageFamilyName (&familyLength, family);
    BAIL_OUT_IF (packaged != ERROR_SUCCESS, S_OK);


    hr = MakePackageSid (family, outProgram.package);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32HostFirewall::MakePackageSid
//
//  S-1-15-2 and the first seven 32-bit values, little-endian, of the
//  SHA-256 of the lowercase name in UTF-16LE: the SID
//  DeriveAppContainerSidFromAppContainerName derives. In a packaged process
//  that call returns a SID for which IsValidSid returns FALSE, so the SID is
//  computed here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32HostFirewall::MakePackageSid (const std::wstring & familyName, std::wstring & outSid)
{
    static constexpr size_t  kParts = 7;
    HRESULT                  hr     = S_OK;
    std::vector<Byte>        bytes;
    std::vector<Byte>        digest;
    std::wstring             sid    = L"S-1-15-2";



    for (wchar_t ch : familyName)
    {
        wchar_t  lower = (wchar_t) towlower (ch);

        bytes.push_back ((Byte) (lower & 0xFF));
        bytes.push_back ((Byte) (lower >> 8));
    }

    hr = Sha256Digest::Compute (bytes, digest);
    CHR (hr);

    for (size_t part = 0; part < kParts; part++)
    {
        uint32_t  value = (uint32_t) digest[part * 4] | ((uint32_t) digest[part * 4 + 1] << 8) | ((uint32_t) digest[part * 4 + 2] << 16) | ((uint32_t) digest[part * 4 + 3] << 24);

        sid += std::format (L"-{}", value);
    }

    outSid = std::move (sid);

Error:
    return hr;
}
