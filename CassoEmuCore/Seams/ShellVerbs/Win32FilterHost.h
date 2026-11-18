#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost
//
//  The process a file's IFilter runs in: a copy of this program, started with
//  kSwitch and with pipes for its standard input and output. A filter is
//  third-party code that can crash or hang, so Win32FileContents runs it
//  here, and a filter that fails costs the file and not the program that is
//  searching.
//
//  The host serves requests until its input closes. A request is the length
//  of a path in UTF-16 code units, as a uint32_t, then the path. A reply is
//  a ReplyHeader, then `length` UTF-16 code units of the file's text. A
//  filter that fails sets `status` to a failure HRESULT and `length` to zero.
//
//  The host has no window or dialog, so its code uses only non-asserting
//  macros.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FilterHost
{
public:
    static constexpr const wchar_t *  kSwitch = L"--filter-host";

    //  The most UTF-16 code units a reply contains, and the longest path a
    //  request contains.
    static constexpr uint32_t  kMaxChars       = 16 * 1024 * 1024;
    static constexpr uint32_t  kMaxRequestPath = 32768;

    //  A reply's fixed part.
    struct ReplyHeader
    {
        int32_t   status = 0;
        uint32_t  length = 0;
    };

    //  True for the host's command line, which is then run, with its exit
    //  code; false for every other command line.
    static bool  TryRunFromCommandLine (int & outExitCode);

private:
    static constexpr DWORD  kReadBufferChars = 4096;

    static HRESULT  Serve             ();
    static HRESULT  ReadExact         (HANDLE input, void * buffer, DWORD count);
    static HRESULT  WriteAll          (HANDLE output, const void * buffer, DWORD count);
    static HRESULT  ReadRequest       (HANDLE input, std::wstring & outPath);
    static HRESULT  WriteReply        (HANDLE output, HRESULT status, const std::wstring & text);
    static HRESULT  LoadFilter        (const std::wstring & path, IFilter ** outFilter);
    static HRESULT  CreateFilter      (const std::wstring & path, IFilter ** outFilter);
    static HRESULT  InitializeFilter  (IFilter * filter, const std::wstring & path);
    static HRESULT  ExtractText       (const std::wstring & path, std::wstring & outText);
    static void     ReadMetadata      (const std::wstring & path, std::wstring & text);
    static void     ReadChunkText     (IFilter * filter, std::wstring & text);
    static void     ReadChunkValues   (IFilter * filter, std::wstring & text);
    static void     AppendValue       (const PROPVARIANT & value, std::wstring & text);
};
