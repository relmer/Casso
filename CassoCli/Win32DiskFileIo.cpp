#include "Pch.h"

#include "Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::ReadAllBytes (const std::string & path, std::vector<Byte> & outBytes)
{
    HRESULT        hr     = S_OK;
    std::ifstream  file (path, std::ios::binary | std::ios::ate);
    bool           isOpen = false;
    std::streamoff size   = 0;



    isOpen = file.is_open();
    CBR (isOpen);

    size = file.tellg();
    CBR (size >= 0);

    outBytes.resize ((size_t) size);
    file.seekg (0, std::ios::beg);
    file.read (reinterpret_cast<char *> (outBytes.data()), size);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::WriteAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes)
{
    HRESULT        hr     = S_OK;
    std::ofstream  file (path, std::ios::binary | std::ios::trunc);
    bool           isOpen = false;
    bool           wrote  = false;



    isOpen = file.is_open();
    CBR (isOpen);

    file.write (reinterpret_cast<const char *> (bytes.data()), (std::streamsize) bytes.size());

    wrote = file.good();
    CBR (wrote);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::Stat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::Stat (const std::string & path, FileStamp & outStamp)
{
    HRESULT                     hr       = S_OK;
    WIN32_FILE_ATTRIBUTE_DATA   data     = {};
    std::wstring                widePath = std::filesystem::path (path).wstring();
    BOOL                        ok       = FALSE;
    ULARGE_INTEGER              size     = {};
    ULARGE_INTEGER              written  = {};



    ok = GetFileAttributesExW (widePath.c_str(), GetFileExInfoStandard, &data);
    CWR (ok);

    size.LowPart     = data.nFileSizeLow;
    size.HighPart    = data.nFileSizeHigh;
    written.LowPart  = data.ftLastWriteTime.dwLowDateTime;
    written.HighPart = data.ftLastWriteTime.dwHighDateTime;

    outStamp.sizeBytes    = size.QuadPart;
    outStamp.modifiedUnix = (int64_t) written.QuadPart;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool Win32DiskFileIo::Exists (const std::string & path)
{
    std::error_code  ec;



    return std::filesystem::exists (std::filesystem::path (path), ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::Remove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::Remove (const std::string & path)
{
    std::error_code  ec;



    std::filesystem::remove (std::filesystem::path (path), ec);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::ReplaceAtomically
//
//  MoveFileExW with REPLACE_EXISTING and WRITE_THROUGH, so an interrupted write
//  cannot leave the target half-written: it is either the old file or the new
//  one, never a mixture.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::ReplaceAtomically (const std::string & tempPath, const std::string & targetPath)
{
    HRESULT       hr       = S_OK;
    std::wstring  wideTemp = std::filesystem::path (tempPath).wstring();
    std::wstring  wideDest = std::filesystem::path (targetPath).wstring();
    BOOL          ok       = FALSE;



    ok = MoveFileExW (wideTemp.c_str(), wideDest.c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    CWR (ok);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::IsHeldByAnotherProcess
//
//  Best effort by nature. It catches another TOOL holding the file -- an editor,
//  a sync client, another disk utility -- and it CANNOT catch this emulator,
//  which reads a mounted image and closes it rather than keeping a handle. The
//  help text must not imply otherwise.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32DiskFileIo::IsHeldByAnotherProcess (const std::string & path)
{
    std::wstring  widePath = std::filesystem::path (path).wstring();
    HANDLE        handle   = INVALID_HANDLE_VALUE;
    bool          held     = false;



    handle = CreateFileW (widePath.c_str(), GENERIC_READ, 0, nullptr,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    held = handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION;

    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle (handle);
    }

    return held;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::WritePayloadToStandardOutput
//
//  THE _setmode CALL BELOW IS LOAD-BEARING. DO NOT REMOVE IT.
//
//  This platform opens a process's standard output in TEXT mode, so the runtime
//  rewrites every 0x0A byte on its way out as 0x0D 0x0A. Extracting a binary
//  that contains line-feed bytes would then deliver a longer file carrying
//  bytes that were never on the disk -- while reporting success.
//
//  Measured on the Merlin fixture disk: MAKE DUMP's payload is 589 bytes with
//  29 line feeds and no existing 0x0D 0x0A pair, so in text mode it arrives as
//  618. Those are the only two possible lengths, which is what makes the check
//  diagnostic; anything else means something other than the mode is wrong.
//
//  NO AUTOMATED TEST COVERS THIS. The translation happens in the runtime below
//  the seam, so the in-memory substitute the unit tests use does not perform it
//  and a test written against it would pass whether or not this call is here.
//  It cannot be verified at a higher level either: unit tests may not inspect
//  console handles or alter process state, and no test may run this binary. The
//  check is the manual procedure in specs/020-disk-file-access/quickstart.md.
//
//  The mode is set here rather than once at startup because diagnostics and
//  listings are TEXT and want the translation -- switching globally would strip
//  the line endings that make them read correctly in a terminal.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32DiskFileIo::WritePayloadToStandardOutput (const std::vector<Byte> & bytes)
{
    HRESULT  hr       = S_OK;
    int      previous = -1;
    size_t   written  = 0;
    size_t   wanted   = bytes.size();



    // Flush whatever text is pending BEFORE switching modes, or it leaves under
    // the wrong one.
    fflush (stdout);

    previous = _setmode (_fileno (stdout), _O_BINARY);
    CBR (previous != -1);

    written = fwrite (bytes.data(), 1, wanted, stdout);
    fflush (stdout);

    // Restore whatever the stream had, so anything printed afterwards -- a
    // diagnostic, a trailing newline -- is translated the way text should be.
    IGNORE_RETURN_VALUE (previous, -1);
    _setmode (_fileno (stdout), _O_TEXT);

    CBR (written == wanted);

Error:
    return hr;
}
