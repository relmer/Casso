#pragma once

#include "Pch.h"

#include "Seams/RawDevice/Win32PipeStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardBroker
//
//  CassoStorageCardBroker.exe, the elevated program that reads and writes
//  storage cards for Casso Explorer, and Casso Explorer's launch of it.
//
//  Casso Explorer creates a pipe with a new random name that only SYSTEM
//  and elevated administrators may open, and starts the broker with the
//  name through the UAC prompt. Casso Explorer accepts the connection only
//  from the process it started. The broker connects without allowing
//  impersonation, and serves the pipe only when its server is
//  CassoExplorer.exe in the broker's folder, signed by the same publisher
//  when the broker is signed. The broker serves one connection and exits when
//  the pipe closes.
//
////////////////////////////////////////////////////////////////////////////////

class Win32StorageCardBroker
{
public:
    static constexpr const wchar_t *  kProgramFile  = L"CassoStorageCardBroker.exe";
    static constexpr const wchar_t *  kExplorerFile = L"CassoExplorer.exe";

    //  Returns once the broker has connected, with `stream` attached to the
    //  pipe. Fails with ERROR_CANCELLED when the user declines the UAC
    //  prompt, with the broker's exit code when it exits before connecting,
    //  with E_ACCESSDENIED when another process connects, and with
    //  ERROR_TIMEOUT when nothing connects in kConnectTimeoutMs. The caller
    //  closes `outProcess`.
    static HRESULT  Launch (HWND owner, Win32PipeStream & stream, HANDLE & outProcess);

    //  The broker's work, from its command line of the pipe's name alone.
    static HRESULT  Run    (const wchar_t * commandLine);

    //  A pipe name is the prefix and hexadecimal digits, nothing else.
    static bool     IsValidPipeName (const std::wstring & name);

    //  A line of the broker's log, StorageCardBroker.log in Casso's log folder.
    static void     AppendLog       (const std::wstring & line);

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
