#pragma once

#include "Pch.h"

#include "Seams/IProcessLauncher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchCommand
//
//  How the emulator starts the disk browser and how the browser starts the
//  emulator: which executable, beside which module, with which arguments.
//
//  Both executables sit in one output directory, so each finds the other
//  beside its own module rather than through a search that could pick up a
//  copy from another build.
//
////////////////////////////////////////////////////////////////////////////////

class LaunchCommand
{
public:
    enum class Outcome { Launched, Missing, Failed };

    static constexpr const wchar_t *  kCassqueExe = L"Cassque.exe";
    static constexpr const wchar_t *  kCassoExe   = L"Casso.exe";

    //  `--owner <decimal window>`, and `--title <prefix>` when there is one.
    static std::wstring  MakeCassqueArguments (HWND owner, const std::wstring & titlePrefix);

    //  `--disk1 "<path>"`, and `--title <prefix>` when there is one.
    static std::wstring  MakeCassoArguments (const std::wstring & diskPath, const std::wstring & titlePrefix);

    //  One argument in the quoting CommandLineToArgvW reads back unchanged.
    static std::wstring  QuoteArgument (const std::wstring & argument);

    static std::wstring  GetSiblingPath (const std::wstring & moduleDirectory, const wchar_t * exeName);

    //  Starts the browser beside `moduleDirectory`, or reports it missing
    //  without trying.
    static Outcome  LaunchCassque (IProcessLauncher    & launcher,
                                   const std::wstring  & moduleDirectory,
                                   HWND                  owner,
                                   const std::wstring  & titlePrefix);

    //  The sentence shown when the browser is not beside the emulator.
    static std::wstring  DescribeMissing (const std::wstring & exePath);
};
