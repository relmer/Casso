#include "Pch.h"

#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Devices/Disk/CommitPlan.h"
#include "DiskCheck/ThumbnailStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFileAtomically
//
//  Write to a sibling temp file, verify every step, then rename it over the
//  target. The verification matters as much as the temp file: an ofstream
//  reports a short or failed write only through its stream state, so a full
//  volume otherwise completes a flush that wrote nothing and looks identical
//  to success. The state is read after close, since bytes can still be
//  buffered when write() returns.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommit::WriteFileAtomically (const string & path, const vector<Byte> & bytes)
{
    HRESULT           hr        = S_OK;
    bool              hasPath   = !path.empty();
    bool              foundFree = false;
    unsigned          attempt   = 0;
    string            tempPath;
    std::error_code   ec;
    size_t            byteCount = bytes.size();
    HRESULT           hrThumb   = S_OK;



    CBRAEx (hasPath, E_INVALIDARG);

    //  THE NAME USED TO BE A FIXED SUFFIX AND THAT WAS A DEFECT. Two emulators
    //  holding one image derived the same temporary from it, so each wrote into
    //  the other's file and one of them renamed the other's bytes over the
    //  target as its own.
    //
    //  CommitPlan ALREADY SOLVED THIS FOR THE COMMAND LINE, with a per-process
    //  tag and a step past anything already sitting at the name, and its own
    //  comment names the emulator as the side that still had the bug. A second
    //  scheme here would be a second thing to keep right; this is the same one.
    //
    //  STEPPING PAST AN EXISTING NAME IS WHAT HANDLES AN ABANDONED TEMPORARY.
    //  A writer that was killed mid-commit leaves one behind, and adopting it
    //  would mean appending this image to the remains of somebody else's.
    for (attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
    {
        tempPath  = GetCommitTemporaryPath (path, attempt);
        foundFree = !fs::exists (fs::path (tempPath), ec);

        if (foundFree)
        {
            break;
        }
    }

    CBREx (foundFree, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    //  A disk image is a few hundred KB; a payload past 4 GB is a caller bug.
    CBRAEx (byteCount <= MAXDWORD, E_INVALIDARG);

    //  THE REAL ERROR TRAVELS. This went through an ofstream and a CBR, which
    //  turned a folder that refused the temporary, a read-only target and a
    //  full disk all into E_FAIL -- and the save-failure notice, built to
    //  print the code and the system's own words for it, said "0x80004005
    //  Unspecified error" for a permission problem. CreateFileW and CWR carry
    //  the Win32 code out whole. The write's error is captured BEFORE the
    //  handle closes, because CloseHandle would overwrite it.
    {
        std::wstring  wideTemp = fs::path (tempPath).wstring();
        HANDLE        file     = INVALID_HANDLE_VALUE;
        DWORD         written  = 0;
        DWORD         lastErr  = ERROR_SUCCESS;
        DWORD         expected = static_cast<DWORD> (byteCount);
        BOOL          ok       = TRUE;

        file = CreateFileW (wideTemp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CWR (file != INVALID_HANDLE_VALUE);

        if (!bytes.empty())
        {
            ok      = WriteFile (file, bytes.data(), static_cast<DWORD> (bytes.size()),
                                 &written, nullptr);
            lastErr = ok ? ERROR_SUCCESS : GetLastError();
        }

        CloseHandle (file);

        CBREx (ok, HRESULT_FROM_WIN32 (lastErr));
        CBREx (written == expected, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

    //  The image's thumbnail is in a stream of the file, which the rename
    //  would replace with the temporary's.
    hrThumb = ThumbnailStore::CopyStream (fs::path (path), fs::path (tempPath));
    IGNORE_RETURN_VALUE (hrThumb, S_OK);

    // Rename replaces an existing target, so the swap is one filesystem
    // operation: readers see either the old file or the new one. The code the
    // filesystem gave for a refusal travels with it rather than becoming E_FAIL.
    fs::rename (tempPath, path, ec);
    CBREx (!ec, HRESULT_FROM_WIN32 (ec.value()));

Error:
    if (FAILED (hr))
    {
        std::error_code  cleanupEc;

        // Best effort -- the guarantee is that the TARGET is untouched, not
        // that the temp never lingers, and a temp we cannot remove must not
        // turn into a second error report.
        fs::remove (tempPath, cleanupEc);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileBytes
//
//  Reads a whole file. Reports failure rather than returning a short buffer:
//  a caller that cannot tell a truncated read from a small file will happily
//  write the truncation back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileCommit::ReadFileBytes (const string & path, vector<Byte> & bytes)
{
    HRESULT     hr      = S_OK;
    bool        hasPath = !path.empty();
    bool        fileOk  = false;
    bool        readOk  = false;
    streamsize  size    = 0;



    CBRAEx (hasPath, E_INVALIDARG);

    {
        ifstream  file (path, ios::binary | ios::ate);

        fileOk = file.good();
        CBR (fileOk);

        size = file.tellg();
        CBR (size >= 0);

        file.seekg (0, ios::beg);
        bytes.resize (static_cast<size_t> (size));

        if (size > 0)
        {
            file.read (reinterpret_cast<char *> (bytes.data()),
                       static_cast<streamsize> (size));

            readOk = (file.gcount() == size);
            CBR (readOk);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommit::GetCommitTemporaryPath
//
//  Where a commit of `path` writes before it renames.
//
//  ONE INVOCATION TAG FOR THE WHOLE PROCESS, taken once. Two emulators get
//  different tags and therefore different names; two flushes inside one
//  emulator get the same tag and are separated by the attempt counter and the
//  existence check, which is enough because they cannot overlap -- every flush
//  runs on the one thread that owns disk writes.
//
////////////////////////////////////////////////////////////////////////////////

string DiskFileCommit::GetCommitTemporaryPath (const string & path, unsigned attempt)
{
    static const uint64_t  s_kInvocationTag = CommitPlan::NextInvocationTag();



    return CommitPlan::GetTemporaryPath (path, s_kInvocationTag, attempt);
}





