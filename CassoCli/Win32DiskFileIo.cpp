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

#if defined(_DEBUG) && defined(CASSO_DIAG_DISK_ABORT)
    // One of the two chosen interruption points, and it is absent from every
    // binary anyone builds without asking for it. See Win32DiskFileIo.h for the
    // command that asks.
    AbortWithPartialFileIfRequested (file, bytes);
#endif

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



#if defined(_DEBUG) && defined(CASSO_DIAG_DISK_ABORT)
    // The other interruption point, and the valuable one: the temporary is
    // whole, the target has not been touched, and nothing else has ever
    // exercised this instant in the real code. Absent unless deliberately
    // armed; see Win32DiskFileIo.h.
    AbortBeforeReplaceIfRequested();
#endif

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
//  NO AUTOMATED TEST COVERS THIS, AND NONE CAN. The translation happens in the
//  runtime below the seam, so the in-memory substitute the unit tests use does
//  not perform it and a test written against it would pass whether or not this
//  call is here.
//
//  The narrower assertion -- "the handle is in binary mode before the write" --
//  is not available either, which is worth saying because it looks like the way
//  out. Querying or changing that mode inspects a console handle and mutates
//  real process state, both of which the test rules forbid outright. It would be
//  a bad trade regardless: this test framework runs everything in one process,
//  so a test flipping the mode would mutate state every other test shares.
//
//  The check is therefore the manual procedure in
//  specs/020-disk-file-access/quickstart.md, which also names the shell -- a
//  Windows PowerShell 5.1 redirect re-encodes the bytes and reports 1174,
//  which would read as a defect here rather than in the shell.
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
    int      restored = -1;
    size_t   written  = 0;
    size_t   wanted   = bytes.size();



    // Flush whatever text is pending BEFORE switching modes, or it leaves under
    // the wrong one.
    fflush (stdout);

    previous = _setmode (_fileno (stdout), _O_BINARY);
    CBR (previous != -1);

    written = fwrite (bytes.data(), 1, wanted, stdout);
    fflush (stdout);

    // Restore the mode the stream actually had, so anything printed afterwards
    // -- a diagnostic, a trailing newline -- is translated the way text should
    // be. Putting back a hardcoded _O_TEXT would IMPOSE text mode rather than
    // restore, which is only indistinguishable while every caller happens to
    // start in text mode.
    restored = _setmode (_fileno (stdout), previous);

    // The payload result is the one the caller asked about, so it is checked
    // first; a stream left in the wrong mode matters, but not more than bytes
    // that did not arrive.
    CBR (written == wanted);
    CBR (restored != -1);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::IsAbortStageRequested
//
//  THE INTERRUPTION SWITCH FOR THE COMMIT PATH. Not in any binary that was not
//  deliberately built to carry it -- see Win32DiskFileIo.h for the command.
//
//  The guarantee this edge makes is that stopping the process partway through a
//  write leaves the image wholly old or wholly new and never a mixture. Proving
//  it from OUTSIDE was tried and does not work. Seventy attempts across two
//  methods -- polling, and a change watcher armed on the temporary's creation,
//  which fired on thirty-nine of forty tries -- never once ended with the
//  original still in place. Every kill landed after the replace, because the
//  window between the temporary's last byte and the rename is shorter than the
//  time this platform takes to stop a process from another one. That is
//  reassuring about how exposed the path is, and it demonstrates nothing
//  whatsoever about the instant in question, since the instant was never
//  reached.
//
//  So the interruption is raised from INSIDE the code under test, at a point
//  chosen rather than raced for.
//
//  THIS DOES NOT OVERLAP THE SUBSTITUTE FILE INTERFACE, and both are needed.
//  The substitute covers every DECISION above the seam -- the ordering, each
//  refusal, the cleanup rule -- deterministically, in the test assembly, over
//  nothing real. What it structurally cannot cover is this class: the test
//  project does not link the console executable, and the property at stake is
//  what a REAL file on a REAL filesystem looks like once the process has
//  stopped existing. A substitute that recorded the right intentions in memory
//  would pass either way.
//
//  IT CANNOT EXIST UNLESS SOMEBODY ASKED FOR IT. The declarations, these
//  definitions and both call sites are gated on a define no project
//  configuration sets, so a stock build holds no code to reach and not even the
//  strings that name the stages -- the switch is a compile-time absence, not a
//  runtime flag that happens to be off. Setting the environment variable
//  against such a binary does nothing, because there is nothing there to read
//  it, and that is what stops a forgotten setx from quietly aborting every
//  write on somebody's machine.
//
////////////////////////////////////////////////////////////////////////////////

#if defined(_DEBUG) && defined(CASSO_DIAG_DISK_ABORT)

bool Win32DiskFileIo::IsAbortStageRequested (const wchar_t * stage)
{
    constexpr DWORD  kMaxStageLength         = 32;
    wchar_t          buffer[kMaxStageLength] = {};
    DWORD            length                  = 0;
    std::wstring     requested;
    bool             matches                 = false;



    length = GetEnvironmentVariableW (kpszAbortVariable, buffer, kMaxStageLength);

    // Absent yields zero, and a value too long for the buffer yields the size
    // it would need while leaving the buffer empty. Neither names a stage, and
    // treating the second as a match would fire on a truncation.
    if (length > 0 && length < kMaxStageLength)
    {
        requested.assign (buffer, length);
    }

    matches = !requested.empty() && requested == stage;

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::AbortWithPartialFileIfRequested
//
//  The lesser of the two points: stop with a half-written temporary sitting
//  beside the image. Half the bytes are pushed to the filesystem rather than
//  left in the stream's buffer, because a buffer that never drains would leave
//  an EMPTY file behind and that is a different case -- a reader can tell a
//  zero-length file is unfinished, and a short one it cannot.
//
////////////////////////////////////////////////////////////////////////////////

void Win32DiskFileIo::AbortWithPartialFileIfRequested (
    std::ofstream            & file,
    const std::vector<Byte>  & bytes)
{
    constexpr size_t  kPartDivisor = 2;

    bool              requested    = false;
    size_t            partial      = 0;



    requested = IsAbortStageRequested (kpszStageDuringWrite);

    if (!requested)
    {
        return;
    }

    partial = bytes.size() / kPartDivisor;

    file.write (reinterpret_cast<const char *> (bytes.data()), (std::streamsize) partial);
    file.flush();

    AbortProcessNow (kpszStageDuringWrite);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::AbortBeforeReplaceIfRequested
//
//  The point worth having. The temporary is complete and flushed -- the stream
//  that wrote it was destroyed when the write returned -- and the target has
//  not been touched. Everything the guarantee claims is riding on the single
//  step that has not happened yet.
//
////////////////////////////////////////////////////////////////////////////////

void Win32DiskFileIo::AbortBeforeReplaceIfRequested()
{
    bool  requested = false;



    requested = IsAbortStageRequested (kpszStageBeforeSwap);

    if (requested)
    {
        AbortProcessNow (kpszStageBeforeSwap);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIo::AbortProcessNow
//
//  A HARD STOP, DELIBERATELY. No unwinding, no destructors, no atexit, no
//  stream flushed on the way out -- because that is what a crash is, and a
//  tidier exit would let exactly the cleanup this exercise is meant to do
//  without run anyway and prove nothing.
//
//  The line on standard error is written and flushed FIRST, since after this
//  call there is no process to write it. It is what tells an operator the stop
//  was the switch firing rather than the tool falling over on its own; the exit
//  code says the same thing to a script.
//
////////////////////////////////////////////////////////////////////////////////

void Win32DiskFileIo::AbortProcessNow (const wchar_t * stage)
{
    constexpr UINT  kAbortExitCode = 0xDEAD;



    fprintf (stderr, "disk: stopping hard at '%ls' -- debug interruption switch\n", stage);
    fflush (stderr);

    TerminateProcess (GetCurrentProcess(), kAbortExitCode);
}

#endif
