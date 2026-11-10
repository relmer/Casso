#pragma once

#include "Pch.h"

#include "Seams/RawDevice/Win32PipeStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper
//
//  The program run again elevated, with kSwitch, a pipe name and a drive
//  number, to read and write the blocks of one removable drive. Launch
//  creates the pipe and starts the helper, which raises the UAC prompt.
//  TryRunFromCommandLine is called first thing in the program's entry point,
//  and runs a RawDeviceServer on the pipe until the pipe closes.
//
//  The pipe is created for the user, Administrators and SYSTEM, and one
//  instance. The helper connects without allowing impersonation, and serves
//  only a pipe whose server is a process of the helper's program.
//
////////////////////////////////////////////////////////////////////////////////

class Win32RawDeviceHelper
{
public:
    static constexpr const wchar_t *  kSwitch = L"/rawdevice";

    //  Returns once the helper has connected, with `stream` attached to the
    //  pipe. Fails with ERROR_CANCELLED when the user declines the UAC
    //  prompt, with the helper's exit code when it exits before connecting,
    //  and with ERROR_TIMEOUT when it does neither in kConnectTimeoutMs.
    //  The caller closes `outProcess`.
    static HRESULT  Launch (HWND owner, int drive, Win32PipeStream & stream, HANDLE & outProcess);

    //  True for a helper's command line, which is then run, with its exit
    //  code; false for every other command line.
    static bool     TryRunFromCommandLine (int & outExitCode);

    //  A pipe name is the prefix and hexadecimal digits, nothing else.
    static bool     IsValidPipeName (const std::wstring & name);

    //  Decimal digits only, and a number of at most four of them.
    static bool     TryParseDrive   (const std::wstring & text, int & outDrive);

private:
    static constexpr const wchar_t *  kPipePrefix       = L"CassoRawDevice";
    static constexpr size_t           kMaxNameChars     = 64;
    static constexpr DWORD            kConnectTimeoutMs = 30000;

    static HRESULT  MakePipeName  (std::wstring & outName);
    static HRESULT  GetUserSid    (std::wstring & outSid);
    static HRESULT  CreatePipe    (const std::wstring & name, HANDLE & outPipe);
    static HRESULT  WaitForClient (HANDLE pipe, HANDLE process);
    static HRESULT  CheckServer   (HANDLE pipe);
};
