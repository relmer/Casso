#pragma once

#include "Pch.h"

#include "DurableCommit.h"
#include "IDiskFileIo.h"
#include "MountedImageState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileOps
//
//  The disk store's file work, as a value a job copies to the I/O thread:
//  the seams a test can set (a write sink, a read function, an identity
//  function and the file interface) and the operations over them. It reads
//  and writes files only, never a bay, so a job can run it on any thread.
//
//  A seam's function runs on the thread the job runs on. The functions tests
//  set capture the test's buffers, which suits the inline queue only.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFileOps
{
public:
    using FlushSink      = std::function<HRESULT (const std::string &, const std::vector<Byte> &)>;
    using ImageReader    = std::function<HRESULT (const std::string &, std::vector<Byte> &)>;
    using IdentityReader = std::function<ImageIdentity (const std::string &)>;

    FlushSink        flushSink;
    ImageReader      imageReader;
    IdentityReader   identityReader;
    IDiskFileIo    * fileIo = nullptr;

    //  Puts bytes at a path: through the flush sink when one is set, and
    //  otherwise through DurableCommit over the file interface, or over a
    //  local Win32DiskFileIo when none is set.
    HRESULT        Commit        (const std::string & path, const std::vector<Byte> & bytes, CommitMode mode) const;

    //  The whole file, through the read function when one is set.
    HRESULT        Read          (const std::string & path, std::vector<Byte> & outBytes) const;

    //  The identity `path` has now, through the identity function when one is
    //  set.
    ImageIdentity  ReadIdentity  (const std::string & path) const;

    //  Whether something is at `path`, through the file interface when one is
    //  set.
    bool           Exists        (const std::string & path) const;

    //  The first preserved-copy name beside imagePath, for `stamp`, that
    //  nothing is at.
    HRESULT        FindFreePreservedPath (const std::string & imagePath, const std::string & stamp, std::string & outPath) const;

    //  A new recovery copy at the first free recovery name beside imagePath.
    //  With isNameFixed, the first name is used whatever is there, for a test
    //  whose sink captures every write.
    HRESULT        WriteNewRecoveryImage (const std::string & imagePath, const std::vector<Byte> & wozBytes, bool isNameFixed, std::string & outPath) const;

    //  Whether the host file can be written back to: its read-only attribute,
    //  or, when that is clear, a denied or locked read-write open. A missing
    //  or empty path is writable -- there is nothing to protect.
    static void    ProbeWritability (const std::string & path, bool & outReadOnly, bool & outNoPermission);
};
