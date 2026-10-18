#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStateSave
//
//  What to do with a machine's automatic state at exit or on a switch away
//  from the machine.
//
////////////////////////////////////////////////////////////////////////////////

enum class AutoStateSave
{
    Skip,
    Write,
    PromptFirst,
};





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy
//
//  When a machine's automatic state is resumed, written, or written only
//  after the user chooses Replace in a prompt, its path, and the notice when
//  it does not resume. Pure: the shell supplies the facts and acts on the
//  results.
//
////////////////////////////////////////////////////////////////////////////////

class AutoStatePolicy
{
public:
    static constexpr const wchar_t *  kAutoStateSuffix = L".autosave";

    static bool                   ShouldResumeAtStartup (bool isEnabled, bool hasCommandLineMedia, bool hasAutoState);
    static bool                   ShouldResumeOnSwitch  (bool isEnabled, bool isLoadingState, bool hasAutoState);
    static AutoStateSave          GetSaveAction         (bool isEnabled, bool isCommandLineMediaMachine, bool hasAutoState);
    static std::filesystem::path  GetAutoStatePath      (const std::filesystem::path & stateDirectory, const std::wstring & machineName);
    static std::wstring           FormatResumeFailure   (const std::wstring & machineDisplayName, const std::string & errorLabel);
};
