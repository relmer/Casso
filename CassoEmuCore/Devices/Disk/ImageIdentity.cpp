#include "Pch.h"

#include "ImageIdentity.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageIdentity::Read
//
//  The identity a file has at this moment.
//
//  A FAILED STAT YIELDS AN UNRECORDED IDENTITY, not a zeroed one. A file that
//  has been deleted must not compare equal to a file of zero bytes, and it must
//  not compare equal to whatever was recorded at mount either -- both would say
//  "nothing changed" about a disk that has gone.
//
////////////////////////////////////////////////////////////////////////////////

ImageIdentity ImageIdentity::Read (IDiskFileIo & fileIo, const std::string & path)
{
    ImageIdentity  identity;
    FileStamp      stamp;
    HRESULT        hr = fileIo.Stat (path, stamp);



    if (SUCCEEDED (hr))
    {
        identity.stamp    = stamp;
        identity.recorded = true;
    }

    return identity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageIdentity::ReadFromFileSystem
//
//  The same identity, read straight from the filesystem.
//
//  THE TIME IS A TICK COUNT AND NOT SECONDS, exactly as the platform seam's
//  Stat produces. Neither number is ever compared against the other's: an
//  identity is recorded and re-checked by one owner, using one source, and the
//  only thing either value has to do is differ when the file has been written.
//  Converting both to a common epoch would cost resolution and buy nothing.
//
//  A MISSING FILE COMES BACK UNRECORDED rather than zeroed, for the same reason
//  the seam overload does: gone must not read as unchanged.
//
////////////////////////////////////////////////////////////////////////////////

ImageIdentity ImageIdentity::ReadFromFileSystem (const std::string & path)
{
    ImageIdentity              identity;
    std::error_code            sizeEc;
    std::error_code            timeEc;
    fs::path                   fsPath (path);
    uintmax_t                  size = 0;
    fs::file_time_type         written;



    if (!path.empty())
    {
        size    = fs::file_size       (fsPath, sizeEc);
        written = fs::last_write_time (fsPath, timeEc);

        if (!sizeEc && !timeEc)
        {
            identity.stamp.sizeBytes    = (uint64_t) size;
            identity.stamp.modifiedUnix = (int64_t) written.time_since_epoch().count();
            identity.recorded           = true;
        }
    }

    return identity;
}
