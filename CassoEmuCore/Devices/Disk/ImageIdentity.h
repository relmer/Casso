#pragma once

#include "Pch.h"
#include "IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageIdentity
//
//  What is compared to answer "has this file changed since I read it".
//
//  A MOUNTED IMAGE IS READ ONCE AND THE FILE IS CLOSED, so nothing about a
//  mounted disk says whether the bytes behind it are still the bytes that were
//  loaded. This is what the emulator records at mount so that question has an
//  answer at all.
//
//  IT WRAPS FileStamp RATHER THAN RESTATING IT. The staleness check on the
//  command-line side already compares a size and a write time for exactly this
//  purpose; a second shape holding the same two fields would be two things to
//  keep in step.
//
//  `recorded` IS NOT DERIVABLE FROM THE OTHER TWO. A zero size and a zero time
//  are both legal, so a caller comparing against a default-constructed identity
//  would read "unchanged" out of a stat that never ran. DiskImageSession carries
//  a `stampRecorded` flag beside its own stamp for the same reason.
//
//  A SAME-SIZE OVERWRITE INSIDE THE FILESYSTEM'S TIMESTAMP RESOLUTION IS
//  INVISIBLE HERE, and therefore invisible to everything built on it. Closing
//  that would mean hashing the contents, which costs a full read of every
//  mounted image on every commit, to catch a case that needs a writer to
//  preserve the byte count exactly and land inside the same timestamp tick. The
//  residual is recorded rather than hidden.
//
////////////////////////////////////////////////////////////////////////////////

struct ImageIdentity
{
    FileStamp  stamp;
    bool       recorded = false;



    //  Whether two identities describe the same file contents.
    //
    //  AN UNRECORDED IDENTITY MATCHES NOTHING, including another unrecorded
    //  one. "I never looked" is not evidence that nothing changed.
    bool  Matches (const ImageIdentity & other) const
    {
        return recorded
            && other.recorded
            && stamp.sizeBytes    == other.stamp.sizeBytes
            && stamp.modifiedUnix == other.stamp.modifiedUnix;
    }



    //  The identity a file has right now, or an unrecorded one where it could
    //  not be stat'd -- a file that is gone stats as absent rather than as
    //  unchanged.
    static ImageIdentity  Read (IDiskFileIo & fileIo, const std::string & path);



    //  The same answer for a caller that has no IDiskFileIo to ask.
    //
    //  THE EMULATOR IS THAT CALLER. DiskImageStore reads and writes images
    //  through its own static filesystem helpers rather than through the
    //  command line's seam, so routing it through IDiskFileIo would mean
    //  threading an interface into a class that has no other use for one. The
    //  two overloads answer the same question from the same two fields; only
    //  the source of the stat differs.
    static ImageIdentity  ReadFromFileSystem (const std::string & path);
};
