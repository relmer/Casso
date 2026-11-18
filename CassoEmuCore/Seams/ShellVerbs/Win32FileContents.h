#pragma once

#include "Pch.h"

#include "Seams/ShellVerbs/Win32FilterHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileContents
//
//  A file's text, for a search of file contents. The reader for a file
//  depends on the IFilter registered for its extension: the null filter
//  means the file is not opened, the plain-text filter means its bytes are
//  read directly, and any other filter extracts the text, including the
//  metadata text of an image, except for .zip, which the Compressed files
//  option covers. A file with no filter is read directly when its first 4 KB
//  are ANSI, UTF-8 or UTF-16 text.
//
//  Filters run in a separate process (Win32FilterHost), so one that crashes
//  or hangs costs that file and not the program. The process starts on the
//  first filter read, serves every later one, and is replaced after it
//  crashes or exceeds the time limit.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FileContents
{
public:
    enum class Reader
    {
        None,
        Direct,
        Filter
    };

    static constexpr DWORD   kFilterTimeoutMs = 10'000;
    static constexpr size_t  kMaxChars        = Win32FilterHost::kMaxChars;
    static constexpr size_t  kSniffBytes      = 4096;

    Win32FileContents() = default;
    ~Win32FileContents();

    Win32FileContents (const Win32FileContents &)             = delete;
    Win32FileContents & operator= (const Win32FileContents &) = delete;

    //  The reader for a path; Direct for a file with no filter only when its
    //  first 4 KB are text.
    static Reader  GetReader (const std::wstring & path);

    //  The class id, as a string, of the IFilter registered for an extension
    //  such as ".pdf"; false when none is.
    static bool  TryGetFilterClass (const std::wstring & extension, std::wstring & outClass);

    //  The file's text, lowercased, at most kMaxChars. S_OK with text; S_FALSE
    //  when the file has no readable text, as for Reader::None or a filter
    //  that failed, crashed or timed out; a failure for a file that cannot be
    //  opened or a filter process that cannot start.
    HRESULT  Load (const std::wstring & path, std::wstring & outText);

    //  The limit for one file's filter, kFilterTimeoutMs unless set.
    void   SetFilterTimeout (DWORD milliseconds);
    DWORD  GetFilterTimeout () const;

    //  The filter process's id, or zero when none is running; and ending it,
    //  after which the next filter read starts another.
    DWORD  GetHostProcessId () const;
    void   EndHost          ();

    //  Whether the bytes are ANSI, UTF-8 or UTF-16 text, and their text. A
    //  byte order mark selects UTF-8, UTF-16LE or UTF-16BE; UTF-16 without one
    //  is detected from the zero bytes of ASCII text; otherwise the bytes are
    //  UTF-8 when valid and the ANSI code page when not.
    static bool          LooksLikeText (const Byte * data, size_t count);
    static std::wstring  DecodeText    (const Byte * data, size_t count);

private:
    enum class Handler
    {
        Absent,
        Null,
        PlainText,
        Other
    };

    enum class Encoding
    {
        Utf8Bom,
        Utf16LeBom,
        Utf16BeBom,
        Utf16Le,
        Utf16Be,
        Other
    };

    static constexpr DWORD  kPipeBytes = 64 * 1024;

    static constexpr const wchar_t *  kZipExtension              = L".zip";
    static constexpr const wchar_t *  kHostExeName               = L"CassoExplorer.exe";
    static constexpr const wchar_t *  kNullPersistentHandler     = L"{098f2470-bae0-11cd-b579-08002b30bfeb}";
    static constexpr const wchar_t *  kPlainTextPersistentHandler = L"{5e941d80-bf96-11cd-b579-08002b30bfeb}";
    static constexpr const wchar_t *  kNullFilter                = L"{c3278e90-bea7-11cd-b579-08002b30bfeb}";
    static constexpr const wchar_t *  kPlainTextFilter           = L"{c1243ca0-bf96-11cd-b579-08002b30bfeb}";
    static constexpr const wchar_t *  kFilterInterfaceId         = L"{89BCB740-6119-101A-BCB7-00DD010655AF}";

    static Handler       GetHandler        (const std::wstring & extension, std::wstring & outFilter);
    static bool          TryReadRegistry   (const std::wstring & subKey, std::wstring & outValue);
    static HRESULT       ReadFileBytes     (const std::wstring & path, std::vector<Byte> & outBytes, size_t limit);
    static Encoding      DetectEncoding    (const Byte * data, size_t count);
    static bool          IsUtf16Text       (const Byte * data, size_t count, bool isBigEndian);
    static bool          IsTextByte        (Byte byte);
    static size_t        GetBomLength      (Encoding encoding);
    static bool          IsValidUtf8       (const Byte * data, size_t count);
    static std::wstring  DecodeUtf16       (const Byte * data, size_t count, bool isBigEndian);
    static std::wstring  ConvertCodePage   (const Byte * data, size_t count, UINT codePage);
    static void          MakeLowerCase     (std::wstring & text);
    static HRESULT       GetHostPath       (std::wstring & outPath);

    HRESULT  LoadDirect        (const std::wstring & path, std::wstring & outText);
    HRESULT  LoadFiltered      (const std::wstring & path, std::wstring & outText);
    HRESULT  StartHost         ();
    HRESULT  CreatePipes       (HANDLE & outHostInput, HANDLE & outHostOutput);
    HRESULT  LaunchHost        (const std::wstring & exe, HANDLE hostInput, HANDLE hostOutput);
    HRESULT  WriteRequest      (const std::wstring & path);
    HRESULT  ReadReply         (ULONGLONG deadline, std::wstring & outText, HRESULT & outStatus);
    HRESULT  ReadWithDeadline  (void * buffer, DWORD count, ULONGLONG deadline);

    HANDLE    m_process    = nullptr;
    HANDLE    m_job        = nullptr;
    HANDLE    m_toHost     = nullptr;
    HANDLE    m_fromHost   = nullptr;
    HANDLE    m_readDone   = nullptr;
    DWORD     m_processId  = 0;
    DWORD     m_timeoutMs  = kFilterTimeoutMs;
    uint32_t  m_pipeSerial = 0;
};
