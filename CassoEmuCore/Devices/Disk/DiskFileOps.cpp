#include "Pch.h"
#include "Core/LongPath.h"

#include "DiskFileOps.h"
#include "DiskImageStore.h"
#include "CommitPlan.h"
#include "PreservedCopy.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::Commit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileOps::Commit (const std::string & path, const std::vector<Byte> & bytes, CommitMode mode) const
{
    HRESULT               hr       = S_OK;
    Win32DiskFileIo       localIo;
    IDiskFileIo         & io       = (fileIo != nullptr) ? *fileIo : localIo;
    CommitPlan::Progress  progress = {};



    if (flushSink)
    {
        hr = flushSink (path, bytes);
        CHR (hr);
    }
    else
    {
        hr = DurableCommit::Commit (io, path, bytes, DiskImageStore::GetInvocationTag(), mode, progress);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileOps::Read (const std::string & path, std::vector<Byte> & outBytes) const
{
    HRESULT  hr = S_OK;



    if (imageReader)
    {
        hr = imageReader (path, outBytes);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = DiskImageStore::ReadFileBytes (path, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::ReadIdentity
//
////////////////////////////////////////////////////////////////////////////////

ImageIdentity DiskFileOps::ReadIdentity (const std::string & path) const
{
    if (identityReader)
    {
        return identityReader (path);
    }

    return ImageIdentity::ReadFromFileSystem (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFileOps::Exists (const std::string & path) const
{
    std::error_code  ec;



    if (fileIo != nullptr)
    {
        return fileIo->Exists (path);
    }

    return fs::exists (fs::path (LongPath::ToWin32 (path)), ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::FindFreePreservedPath
//
//  THE COLLISION LOOP IS NOT OPTIONAL. Two conflicts on one image inside a
//  second is exactly what a build loop produces, and with the one-second
//  stamp alone the second copy would overwrite the first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileOps::FindFreePreservedPath (const std::string & imagePath, const std::string & stamp, std::string & outPath) const
{
    HRESULT  hr      = S_OK;
    bool     isFree  = false;
    int      attempt = 0;



    for (attempt = 0; attempt < PreservedCopy::kMaxAttempts; attempt++)
    {
        std::string  candidate = PreservedCopy::MakePath (imagePath, stamp, attempt);

        if (!Exists (candidate))
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
//  DiskFileOps::WriteNewRecoveryImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFileOps::WriteNewRecoveryImage (const std::string & imagePath, const std::vector<Byte> & wozBytes, bool isNameFixed, std::string & outPath) const
{
    HRESULT  hr      = S_OK;
    int      attempt = 0;
    bool     isDone  = false;
    bool     isFree  = false;
    bool     isTaken = false;



    for (attempt = 0; !isDone && attempt < DiskImageStore::kMaxRecoveryNameAttempts; attempt++)
    {
        outPath = DiskImageStore::MakeRecoveryPath (imagePath, attempt);
        isFree  = isNameFixed || !Exists (outPath);

        if (isFree)
        {
            hr      = Commit (outPath, wozBytes, CommitMode::CreateNew);
            isTaken = hr == HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS) || hr == HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS);
            isDone  = isNameFixed || !isTaken;
        }
    }

    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps::ProbeWritability
//
//  Prefers the read-only attribute (surfaced via the owner_write perms bit on
//  Windows) so a plain read-only file reports its true cause; otherwise
//  probes with a non-truncating read+write open to catch ACL denials and
//  exclusive locks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFileOps::ProbeWritability (const std::string & path, bool & outReadOnly, bool & outNoPermission)
{
    std::error_code  ec;
    fs::file_status  st;
    fs::path         p (LongPath::ToWin32 (path));



    outReadOnly     = false;
    outNoPermission = false;

    if (!path.empty() && fs::exists (p, ec))
    {
        st = fs::status (p, ec);

        if (!ec && (st.permissions() & fs::perms::owner_write) == fs::perms::none)
        {
            outReadOnly = true;
        }
        else
        {
            std::fstream  probe (p, std::ios::in | std::ios::out | std::ios::binary);

            outNoPermission = !probe.good();
        }
    }
}
