#pragma once

#include "Pch.h"

#include "Seams/RawDevice/Win32PipeStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardProxy
//
//  CassoStorageCardProxy.exe, the elevated program that reads and writes
//  storage cards for Casso Explorer, and Casso Explorer's launch of it.
//
//  Casso Explorer creates a pipe with a new random name that only SYSTEM
//  and elevated administrators may open, and starts the proxy with the
//  name through the UAC prompt. Casso Explorer accepts the connection only
//  from the process it started. The proxy connects without allowing
//  impersonation, and serves the pipe only when its server is
//  CassoExplorer.exe in the proxy's folder, signed by the same publisher
//  when the proxy is signed. The proxy serves one connection and exits when
//  the pipe closes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32StorageCardProxy
{
public:
    static constexpr const wchar_t *  kProgramFile  = L"CassoStorageCardProxy.exe";
    static constexpr const wchar_t *  kExplorerFile = L"CassoExplorer.exe";

    //  Returns once the proxy has connected, with `stream` attached to the
    //  pipe. Fails with ERROR_CANCELLED when the user declines the UAC
    //  prompt, with the proxy's exit code when it exits before connecting,
    //  with E_ACCESSDENIED when another process connects, and with
    //  ERROR_TIMEOUT when nothing connects in kConnectTimeoutMs. The caller
    //  closes `outProcess`.
    static HRESULT  Launch (HWND owner, Win32PipeStream & stream, HANDLE & outProcess);

    //  The proxy's work, from its command line of the pipe's name alone.
    static HRESULT  Run    (const wchar_t * commandLine);

    //  A pipe name is the prefix and hexadecimal digits, nothing else.
    static bool     IsValidPipeName (const std::wstring & name);

    //  The file in the same folder as this program.
    static HRESULT  GetSiblingPath  (const wchar_t * file, std::wstring & outPath);

private:
    static constexpr const wchar_t *  kPipePrefix       = L"CassoStorageCard";
    static constexpr size_t           kMaxNameChars     = 64;
    static constexpr DWORD            kConnectTimeoutMs = 30000;

    static HRESULT  MakePipeName  (std::wstring & outName);
    static HRESULT  CreatePipe    (const std::wstring & name, HANDLE & outPipe);
    static HRESULT  WaitForClient (HANDLE pipe, HANDLE process);
    static HRESULT  CheckClient   (HANDLE pipe, HANDLE process);
    static HRESULT  CheckServer   (HANDLE pipe);
};
