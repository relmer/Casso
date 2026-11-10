#include "Pch.h"

#include "Seams/RawDevice/Win32RawDeviceHelper.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Win32RawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::Launch
//
//  The pipe exists before the helper starts, so the helper's connection
//  cannot arrive first. ShellExecuteExW returns once the user has
//  dealt with the UAC prompt, so that time is not part of the wait.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::Launch (HWND owner, int drive, Win32PipeStream & stream, HANDLE & outProcess)
{
    HRESULT            hr             = S_OK;
    wchar_t            self[MAX_PATH] = {};
    DWORD              length         = 0;
    bool               fits           = false;
    HANDLE             pipe           = INVALID_HANDLE_VALUE;
    SHELLEXECUTEINFOW  info           = {};
    BOOL               started        = FALSE;
    std::wstring       name;
    std::wstring       parameters;



    outProcess = nullptr;

    length = GetModuleFileNameW (nullptr, self, ARRAYSIZE (self));
    fits   = length > 0 && length < ARRAYSIZE (self);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    hr = MakePipeName (name);
    CHR (hr);

    hr = CreatePipe (name, pipe);
    CHR (hr);

    parameters = std::format (L"{} {} {}", kSwitch, name, drive);

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

    hr = WaitForClient (pipe, outProcess);
    CHR (hr);

    //  The stream closes the pipe from here on.
    hr   = stream.Attach (pipe);
    pipe = INVALID_HANDLE_VALUE;
    CHR (hr);

Error:
    if (pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle (pipe);
    }

    if (FAILED (hr) && outProcess != nullptr)
    {
        CloseHandle (outProcess);
        outProcess = nullptr;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::TryRunFromCommandLine
//
//  The helper's command line is the switch, the pipe name and the drive
//  number. Both are checked before anything is opened.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RawDeviceHelper::TryRunFromCommandLine (int & outExitCode)
{
    HRESULT            hr         = S_OK;
    int                argc       = 0;
    LPWSTR           * argv       = CommandLineToArgvW (GetCommandLineW(), &argc);
    bool               helper     = argv != nullptr && argc == 4 && wcscmp (argv[1], kSwitch) == 0;
    bool               validName  = false;
    bool               validDrive = false;
    int                drive      = -1;
    HANDLE             pipe       = INVALID_HANDLE_VALUE;
    std::wstring       path;
    Win32PipeStream    stream;
    Win32RawDevice     device;



    BAIL_OUT_IF (!helper, S_OK);

    validName  = IsValidPipeName (argv[2]);
    validDrive = TryParseDrive (argv[3], drive);
    CBREx (validName && validDrive, E_INVALIDARG);

    path = std::format (L"\\\\.\\pipe\\{}", argv[2]);

    //  Anonymous, so the pipe's server cannot act as this elevated process.
    pipe = CreateFileW (path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
    CWR (pipe != INVALID_HANDLE_VALUE);

    hr = CheckServer (pipe);
    CHR (hr);

    //  The stream closes the pipe from here on.
    hr   = stream.Attach (pipe);
    pipe = INVALID_HANDLE_VALUE;
    CHR (hr);

    {
        RawDeviceServer  server (device, stream);

        hr = server.Run (drive);
        CHR (hr);
    }

Error:
    if (pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle (pipe);
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
//  Win32RawDeviceHelper::IsValidPipeName
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RawDeviceHelper::IsValidPipeName (const std::wstring & name)
{
    size_t  prefix = wcslen (kPipePrefix);
    bool    valid  = name.size() > prefix && name.size() <= kMaxNameChars && name.compare (0, prefix, kPipePrefix) == 0;



    for (size_t at = prefix; valid && at < name.size(); at++)
    {
        wchar_t  ch = name[at];

        valid = (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F');
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::TryParseDrive
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RawDeviceHelper::TryParseDrive (const std::wstring & text, int & outDrive)
{
    bool  valid = !text.empty() && text.size() <= 4;
    int   value = 0;



    for (size_t at = 0; valid && at < text.size(); at++)
    {
        valid = text[at] >= L'0' && text[at] <= L'9';
        value = value * 10 + (text[at] - L'0');
    }

    outDrive = valid ? value : -1;

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::MakePipeName
//
//  The prefix and a new GUID's digits, so no two launches share a pipe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::MakePipeName (std::wstring & outName)
{
    HRESULT  hr   = S_OK;
    GUID     guid = {};



    outName.clear();

    hr = CoCreateGuid (&guid);
    CHR (hr);

    outName = std::format (L"{}{:08X}{:04X}{:04X}", kPipePrefix, guid.Data1, guid.Data2, guid.Data3);

    for (Byte part : guid.Data4)
    {
        outName += std::format (L"{:02X}", part);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::GetUserSid
//
//  The SID of the user this process runs as, in its text form.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::GetUserSid (std::wstring & outSid)
{
    HRESULT            hr       = S_OK;
    HANDLE             token    = nullptr;
    DWORD              length   = 0;
    LPWSTR             text     = nullptr;
    BOOL               ok       = FALSE;
    std::vector<Byte>  buffer;



    outSid.clear();

    ok = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token);
    CWR (ok);

    GetTokenInformation (token, TokenUser, nullptr, 0, &length);
    CBREx (length > 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    buffer.resize (length);

    ok = GetTokenInformation (token, TokenUser, buffer.data(), length, &length);
    CWR (ok);

    ok = ConvertSidToStringSidW (((TOKEN_USER *) buffer.data())->User.Sid, &text);
    CWR (ok);

    outSid = text;

Error:
    if (text != nullptr)
    {
        LocalFree (text);
    }

    if (token != nullptr)
    {
        CloseHandle (token);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::CreatePipe
//
//  FILE_FLAG_FIRST_PIPE_INSTANCE fails the call when a pipe of this name
//  exists, so the pipe is never one some other process created first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::CreatePipe (const std::wstring & name, HANDLE & outPipe)
{
    HRESULT              hr         = S_OK;
    SECURITY_ATTRIBUTES  attributes = {};
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    BOOL                 ok         = FALSE;
    std::wstring         sid;
    std::wstring         sddl;
    std::wstring         path       = std::format (L"\\\\.\\pipe\\{}", name);



    outPipe = INVALID_HANDLE_VALUE;

    hr = GetUserSid (sid);
    CHR (hr);

    sddl = std::format (L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;{})", sid);

    ok = ConvertStringSecurityDescriptorToSecurityDescriptorW (sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr);
    CWR (ok);

    attributes.nLength              = sizeof (attributes);
    attributes.lpSecurityDescriptor = descriptor;
    attributes.bInheritHandle       = FALSE;

    outPipe = CreateNamedPipeW (path.c_str(),
                                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                1,
                                0x10000,
                                0x10000,
                                0,
                                &attributes);
    CWR (outPipe != INVALID_HANDLE_VALUE);

Error:
    if (descriptor != nullptr)
    {
        LocalFree (descriptor);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::WaitForClient
//
//  Ends when the helper connects, when it exits, or at the timeout. A helper
//  that exits first failed, and its exit code is the reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::WaitForClient (HANDLE pipe, HANDLE process)
{
    HRESULT     hr         = S_OK;
    OVERLAPPED  overlapped = {};
    HANDLE      waits[2]   = {};
    DWORD       error      = ERROR_SUCCESS;
    DWORD       waited     = WAIT_OBJECT_0;
    DWORD       exitCode   = 0;
    DWORD       ignored    = 0;
    BOOL        ok         = FALSE;



    overlapped.hEvent = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    CWR (overlapped.hEvent != nullptr);

    ok    = ConnectNamedPipe (pipe, &overlapped);
    error = ok ? ERROR_SUCCESS : GetLastError();

    BAIL_OUT_IF (ok || error == ERROR_PIPE_CONNECTED, S_OK);
    CBREx (error == ERROR_IO_PENDING, HRESULT_FROM_WIN32 (error));

    waits[0] = overlapped.hEvent;
    waits[1] = process;

    waited = WaitForMultipleObjects (process != nullptr ? 2 : 1, waits, FALSE, kConnectTimeoutMs);

    if (waited != WAIT_OBJECT_0)
    {
        //  The connect is over, one way or the other, before the event goes.
        CancelIoEx (pipe, &overlapped);
        GetOverlappedResult (pipe, &overlapped, &ignored, TRUE);
    }

    if (waited == WAIT_OBJECT_0 + 1)
    {
        ok = GetExitCodeProcess (process, &exitCode);
        hr = ok && FAILED ((HRESULT) exitCode) ? (HRESULT) exitCode : E_FAIL;
    }
    else if (waited != WAIT_OBJECT_0)
    {
        hr = HRESULT_FROM_WIN32 (ERROR_TIMEOUT);
    }

    CHR (hr);

Error:
    if (overlapped.hEvent != nullptr)
    {
        CloseHandle (overlapped.hEvent);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RawDeviceHelper::CheckServer
//
//  The process at the pipe's other end must be running this program's file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RawDeviceHelper::CheckServer (HANDLE pipe)
{
    HRESULT   hr               = S_OK;
    wchar_t   self[MAX_PATH]   = {};
    wchar_t   server[MAX_PATH] = {};
    DWORD     selfLength       = 0;
    DWORD     serverLength     = ARRAYSIZE (server);
    ULONG     processId        = 0;
    HANDLE    process          = nullptr;
    BOOL      ok               = FALSE;
    bool      fits             = false;
    bool      same             = false;



    selfLength = GetModuleFileNameW (nullptr, self, ARRAYSIZE (self));
    fits       = selfLength > 0 && selfLength < ARRAYSIZE (self);
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    ok = GetNamedPipeServerProcessId (pipe, &processId);
    CWR (ok);

    process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    CWR (process != nullptr);

    ok = QueryFullProcessImageNameW (process, 0, server, &serverLength);
    CWR (ok);

    same = _wcsicmp (self, server) == 0;
    CBREx (same, E_ACCESSDENIED);

Error:
    if (process != nullptr)
    {
        CloseHandle (process);
    }

    return hr;
}
