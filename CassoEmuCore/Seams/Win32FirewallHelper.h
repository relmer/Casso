#pragma once

#include "Pch.h"

#include "Net/FirewallRemedy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FirewallHelper
//
//  The program run again elevated, with kSwitch and a FirewallPlan, to
//  change Windows Firewall: Launch starts it, which raises the UAC prompt,
//  and the caller checks the process from its poll with TryFinish rather
//  than waiting on it. TryRunFromCommandLine is called first thing in each
//  program's entry point and applies the plan.
//
//  Only a program in the helper's folder is admitted, so a plan cannot open
//  a port for some other program.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FirewallHelper
{
public:
    static constexpr const wchar_t *  kSwitch = L"--firewall-helper";

    //  Fails with ERROR_CANCELLED when the user declines the UAC prompt.
    static HRESULT  Launch    (HWND owner, const FirewallPlan & plan, HANDLE & outProcess);

    //  Whether the helper exited, and its result; the handle is closed once
    //  it has.
    static bool     TryFinish (HANDLE & process, HRESULT & outResult);

    //  True for a helper's command line, which is then run, with its exit
    //  code; false for every other command line.
    static bool     TryRunFromCommandLine (int & outExitCode);

private:
    static HRESULT  Apply               (const FirewallPlan & plan);
    static HRESULT  AddRule             (INetFwRules * rules, const FirewallPlan & plan, const FirewallPort & port);
    static HRESULT  MarkNetworksPrivate ();
    static HRESULT  CheckProgramFolder  (const std::wstring & program);
};
