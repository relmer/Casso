#include "Pch.h"

#include "Seams/RawDevice/Win32StorageCardProxy.h"
#include "Seams/RawDevice/RawDeviceServer.h"
#include "Seams/RawDevice/Win32RawDevice.h"
#include "Update/AuthenticodeVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardProxy::Launch
//
//  The pipe exists before the proxy starts, so the proxy's connection
//  cannot arrive first. ShellExecuteExW returns once the user has dealt
//  with the UAC prompt, so that time is not part of the wait.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::Launch (HWND owner, Win32PipeStream & stream, HANDLE & outProcess)
{
    HRESULT            hr      = S_OK;
    HANDLE             pipe    = INVALID_HANDLE_VALUE;
    SHELLEXECUTEINFOW  info    = {};
    BOOL               started = FALSE;
    std::wstring       program;
    std::wstring       name;



    outProcess = nullptr;

    hr = GetSiblingPath (kProgramFile, program);
    CHR (hr);

    hr = MakePipeName (name);
    CHR (hr);

    hr = CreatePipe (name, pipe);
    CHR (hr);

    info.cbSize       = sizeof (info);
    info.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
    info.hwnd         = owner;
    info.lpVerb       = L"runas";
    info.lpFile       = program.c_str();
    info.lpParameters = name.c_str();
    info.nShow        = SW_HIDE;

    started = ShellExecuteExW (&info);
    CWR (started);

    outProcess = info.hProcess;
    CBREx (outProcess != nullptr, E_UNEXPECTED);

    hr = WaitForClient (pipe, outProcess);
    CHR (hr);

    hr = CheckClient (pipe, outProcess);
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
//  Win32StorageCardProxy::Run
//
//  The command line is the program and the pipe's name, and the name is
//  checked before anything is opened.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::Run (const wchar_t * commandLine)
{
    HRESULT            hr        = S_OK;
    int                argc      = 0;
    LPWSTR           * argv      = CommandLineToArgvW (commandLine, &argc);
    bool               validName = argv != nullptr && argc == 2 && IsValidPipeName (argv[1]);
    HANDLE             pipe      = INVALID_HANDLE_VALUE;
    std::wstring       path;
    Win32PipeStream    stream;



    CBREx (validName, E_INVALIDARG);

    path = std::format (L"\\\\.\\pipe\\{}", argv[1]);

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
        RawDeviceServer  server ([]() { return std::make_unique<Win32RawDevice>(); }, stream);

        hr = server.Run();
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

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardProxy::IsValidPipeName
//
////////////////////////////////////////////////////////////////////////////////

bool Win32StorageCardProxy::IsValidPipeName (const std::wstring & name)
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
//  Win32StorageCardProxy::GetSiblingPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::GetSiblingPath (const wchar_t * file, std::wstring & outPath)
{
    HRESULT  hr             = S_OK;
    wchar_t  self[MAX_PATH] = {};
    DWORD    length         = GetModuleFileNameW (nullptr, self, ARRAYSIZE (self));
    bool     fits           = length > 0 && length < ARRAYSIZE (self);



    outPath.clear();

    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    outPath = (std::filesystem::path (self).parent_path() / file).wstring();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardProxy::MakePipeName
//
//  The prefix and a new GUID's digits, so no two launches share a pipe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::MakePipeName (std::wstring & outName)
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
//  Win32StorageCardProxy::CreatePipe
//
//  Only SYSTEM and elevated administrators may open the pipe: a process that
//  is not elevated has the Administrators group for deny only, so Windows
//  fails its open. FILE_FLAG_FIRST_PIPE_INSTANCE fails the call when a pipe
//  of this name exists, so the pipe is never one some other process created
//  first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::CreatePipe (const std::wstring & name, HANDLE & outPipe)
{
    HRESULT              hr         = S_OK;
    SECURITY_ATTRIBUTES  attributes = {};
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    BOOL                 ok         = FALSE;
    std::wstring         path       = std::format (L"\\\\.\\pipe\\{}", name);



    outPipe = INVALID_HANDLE_VALUE;

    ok = ConvertStringSecurityDescriptorToSecurityDescriptorW (L"D:P(A;;GA;;;SY)(A;;GA;;;BA)", SDDL_REVISION_1, &descriptor, nullptr);
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
//  Win32StorageCardProxy::WaitForClient
//
//  Ends when a client connects, when the proxy exits, or at the timeout. A
//  proxy that exits first failed, and its exit code is the reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::WaitForClient (HANDLE pipe, HANDLE process)
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

    waited = WaitForMultipleObjects (2, waits, FALSE, kConnectTimeoutMs);

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
//  Win32StorageCardProxy::CheckClient
//
//  The process at the pipe's other end must be the proxy just started.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::CheckClient (HANDLE pipe, HANDLE process)
{
    HRESULT  hr       = S_OK;
    ULONG    clientId = 0;
    BOOL     ok       = GetNamedPipeClientProcessId (pipe, &clientId);
    bool     same     = false;



    CWR (ok);

    same = clientId != 0 && clientId == GetProcessId (process);
    CBREx (same, E_ACCESSDENIED);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32StorageCardProxy::CheckServer
//
//  The process at the pipe's other end must be running CassoExplorer.exe
//  from this program's folder. A signed proxy also requires a Casso Explorer
//  signed by the same publisher, so a release proxy never serves an
//  unsigned build.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32StorageCardProxy::CheckServer (HANDLE pipe)
{
    HRESULT               hr               = S_OK;
    wchar_t               server[MAX_PATH] = {};
    DWORD                 serverLength     = ARRAYSIZE (server);
    ULONG                 processId        = 0;
    HANDLE                process          = nullptr;
    BOOL                  ok               = FALSE;
    bool                  same             = false;
    bool                  selfSigned       = false;
    bool                  serverSigned     = false;
    std::wstring          self;
    std::wstring          explorer;
    AuthenticodeVerifier  verifier;



    hr = GetSiblingPath (kProgramFile, self);
    CHR (hr);

    hr = GetSiblingPath (kExplorerFile, explorer);
    CHR (hr);

    ok = GetNamedPipeServerProcessId (pipe, &processId);
    CWR (ok);

    process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    CWR (process != nullptr);

    ok = QueryFullProcessImageNameW (process, 0, server, &serverLength);
    CWR (ok);

    same = _wcsicmp (explorer.c_str(), server) == 0;
    CBREx (same, E_ACCESSDENIED);

    hr = verifier.IsOfficialFile (self, selfSigned);
    CHR (hr);

    if (selfSigned)
    {
        hr = verifier.IsOfficialFile (server, serverSigned);
        CHR (hr);

        CBREx (serverSigned, E_ACCESSDENIED);
    }

Error:
    if (process != nullptr)
    {
        CloseHandle (process);
    }

    return hr;
}
