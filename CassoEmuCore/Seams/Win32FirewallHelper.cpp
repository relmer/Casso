#include "Pch.h"

#include "Seams/Win32FirewallHelper.h"
#include "Seams/Win32HostFirewall.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::Launch
//
//  The window is hidden: the helper has no UI beyond the UAC prompt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FirewallHelper::Launch (HWND owner, const FirewallPlan & plan, HANDLE & outProcess)
{
    HRESULT            hr               = S_OK;
    wchar_t            self[MAX_PATH]   = {};
    DWORD              length           = 0;
    bool               fits             = false;
    std::wstring       parameters;
    SHELLEXECUTEINFOW  info             = {};
    BOOL               started          = FALSE;



    outProcess = nullptr;

    length = GetModuleFileNameW (nullptr, self, ARRAYSIZE (self));
    fits   = length > 0 && length < ARRAYSIZE (self);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    parameters = std::format (L"{} \"{}\"", kSwitch, FirewallRemedy::FormatArguments (plan));

    info.cbSize       = sizeof (info);
    info.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
    info.hwnd         = owner;
    info.lpVerb       = L"runas";
    info.lpFile       = self;
    info.lpParameters = parameters.c_str();
    info.nShow        = SW_HIDE;

    started = ShellExecuteExW (&info);
    CWR (started);

    outProcess = info.hProcess;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::TryFinish
//
//  The helper's exit code is its HRESULT.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FirewallHelper::TryFinish (HANDLE & process, HRESULT & outResult)
{
    DWORD  exitCode = 0;
    bool   finished = process == nullptr || WaitForSingleObject (process, 0) == WAIT_OBJECT_0;



    if (finished && process != nullptr)
    {
        outResult = GetExitCodeProcess (process, &exitCode) ? (HRESULT) exitCode : HRESULT_FROM_WIN32 (GetLastError());

        CloseHandle (process);
        process = nullptr;
    }

    return finished;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::TryRunFromCommandLine
//
//  The helper's command line is the switch and one argument, the plan.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FirewallHelper::TryRunFromCommandLine (int & outExitCode)
{
    HRESULT        hr     = S_OK;
    HRESULT        hrCom  = E_FAIL;
    int            argc   = 0;
    LPWSTR       * argv   = CommandLineToArgvW (GetCommandLineW(), &argc);
    bool           helper = argv != nullptr && argc == 3 && wcscmp (argv[1], kSwitch) == 0;
    FirewallPlan   plan;



    BAIL_OUT_IF (!helper, S_OK);

    hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
    CHR (hrCom);

    hr = FirewallRemedy::ParseArguments (argv[2], plan);
    CHR (hr);

    hr = Apply (plan);
    CHR (hr);

Error:
    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }

    if (argv != nullptr)
    {
        LocalFree (argv);
    }

    outExitCode = (int) hr;

    return helper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::Apply
//
//  The networks are marked private first, so the rules are written for the
//  profile they are about to be on. Each port's block rules are disabled
//  rather than deleted, so the change can be undone in Windows Defender
//  Firewall's list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FirewallHelper::Apply (const FirewallPlan & plan)
{
    HRESULT                                hr      = S_OK;
    Microsoft::WRL::ComPtr<INetFwPolicy2>  policy;
    Microsoft::WRL::ComPtr<INetFwRules>    rules;
    Microsoft::WRL::ComPtr<IUnknown>       unknown;
    Microsoft::WRL::ComPtr<IEnumVARIANT>   items;
    std::vector<bool>                      widened (plan.ports.size(), false);



    hr = CheckProgramFolder (plan.program);
    CHR (hr);

    if (plan.markPrivate)
    {
        hr = MarkNetworksPrivate();
        CHR (hr);
    }

    hr = CoCreateInstance (__uuidof (NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&policy));
    CHR (hr);

    hr = policy->get_Rules (&rules);
    CHR (hr);

    hr = rules->get__NewEnum (&unknown);
    CHR (hr);

    hr = unknown.As (&items);
    CHR (hr);

    for (;;)
    {
        VARIANT                             item     = {};
        ULONG                               fetched  = 0;
        Microsoft::WRL::ComPtr<INetFwRule>  rule;
        FirewallRule                        read;
        bool                                inbound  = false;
        HRESULT                             hrRule   = S_OK;
        HRESULT                             hrChange = S_OK;

        VariantInit (&item);

        if (items->Next (1, &item, &fetched) != S_OK || fetched == 0)
        {
            break;
        }

        //  A rule that cannot be read is left as it is.
        hrRule = (item.vt == VT_DISPATCH && item.pdispVal != nullptr) ? item.pdispVal->QueryInterface (IID_PPV_ARGS (&rule)) : E_NOINTERFACE;

        if (SUCCEEDED (hrRule))
        {
            hrRule = Win32HostFirewall::ReadRule (rule.Get(), inbound, read);
        }

        for (size_t at = 0; SUCCEEDED (hrRule) && SUCCEEDED (hrChange) && inbound && at < plan.ports.size(); at++)
        {
            if (FirewallRemedy::IsBlocking (read, plan, plan.ports[at]))
            {
                hrChange = rule->put_Enabled (VARIANT_FALSE);
            }
            else if (FirewallRemedy::IsPlanRule (read, plan, plan.ports[at]))
            {
                hrChange    = rule->put_Profiles ((long) ((uint32_t) read.profiles | (uint32_t) plan.profiles));
                widened[at] = SUCCEEDED (hrChange);
            }
        }

        VariantClear (&item);

        hr = hrChange;
        CHR (hr);
    }

    for (size_t at = 0; at < plan.ports.size(); at++)
    {
        if (!widened[at])
        {
            hr = AddRule (rules.Get(), plan, plan.ports[at]);
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::AddRule
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FirewallHelper::AddRule (INetFwRules * rules, const FirewallPlan & plan, const FirewallPort & port)
{
    HRESULT                             hr       = S_OK;
    Microsoft::WRL::ComPtr<INetFwRule>  rule;
    BSTR                                name     = SysAllocString (FirewallRemedy::MakeRuleName (plan, port).c_str());
    BSTR                                group    = SysAllocString (FirewallRemedy::kGrouping);
    BSTR                                program  = SysAllocString (plan.program.c_str());
    BSTR                                ports    = SysAllocString (std::to_wstring (port.port).c_str());
    bool                                strings  = name != nullptr && group != nullptr && program != nullptr && ports != nullptr;



    CBREx (strings, E_OUTOFMEMORY);

    hr = CoCreateInstance (__uuidof (NetFwRule), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&rule));
    CHR (hr);

    hr = rule->put_Name (name);
    CHR (hr);

    hr = rule->put_Grouping (group);
    CHR (hr);

    hr = rule->put_ApplicationName (program);
    CHR (hr);

    hr = rule->put_Protocol (port.protocol == HostSocketKind::Udp ? NET_FW_IP_PROTOCOL_UDP : NET_FW_IP_PROTOCOL_TCP);
    CHR (hr);

    hr = rule->put_LocalPorts (ports);
    CHR (hr);

    hr = rule->put_Direction (NET_FW_RULE_DIR_IN);
    CHR (hr);

    hr = rule->put_Action (NET_FW_ACTION_ALLOW);
    CHR (hr);

    hr = rule->put_Profiles ((long) plan.profiles);
    CHR (hr);

    hr = rule->put_Enabled (VARIANT_TRUE);
    CHR (hr);

    hr = rules->Add (rule.Get());
    CHR (hr);

Error:
    SysFreeString (name);
    SysFreeString (group);
    SysFreeString (program);
    SysFreeString (ports);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::MarkNetworksPrivate
//
//  Every connected network Windows calls public.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FirewallHelper::MarkNetworksPrivate()
{
    HRESULT                                       hr       = S_OK;
    Microsoft::WRL::ComPtr<INetworkListManager>   manager;
    Microsoft::WRL::ComPtr<IEnumNetworks>         networks;



    hr = CoCreateInstance (CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetNetworks (NLM_ENUM_NETWORK_CONNECTED, &networks);
    CHR (hr);

    for (;;)
    {
        Microsoft::WRL::ComPtr<INetwork>  network;
        ULONG                             fetched  = 0;
        NLM_NETWORK_CATEGORY              category = NLM_NETWORK_CATEGORY_PRIVATE;

        if (networks->Next (1, &network, &fetched) != S_OK || fetched == 0)
        {
            break;
        }

        hr = network->GetCategory (&category);
        CHR (hr);

        if (category == NLM_NETWORK_CATEGORY_PUBLIC)
        {
            hr = network->SetCategory (NLM_NETWORK_CATEGORY_PRIVATE);
            CHR (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper::CheckProgramFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FirewallHelper::CheckProgramFolder (const std::wstring & program)
{
    HRESULT   hr             = S_OK;
    wchar_t   self[MAX_PATH] = {};
    DWORD     length         = 0;
    bool      fits           = false;
    bool      sibling        = false;



    length = GetModuleFileNameW (nullptr, self, ARRAYSIZE (self));
    fits   = length > 0 && length < ARRAYSIZE (self);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    sibling = _wcsicmp (std::filesystem::path (program).parent_path().c_str(), std::filesystem::path (self).parent_path().c_str()) == 0;
    CBREx (sibling, E_ACCESSDENIED);

Error:
    return hr;
}
