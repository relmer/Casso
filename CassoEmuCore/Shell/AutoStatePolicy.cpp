#include "Pch.h"

#include "Shell/AutoStatePolicy.h"
#include "Shell/MachineStateFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy::ShouldResumeAtStartup
//
//  With the setting on, the machine starts from its automatic state when it
//  has one, except in a launch whose command line mounts a disk or a tape:
//  that launch starts cold with the media on its command line.
//
////////////////////////////////////////////////////////////////////////////////

bool AutoStatePolicy::ShouldResumeAtStartup (
    bool  isEnabled,
    bool  hasCommandLineMedia,
    bool  hasAutoState)
{
    return isEnabled && !hasCommandLineMedia && hasAutoState;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy::ShouldResumeOnSwitch
//
//  A switch to a machine resumes its automatic state, except a switch made
//  to load a state file, whose state replaces the machine's anyway.
//
////////////////////////////////////////////////////////////////////////////////

bool AutoStatePolicy::ShouldResumeOnSwitch (
    bool  isEnabled,
    bool  isLoadingState,
    bool  hasAutoState)
{
    return isEnabled && !isLoadingState && hasAutoState;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy::GetSaveAction
//
//  With the setting on, the machine's state is written at exit and on a
//  switch away from it. The machine a command-line launch started with its
//  own media replaces an existing automatic state only after a prompt,
//  since that state is the user's earlier session; with none there, it is
//  written with no prompt.
//
////////////////////////////////////////////////////////////////////////////////

AutoStateSave AutoStatePolicy::GetSaveAction (
    bool  isEnabled,
    bool  isCommandLineMediaMachine,
    bool  hasAutoState)
{
    AutoStateSave  action = AutoStateSave::Write;



    if (!isEnabled)
    {
        action = AutoStateSave::Skip;
    }
    else if (isCommandLineMediaMachine && hasAutoState)
    {
        action = AutoStateSave::PromptFirst;
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy::GetAutoStatePath
//
//  <stateDirectory>\<machineName>.autosave.cassostate, with any character
//  Windows does not allow in a file name replaced by an underscore.
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path AutoStatePolicy::GetAutoStatePath (
    const std::filesystem::path  & stateDirectory,
    const std::wstring           & machineName)
{
    constexpr const wchar_t *  kNotInFileNames = L"\\/:*?\"<>|";
    std::wstring               fileName        = machineName;



    for (wchar_t & ch : fileName)
    {
        if (ch < L' ' || wcschr (kNotInFileNames, ch) != nullptr)
        {
            ch = L'_';
        }
    }

    fileName += kAutoStateSuffix;
    fileName += L'.';
    fileName += MachineStateFile::kExtension;

    return stateDirectory / fileName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoStatePolicy::FormatResumeFailure
//
//  The notice for an automatic state that did not resume, from the machine's
//  display name and the MachineStateError label. The reason is the other
//  version, machine or ROM set a state is from, or else that the state is
//  damaged.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AutoStatePolicy::FormatResumeFailure (
    const std::wstring  & machineDisplayName,
    const std::string   & errorLabel)
{
    const wchar_t  * reason = L"damaged";



    if (errorLabel == "newer state format")
    {
        reason = L"from a newer version of Casso";
    }
    else if (errorLabel == "different ROM images")
    {
        reason = L"from a machine with different ROM images";
    }
    else if (errorLabel == "different machine")
    {
        reason = L"from a different kind of machine";
    }
    else if (errorLabel == "file not read")
    {
        reason = L"unreadable";
    }

    return std::format (L"Could not resume the {} because its saved state is {}.", machineDisplayName, reason);
}





