#include "Pch.h"

#include "Seams/ShellVerbs/Win32FileContents.h"

extern "C" IMAGE_DOS_HEADER  __ImageBase;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::~Win32FileContents
//
////////////////////////////////////////////////////////////////////////////////

Win32FileContents::~Win32FileContents()
{
    EndHost();

    if (m_readDone != nullptr)
    {
        CloseHandle (m_readDone);
        m_readDone = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetReader
//
//  The reader is selected by the persistent handler registered for the
//  extension and the IFilter registered for that handler. A .zip file has no
//  reader here, as the Compressed files option reads inside it.
//
////////////////////////////////////////////////////////////////////////////////

Win32FileContents::Reader Win32FileContents::GetReader (const std::wstring & path)
{
    HRESULT            hr        = S_OK;
    Reader             reader    = Reader::None;
    std::wstring       extension = std::filesystem::path (path).extension().wstring();
    Handler            handler   = Handler::Absent;
    std::vector<Byte>  head;
    std::wstring       filter;
    bool               isZip     = false;
    bool               isText    = false;



    MakeLowerCase (extension);

    isZip = extension == kZipExtension;
    BAIL_OUT_IF (isZip, S_OK);

    handler = GetHandler (extension, filter);

    switch (handler)
    {
        case Handler::Null:
            break;

        case Handler::PlainText:
            reader = Reader::Direct;
            break;

        case Handler::Other:
            reader = Reader::Filter;
            break;

        case Handler::Absent:
            hr = ReadFileBytes (path, head, kSniffBytes);
            CHR (hr);

            isText = LooksLikeText (head.data(), head.size());

            if (isText)
            {
                reader = Reader::Direct;
            }

            break;
    }

Error:
    return reader;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetHandler
//
//  The persistent handler is looked up under the extension's key first, then
//  under its file type's key. The IFilter registered for the handler
//  distinguishes the null filter and the plain-text filter from the rest. A
//  handler with no IFilter registered is no filter at all.
//
////////////////////////////////////////////////////////////////////////////////

Win32FileContents::Handler Win32FileContents::GetHandler (const std::wstring & extension, std::wstring & outFilter)
{
    HRESULT       hr           = S_OK;
    Handler       result       = Handler::Absent;
    std::wstring  handler;
    std::wstring  progId;
    bool          hasExtension = extension.size() > 1;
    bool          found        = false;



    outFilter.clear();

    BAIL_OUT_IF (!hasExtension, S_OK);

    found = TryReadRegistry (extension + L"\\PersistentHandler", handler);

    if (!found)
    {
        found = TryReadRegistry (extension, progId);

        if (found)
        {
            found = TryReadRegistry (progId + L"\\PersistentHandler", handler);
        }
    }

    BAIL_OUT_IF (!found, S_OK);

    if (_wcsicmp (handler.c_str(), kNullPersistentHandler) == 0)
    {
        result = Handler::Null;
    }
    else if (_wcsicmp (handler.c_str(), kPlainTextPersistentHandler) == 0)
    {
        result = Handler::PlainText;
    }
    else
    {
        found = TryReadRegistry (std::wstring (L"CLSID\\") + handler + L"\\PersistentAddinsRegistered\\" + kFilterInterfaceId, outFilter);

        if (found && _wcsicmp (outFilter.c_str(), kNullFilter) == 0)
        {
            result = Handler::Null;
        }
        else if (found && _wcsicmp (outFilter.c_str(), kPlainTextFilter) == 0)
        {
            result = Handler::PlainText;
        }
        else if (found)
        {
            result = Handler::Other;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::TryGetFilterClass
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::TryGetFilterClass (const std::wstring & extension, std::wstring & outClass)
{
    std::wstring  lowered = extension;
    Handler       handler = Handler::Absent;



    MakeLowerCase (lowered);

    handler = GetHandler (lowered, outClass);

    return handler == Handler::Other;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::TryReadRegistry
//
//  The default string value of a key under HKEY_CLASSES_ROOT.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::TryReadRegistry (const std::wstring & subKey, std::wstring & outValue)
{
    constexpr DWORD  kValueChars = 260;



    wchar_t  value[kValueChars] = {};
    DWORD    size               = sizeof (value);
    LSTATUS  status             = RegGetValueW (HKEY_CLASSES_ROOT, subKey.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, value, &size);
    bool     found              = status == ERROR_SUCCESS && value[0] != L'\0';



    outValue.clear();

    if (found)
    {
        outValue = value;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::ReadFileBytes
//
//  The file's first bytes, at most `limit`. The file is opened to allow
//  other readers and writers, as a search runs beside whatever is using it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::ReadFileBytes (const std::wstring & path, std::vector<Byte> & outBytes, size_t limit)
{
    HRESULT        hr      = S_OK;
    HANDLE         file    = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    LARGE_INTEGER  size      = {};
    BOOL           ok        = FALSE;
    DWORD          got       = 0;
    size_t         total     = 0;
    size_t         byteLimit = 0;
    bool           hasMore   = true;



    outBytes.clear();

    CWR (file != INVALID_HANDLE_VALUE);

    ok = GetFileSizeEx (file, &size);
    CWR (ok);

    byteLimit = (size_t) std::min<uint64_t> ((uint64_t) size.QuadPart, limit);
    outBytes.resize (byteLimit);

    while (hasMore && total < byteLimit)
    {
        ok = ReadFile (file, outBytes.data() + total, (DWORD) (byteLimit - total), &got, nullptr);
        CWR (ok);

        total  += got;
        hasMore = got != 0;
    }

    outBytes.resize (total);

Error:
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (file);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::DetectEncoding
//
//  A byte order mark selects the encoding. Without one, UTF-16 is recognized from the zero
//  high bytes of ASCII and Latin-1 text; any other bytes are UTF-8 or the
//  ANSI code page.
//
////////////////////////////////////////////////////////////////////////////////

Win32FileContents::Encoding Win32FileContents::DetectEncoding (const Byte * data, size_t count)
{
    constexpr Byte  kUtf8Bom[]    = { 0xEF, 0xBB, 0xBF };
    constexpr Byte  kUtf16LeBom[] = { 0xFF, 0xFE };
    constexpr Byte  kUtf16BeBom[] = { 0xFE, 0xFF };



    Encoding  encoding = Encoding::Other;



    if (count >= sizeof (kUtf8Bom) && memcmp (data, kUtf8Bom, sizeof (kUtf8Bom)) == 0)
    {
        encoding = Encoding::Utf8Bom;
    }
    else if (count >= sizeof (kUtf16LeBom) && memcmp (data, kUtf16LeBom, sizeof (kUtf16LeBom)) == 0)
    {
        encoding = Encoding::Utf16LeBom;
    }
    else if (count >= sizeof (kUtf16BeBom) && memcmp (data, kUtf16BeBom, sizeof (kUtf16BeBom)) == 0)
    {
        encoding = Encoding::Utf16BeBom;
    }
    else if (IsUtf16Text (data, count, false))
    {
        encoding = Encoding::Utf16Le;
    }
    else if (IsUtf16Text (data, count, true))
    {
        encoding = Encoding::Utf16Be;
    }

    return encoding;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::IsUtf16Text
//
//  Whether nearly every 16-bit unit is a character below U+0100 that is not a
//  control character. Binary data with runs of zeros fails this, as a zero
//  unit is a control character.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::IsUtf16Text (const Byte * data, size_t count, bool isBigEndian)
{
    constexpr size_t  kMinPercent = 90;
    constexpr size_t  kPercent    = 100;



    size_t  units    = count / sizeof (uint16_t);
    size_t  matching = 0;
    size_t  index    = 0;
    Byte    high     = 0;
    Byte    low      = 0;



    for (index = 0; index < units; index++)
    {
        high = data[index * sizeof (uint16_t) + (isBigEndian ? 0 : 1)];
        low  = data[index * sizeof (uint16_t) + (isBigEndian ? 1 : 0)];

        if (high == 0 && IsTextByte (low))
        {
            matching++;
        }
    }

    return units > 0 && matching * kPercent >= units * kMinPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::IsTextByte
//
//  Any byte but a control character other than the tab, line feed, form feed,
//  carriage return and Ctrl+Z.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::IsTextByte (Byte byte)
{
    constexpr Byte  kFirstPrintable = 0x20;
    constexpr Byte  kCtrlZ          = 0x1A;



    return byte >= kFirstPrintable || byte == '\t' || byte == '\n' || byte == '\f' || byte == '\r' || byte == kCtrlZ;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetBomLength
//
////////////////////////////////////////////////////////////////////////////////

size_t Win32FileContents::GetBomLength (Encoding encoding)
{
    constexpr size_t  kUtf8BomBytes  = 3;
    constexpr size_t  kUtf16BomBytes = 2;



    size_t  length = 0;



    if (encoding == Encoding::Utf8Bom)
    {
        length = kUtf8BomBytes;
    }
    else if (encoding == Encoding::Utf16LeBom || encoding == Encoding::Utf16BeBom)
    {
        length = kUtf16BomBytes;
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::LooksLikeText
//
//  Text with a byte order mark, UTF-16 recognized without one, or bytes that
//  contain no zero and no control character other than the whitespace ones,
//  which UTF-8 and the ANSI code pages both satisfy. No bytes are not text.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::LooksLikeText (const Byte * data, size_t count)
{
    Encoding  encoding   = DetectEncoding (data, count);
    bool      isMarked   = encoding != Encoding::Other;
    bool      isOrdinary = count > 0;
    size_t    index      = 0;



    for (index = 0; !isMarked && isOrdinary && index < count; index++)
    {
        isOrdinary = IsTextByte (data[index]);
    }

    return isMarked || isOrdinary;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::DecodeText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32FileContents::DecodeText (const Byte * data, size_t count)
{
    Encoding      encoding = DetectEncoding (data, count);
    size_t        skip     = GetBomLength (encoding);
    const Byte *  payload  = data + skip;
    size_t        size     = count - skip;
    std::wstring  text;
    bool          isUtf8   = false;



    if (encoding == Encoding::Utf16Le || encoding == Encoding::Utf16LeBom)
    {
        text = DecodeUtf16 (payload, size, false);
    }
    else if (encoding == Encoding::Utf16Be || encoding == Encoding::Utf16BeBom)
    {
        text = DecodeUtf16 (payload, size, true);
    }
    else
    {
        isUtf8 = encoding == Encoding::Utf8Bom || IsValidUtf8 (payload, size);
        text   = ConvertCodePage (payload, size, isUtf8 ? CP_UTF8 : CP_ACP);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::DecodeUtf16
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32FileContents::DecodeUtf16 (const Byte * data, size_t count, bool isBigEndian)
{
    constexpr unsigned  kByteBits = 8;



    size_t        units = count / sizeof (uint16_t);
    std::wstring  text  (units, L'\0');
    size_t        index = 0;
    unsigned      first = 0;
    unsigned      next  = 0;



    for (index = 0; index < units; index++)
    {
        first = data[index * sizeof (uint16_t)];
        next  = data[index * sizeof (uint16_t) + 1];

        text[index] = (wchar_t) (isBigEndian ? (first << kByteBits) | next : (next << kByteBits) | first);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::ConvertCodePage
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32FileContents::ConvertCodePage (const Byte * data, size_t count, UINT codePage)
{
    HRESULT       hr        = S_OK;
    std::wstring  text;
    int           charCount = 0;
    int           bytes     = (int) std::min<size_t> (count, INT_MAX);



    BAIL_OUT_IF (bytes == 0, S_OK);

    charCount = MultiByteToWideChar (codePage, 0, (const char *) data, bytes, nullptr, 0);
    BAIL_OUT_IF (charCount <= 0, S_OK);

    text.resize ((size_t) charCount);
    MultiByteToWideChar (codePage, 0, (const char *) data, bytes, text.data(), charCount);

Error:
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::IsValidUtf8
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FileContents::IsValidUtf8 (const Byte * data, size_t count)
{
    int   bytes = (int) std::min<size_t> (count, INT_MAX);
    int   chars = 0;



    chars = MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, (const char *) data, bytes, nullptr, 0);

    return bytes == 0 || chars > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::MakeLowerCase
//
////////////////////////////////////////////////////////////////////////////////

void Win32FileContents::MakeLowerCase (std::wstring & text)
{
    for (wchar_t & ch : text)
    {
        ch = (wchar_t) towlower (ch);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::Load
//
//  S_FALSE for a file with no reader and for one whose reader found no text,
//  which a search skips; a failure for a file that cannot be opened or a
//  filter process that cannot start.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::Load (const std::wstring & path, std::wstring & outText)
{
    HRESULT  hr      = S_OK;
    Reader   reader  = GetReader (path);
    bool     hasText = false;



    outText.clear();

    BAIL_OUT_IF (reader == Reader::None, S_FALSE);   // EHM-ALLOW-SFALSE: a file with no reader has no text

    if (reader == Reader::Direct)
    {
        hr = LoadDirect (path, outText);
    }
    else
    {
        hr = LoadFiltered (path, outText);
    }

    CHR (hr);

    MakeLowerCase (outText);

    hasText = !outText.empty();

    if (!hasText)
    {
        hr = S_FALSE;   // EHM-ALLOW-SFALSE: the reader found no text
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::LoadDirect
//
//  A file is read as far as kMaxChars characters can come from, which is two
//  bytes each in UTF-16.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::LoadDirect (const std::wstring & path, std::wstring & outText)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  bytes;



    hr = ReadFileBytes (path, bytes, kMaxChars * sizeof (wchar_t));
    CHR (hr);

    outText = DecodeText (bytes.data(), bytes.size());

    if (outText.size() > kMaxChars)
    {
        outText.resize (kMaxChars);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::LoadFiltered
//
//  The path goes to the filter process, which is started if it isn't
//  running. A process that ended since the last file is replaced and the
//  request sent again, as the file did not end it. A process that fails to
//  answer within the limit, or ends while answering, is ended and the file
//  has no text; the next filtered file starts another.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::LoadFiltered (const std::wstring & path, std::wstring & outText)
{
    HRESULT  hr       = S_OK;
    HRESULT  hrWrite  = S_OK;
    HRESULT  hrRead   = S_OK;
    HRESULT  hrFilter = S_OK;



    hr = StartHost();
    CHR (hr);

    hrWrite = WriteRequest (path);

    if (FAILED (hrWrite))
    {
        EndHost();

        hr = StartHost();
        CHR (hr);

        hrWrite = WriteRequest (path);
    }

    if (SUCCEEDED (hrWrite))
    {
        hrRead = ReadReply (GetTickCount64() + m_timeoutMs, outText, hrFilter);
    }

    if (FAILED (hrWrite) || FAILED (hrRead))
    {
        EndHost();
        outText.clear();

        hr = S_FALSE;   // EHM-ALLOW-SFALSE: a filter process that failed or timed out costs this file only
    }
    else if (FAILED (hrFilter))
    {
        outText.clear();

        hr = S_FALSE;   // EHM-ALLOW-SFALSE: the filter could not read this file
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::StartHost
//
//  Does nothing when the process is running.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::StartHost()
{
    HRESULT       hr         = S_OK;
    std::wstring  exe;
    HANDLE        hostInput  = nullptr;
    HANDLE        hostOutput = nullptr;
    bool          isRunning  = m_process != nullptr;



    BAIL_OUT_IF (isRunning, S_OK);

    if (m_readDone == nullptr)
    {
        m_readDone = CreateEventW (nullptr, TRUE, FALSE, nullptr);
        CWR (m_readDone != nullptr);
    }

    hr = GetHostPath (exe);
    CHR (hr);

    hr = CreatePipes (hostInput, hostOutput);
    CHR (hr);

    hr = LaunchHost (exe, hostInput, hostOutput);
    CHR (hr);

Error:
    if (hostInput != nullptr)
    {
        CloseHandle (hostInput);
    }

    if (hostOutput != nullptr)
    {
        CloseHandle (hostOutput);
    }

    if (FAILED (hr))
    {
        EndHost();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetHostPath
//
//  CassoExplorer.exe in the folder of the module this code is in, which is the
//  program itself or a test assembly built beside it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::GetHostPath (std::wstring & outPath)
{
    HRESULT                hr               = S_OK;
    wchar_t                module[MAX_PATH] = {};
    DWORD                  length           = GetModuleFileNameW ((HMODULE) &__ImageBase, module, (DWORD) std::size (module));
    bool                   fits             = length != 0 && length < (DWORD) std::size (module);
    std::filesystem::path  exe;
    DWORD                  attributes       = INVALID_FILE_ATTRIBUTES;
    bool                   exists           = false;



    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

    exe        = std::filesystem::path (module).replace_filename (kHostExeName);
    attributes = GetFileAttributesW (exe.c_str());
    exists     = attributes != INVALID_FILE_ATTRIBUTES;
    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    outPath = exe.wstring();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::CreatePipes
//
//  The host's standard input and output, as inheritable handles for it to be
//  started with, and this side's ends. The pipe the replies arrive on has a
//  generated name, as an anonymous pipe cannot be read with a time limit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::CreatePipes (HANDLE & outHostInput, HANDLE & outHostOutput)
{
    HRESULT              hr          = S_OK;
    SECURITY_ATTRIBUTES  inheritable = { sizeof (inheritable), nullptr, TRUE };
    std::wstring         name        = std::format (L"\\\\.\\pipe\\CassoFilterHost-{}-{}-{}", GetCurrentProcessId(), (uintptr_t) this, m_pipeSerial++);
    HANDLE               pipe        = INVALID_HANDLE_VALUE;
    BOOL                 ok          = FALSE;



    outHostInput  = nullptr;
    outHostOutput = nullptr;

    pipe = CreateNamedPipeW (name.c_str(),
                             PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                             1,
                             0,
                             kPipeBytes,
                             0,
                             nullptr);
    CWR (pipe != INVALID_HANDLE_VALUE);

    m_fromHost = pipe;

    pipe = CreateFileW (name.c_str(), GENERIC_WRITE, 0, &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CWR (pipe != INVALID_HANDLE_VALUE);

    outHostOutput = pipe;

    ok = CreatePipe (&outHostInput, &m_toHost, &inheritable, kPipeBytes);
    CWR (ok);

    ok = SetHandleInformation (m_toHost, HANDLE_FLAG_INHERIT, 0);
    CWR (ok);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::LaunchHost
//
//  The program with the host switch, without a window, and inheriting only
//  the two pipe handles. It starts suspended so it is in the job before it runs;
//  closing the job ends it, whatever happens to this process.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::LaunchHost (const std::wstring & exe, HANDLE hostInput, HANDLE hostOutput)
{
    HRESULT                               hr          = S_OK;
    HANDLE                                inherited[] = { hostInput, hostOutput };
    STARTUPINFOEXW                        startup     = {};
    SIZE_T                                listSize    = 0;
    std::vector<Byte>                     listBuffer;
    LPPROC_THREAD_ATTRIBUTE_LIST          list        = nullptr;
    bool                                  hasList     = false;
    PROCESS_INFORMATION                   process     = {};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION  limits      = {};
    std::wstring                          command    = L"\"" + exe + L"\" " + Win32FilterHost::kSwitch;
    BOOL                                  ok         = FALSE;
    DWORD                                 resumed    = 0;



    startup.StartupInfo.cb = sizeof (startup);

    //  The first call fails and returns the size of the list.
    ok = InitializeProcThreadAttributeList (nullptr, 1, 0, &listSize);
    IGNORE_RETURN_VALUE (ok, FALSE);

    listBuffer.resize (listSize);
    list = (LPPROC_THREAD_ATTRIBUTE_LIST) listBuffer.data();
    CBR (list != nullptr);

    ok = InitializeProcThreadAttributeList (list, 1, 0, &listSize);
    CWR (ok);

    hasList = true;

    ok = UpdateProcThreadAttribute (list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof (inherited), nullptr, nullptr);
    CWR (ok);

    startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput  = hostInput;
    startup.StartupInfo.hStdOutput = hostOutput;
    startup.StartupInfo.hStdError  = nullptr;
    startup.lpAttributeList        = list;

    ok = CreateProcessW (exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                         EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW,
                         nullptr, nullptr, &startup.StartupInfo, &process);
    CWR (ok);

    m_process   = process.hProcess;
    m_processId = process.dwProcessId;

    m_job = CreateJobObjectW (nullptr, nullptr);
    CWR (m_job != nullptr);

    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    ok = SetInformationJobObject (m_job, JobObjectExtendedLimitInformation, &limits, sizeof (limits));
    CWR (ok);

    ok = AssignProcessToJobObject (m_job, m_process);
    CWR (ok);

    resumed = ResumeThread (process.hThread);
    CWR (resumed != (DWORD) -1);

Error:
    if (process.hThread != nullptr)
    {
        CloseHandle (process.hThread);
    }

    if (hasList)
    {
        DeleteProcThreadAttributeList (list);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::EndHost
//
//  Closing the job ends the process and anything it started.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FileContents::EndHost()
{
    BOOL  terminated = FALSE;



    if (m_job != nullptr)
    {
        CloseHandle (m_job);
        m_job = nullptr;
    }

    if (m_process != nullptr)
    {
        terminated = TerminateProcess (m_process, 1);
        IGNORE_RETURN_VALUE (terminated, FALSE);

        CloseHandle (m_process);
        m_process = nullptr;
    }

    if (m_toHost != nullptr)
    {
        CloseHandle (m_toHost);
        m_toHost = nullptr;
    }

    if (m_fromHost != nullptr)
    {
        CloseHandle (m_fromHost);
        m_fromHost = nullptr;
    }

    m_processId = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetHostProcessId
//
////////////////////////////////////////////////////////////////////////////////

DWORD Win32FileContents::GetHostProcessId() const
{
    return m_processId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::SetFilterTimeout
//
////////////////////////////////////////////////////////////////////////////////

void Win32FileContents::SetFilterTimeout (DWORD milliseconds)
{
    m_timeoutMs = milliseconds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::GetFilterTimeout
//
////////////////////////////////////////////////////////////////////////////////

DWORD Win32FileContents::GetFilterTimeout() const
{
    return m_timeoutMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::WriteRequest
//
//  The request is written in one call, so the length and the path arrive
//  together.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::WriteRequest (const std::wstring & path)
{
    HRESULT            hr       = S_OK;
    uint32_t           length   = (uint32_t) path.size();
    size_t             pathSize = path.size() * sizeof (wchar_t);
    std::vector<Byte>  message  (sizeof (length) + pathSize);
    DWORD              written  = 0;
    BOOL               ok       = FALSE;
    bool               fits     = path.size() > 0 && length <= Win32FilterHost::kMaxRequestPath;



    CBREx (fits, E_INVALIDARG);

    memcpy (message.data(), &length, sizeof (length));
    memcpy (message.data() + sizeof (length), path.data(), pathSize);

    ok = WriteFile (m_toHost, message.data(), (DWORD) message.size(), &written, nullptr);
    CWR (ok);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::ReadReply
//
//  The reply's header and text, all by the deadline. `outStatus` is what the
//  filter returned for the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::ReadReply (ULONGLONG deadline, std::wstring & outText, HRESULT & outStatus)
{
    HRESULT                       hr     = S_OK;
    Win32FilterHost::ReplyHeader  header;



    outText.clear();
    outStatus = S_OK;

    hr = ReadWithDeadline (&header, sizeof (header), deadline);
    CHR (hr);

    CBREx (header.length <= Win32FilterHost::kMaxChars, E_UNEXPECTED);

    outText.resize (header.length);

    hr = ReadWithDeadline (outText.data(), header.length * sizeof (wchar_t), deadline);
    CHR (hr);

    outStatus = header.status;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents::ReadWithDeadline
//
//  Reads exactly `count` bytes from the host, or fails with ERROR_TIMEOUT
//  once the deadline (a GetTickCount64 value) passes with the read pending.
//  The pending read is canceled and awaited, so nothing refers to the buffer
//  afterward.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FileContents::ReadWithDeadline (void * buffer, DWORD count, ULONGLONG deadline)
{
    HRESULT       hr         = S_OK;
    Byte        * cursor     = (Byte *) buffer;
    OVERLAPPED    overlapped = {};
    DWORD         got        = 0;
    DWORD         waited     = WAIT_OBJECT_0;
    DWORD         error      = ERROR_SUCCESS;
    ULONGLONG     now        = 0;
    DWORD         remaining  = 0;
    BOOL          ok         = FALSE;
    bool          isPending  = false;
    bool          isDone     = false;



    overlapped.hEvent = m_readDone;

    while (count > 0)
    {
        ResetEvent (m_readDone);

        ok    = ReadFile (m_fromHost, cursor, count, nullptr, &overlapped);
        error = ok ? ERROR_SUCCESS : GetLastError();

        isPending = ok || error == ERROR_IO_PENDING;
        CBREx (isPending, HRESULT_FROM_WIN32 (error));

        now       = GetTickCount64();
        remaining = deadline > now ? (DWORD) std::min<ULONGLONG> (deadline - now, MAXDWORD - 1) : 0;
        waited    = WaitForSingleObject (m_readDone, remaining);

        if (waited != WAIT_OBJECT_0)
        {
            CancelIoEx (m_fromHost, &overlapped);
            GetOverlappedResult (m_fromHost, &overlapped, &got, TRUE);

            isDone = false;
            CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
        }

        ok = GetOverlappedResult (m_fromHost, &overlapped, &got, FALSE);
        CWR (ok);

        cursor += got;
        count  -= got;
    }

Error:
    return hr;
}
