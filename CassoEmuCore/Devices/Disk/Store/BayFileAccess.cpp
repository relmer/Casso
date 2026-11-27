#include "Pch.h"

#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Devices/Disk/Store/DiskBay.h"
#include "Devices/Disk/Store/DiskFileCommit.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::MakeRecoveryPath
//
//  Recovery images sit beside the original under its own name, so the pairing
//  is obvious in a folder listing. WOZ because it is the only format that can
//  hold what the sector formats could not -- including the very track whose
//  content caused the refusal.
//
//  The attempt index exists so an earlier recovery is never overwritten; losing
//  a previous rescue to a later one would repeat the mistake being fixed.
//
////////////////////////////////////////////////////////////////////////////////

string BayFileAccess::MakeRecoveryPath (const string & imagePath, int attempt)
{
    fs::path  base = fs::path (imagePath);
    string    stem;



    base.replace_extension();
    stem = base.string();

    if (attempt <= 0)
    {
        return stem + ".recovered.woz";
    }

    return stem + ".recovered." + std::to_string (attempt) + ".woz";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::TryWriteRecoveryImage
//
//  Serializes the live image to WOZ and lands it beside the original. WOZ is
//  chosen deliberately over the denibblized sector buffer: that buffer holds
//  zeros exactly where the unreadable track should be, so writing it would
//  discard the one thing the refusal was protecting. WozLoader::Serialize takes
//  the per-track bit streams verbatim and is format-agnostic, so a .dsk-sourced
//  mount round-trips whole.
//
//  Never overwrites an existing file, and never touches the original or what is
//  currently mounted -- preserving the work and adopting it are different
//  operations, and only the first belongs here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BayFileAccess::TryWriteRecoveryImage (DiskBay & entry, string & outPath) const
{
    HRESULT       hr        = S_OK;
    string        candidate;
    int           attempt   = 0;
    bool          haveImage = entry.image != nullptr;
    bool          havePath  = !entry.path.empty();
    bool          haveName  = false;
    vector<Byte>  wozBytes;



    outPath.clear();

    CBR (haveImage);
    CBR (havePath);

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    for (attempt = 0; !haveName && attempt < kMaxRecoveryNameAttempts; attempt++)
    {
        candidate = MakeRecoveryPath (entry.path, attempt);

        // With a sink installed there is no host file to collide with -- the
        // sink is the filesystem, and it decides what to do with the name.
        if (m_hooks.flushSink)
        {
            haveName = true;
        }
        else
        {
            bool  taken = fs::exists (fs::path (candidate));

            haveName = !taken;
        }
    }

    CBREx (haveName, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    if (m_hooks.flushSink)
    {
        hr = m_hooks.flushSink (candidate, wozBytes);
        CHR (hr);
    }
    else
    {
        //  Same rule as WriteFileAtomically: the filesystem's own code for a
        //  refusal travels out, not E_FAIL. The name was just verified free,
        //  so CREATE_NEW is faithful and refuses a race for it rather than
        //  overwriting whoever won.
        std::wstring  wide     = fs::path (candidate).wstring();
        HANDLE        file     = INVALID_HANDLE_VALUE;
        DWORD         written  = 0;
        DWORD         lastErr  = ERROR_SUCCESS;
        DWORD         expected = static_cast<DWORD> (wozBytes.size());
        BOOL          ok       = FALSE;

        file = CreateFileW (wide.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        CWR (file != INVALID_HANDLE_VALUE);

        ok      = WriteFile (file, wozBytes.data(), static_cast<DWORD> (wozBytes.size()),
                             &written, nullptr);
        lastErr = ok ? ERROR_SUCCESS : GetLastError();

        CloseHandle (file);

        CBREx (ok, HRESULT_FROM_WIN32 (lastErr));
        CBREx (written == expected, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

    outPath = candidate;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadImageFile
//
//  Whole-file read for a mounted image's backing file, through the test
//  reader hook when one is installed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BayFileAccess::ReadImageFile (const string & path, vector<Byte> & bytes) const
{
    HRESULT   hr = S_OK;



    if (m_hooks.imageReader)
    {
        hr = m_hooks.imageReader (path, bytes);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = DiskFileCommit::ReadFileBytes (path, bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::ReadIdentity
//
//  What the file at `path` looks like right now.
//
//  THROUGH THE SEAM WHEN ONE IS INSTALLED. A test that has redirected reads and
//  writes into memory has no file to stat, and reaching past the seam to the
//  filesystem here would have the store checking one world and writing another.
//
////////////////////////////////////////////////////////////////////////////////

ImageIdentity BayFileAccess::ReadIdentity (const string & path) const
{
    ImageIdentity  identity;



    if (m_hooks.identityReader)
    {
        identity = m_hooks.identityReader (path);
    }
    else
    {
        identity = ImageIdentity::ReadFromFileSystem (path);
    }

    return identity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::DoesPathExist
//
//  Whether something is already sitting at a path.
//
//  THROUGH THE FILE SEAM WHEN ONE IS INSTALLED, so a test that keeps its files
//  in memory is asked about the world it actually has. Reaching past it to the
//  filesystem would let the collision loop below hand out a name a test already
//  used.
//
////////////////////////////////////////////////////////////////////////////////

bool BayFileAccess::DoesPathExist (const string & path) const
{
    std::error_code  ec;



    if (m_hooks.fileIo != nullptr)
    {
        return m_hooks.fileIo->Exists (path);
    }

    return fs::exists (fs::path (path), ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::FindFreePreservedPath
//
//  A preserved-copy name nothing is sitting at yet.
//
//  THE LOOP IS THE POINT. A one-second timestamp cannot keep the promise that
//  repeated conflicts accumulate rather than overwrite each other, and two
//  conflicts on one image inside a second is exactly what a build loop makes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BayFileAccess::FindFreePreservedPath (const string & imagePath, string & outPath) const
{
    HRESULT  hr      = S_OK;
    string   stamp   = PreservedCopy::MakeStamp (m_hooks.timestamp ? m_hooks.timestamp() : time (nullptr));
    bool     isFree  = false;
    int      attempt = 0;



    for (attempt = 0; attempt < PreservedCopy::kMaxAttempts; attempt++)
    {
        string  candidate = PreservedCopy::MakePath (imagePath, stamp, attempt);

        if (!DoesPathExist (candidate))
        {
            outPath = candidate;
            isFree  = true;
            break;
        }
    }

    //  Ninety-nine names taken in one second is not a collision any more, it is
    //  something else going wrong, and inventing a hundredth would not help.
    CBR (isFree);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::WritePreserved
//
//  Writes a preserved copy.
//
//  THROUGH THE SAME SINK AS A FLUSH, so a test captures preserved copies the
//  way it captures everything else this class writes, and so the atomic
//  temp-then-rename applies to them too. A preserved copy that could be found
//  half-written would be worth nothing at the moment it is needed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BayFileAccess::WritePreserved (const string & path, const vector<Byte> & bytes) const
{
    HRESULT  hr = S_OK;



    if (m_hooks.flushSink)
    {
        hr = m_hooks.flushSink (path, bytes);
    }
    else
    {
        hr = DiskFileCommit::WriteFileAtomically (path, bytes);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccess::SaveLoadedImage
//
//  Writes what the bay is holding to a file of its own.
//
//  SERIALIZED FROM THE MOUNTED IMAGE, NOT COPIED FROM THE FILE. The entire
//  reason this version needs preserving is that the file no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BayFileAccess::SaveLoadedImage (DiskBay & entry, string & outPath) const
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;



    CBR (entry.image != nullptr);

    hr = entry.image->Serialize (bytes);
    CHR (hr);

    //  A name already reserved is used as it is. Choosing a fresh one here is
    //  what let a question and a flush disagree about where the copy went.
    if (outPath.empty())
    {
        hr = FindFreePreservedPath (entry.path, outPath);
        CHR (hr);
    }

    hr = WritePreserved (outPath, bytes);
    CHR (hr);

Error:
    //  outPath IS LEFT AS THE PATH THAT WAS TRIED, even on failure. The notice
    //  raised for a failed copy prints where it tried to write, and clearing it
    //  here left that notice with nothing to show but an error code.
    return hr;
}





