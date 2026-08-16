#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileStamp
//
//  What the staleness check compares. Recorded when an image is read and
//  re-verified immediately before a write commits, so a change that landed in
//  between is refused rather than overwritten.
//
////////////////////////////////////////////////////////////////////////////////

struct FileStamp
{
    uint64_t  sizeBytes    = 0;
    int64_t   modifiedUnix = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskFileIo
//
//  Everything the disk commands need from the platform, and nothing else.
//
//  The point of the seam is that every DECISION sits above it -- which file to
//  read, what to compute, whether the result is safe to commit, what to say
//  when it is not -- while only the syscalls sit below. That is what lets the
//  whole command path be exercised by tests, since the project's test assembly
//  does not link the console executable at all.
//
//  WRITING BINARY PAYLOAD TO THE PROCESS'S OWN OUTPUT IS PART OF THIS SEAM, and
//  that is deliberate rather than tidy-minded. On this platform a process's
//  standard output is opened in TEXT mode, so the runtime rewrites every 0x0A
//  byte on its way out as 0x0D 0x0A. Extracting a binary that happens to
//  contain 0x0A would then produce a file one byte longer per occurrence, with
//  bytes injected that were never on the disk -- reporting success while
//  handing back something different from what was asked for.
//
//  A test seam cannot catch that on its own: the substitute records the correct
//  bytes in memory and passes, because the corruption happens in the runtime
//  BELOW the seam and only in real use. So the mode decision is made in ONE
//  place -- the platform implementation of this interface -- rather than at each
//  call site, where the second one would eventually be written without it.
//
//  Diagnostics and listings deliberately do NOT come through here. They are
//  text, they WANT the platform's line-ending translation, and routing them
//  through a binary sink would strip the very translation that makes them read
//  correctly in a terminal.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskFileIo
{
public:
    virtual ~IDiskFileIo () = default;

    virtual HRESULT  ReadAllBytes  (const std::string & path, std::vector<Byte> & outBytes) = 0;
    virtual HRESULT  WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes) = 0;

    //  Size and modification time, for the read-then-commit staleness check.
    virtual HRESULT  Stat          (const std::string & path, FileStamp & outStamp) = 0;

    virtual bool     Exists        (const std::string & path) = 0;
    virtual HRESULT  Remove        (const std::string & path) = 0;

    //  Replaces the target with the temporary in one step, so an interrupted
    //  write cannot leave a half-written image behind.
    virtual HRESULT  ReplaceAtomically (const std::string & tempPath,
                                        const std::string & targetPath) = 0;

    //  Opens the target for exclusive access and reports whether anything else
    //  already holds it. Best-effort by nature: it catches another TOOL, and
    //  cannot catch this emulator, which holds no handle on a mounted image.
    virtual bool     IsHeldByAnotherProcess (const std::string & path) = 0;

    //  Payload bytes to the process's own output, byte-for-byte. See the note
    //  above on why this belongs to the seam rather than to the caller.
    virtual HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> & bytes) = 0;
};
