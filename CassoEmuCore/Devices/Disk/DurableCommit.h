#pragma once

#include "Pch.h"

#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommitMode
//
//  Replace puts the new bytes where a file may already be; CreateNew makes a
//  file that must not exist yet, and fails with ERROR_ALREADY_EXISTS rather
//  than overwrite one.
//
////////////////////////////////////////////////////////////////////////////////

enum class CommitMode
{
    Replace,
    CreateNew,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DurableCommit
//
//  Puts bytes at a path so that an interruption at any moment leaves either
//  the old file or the new one, whole, and the new one survives a power
//  loss once Commit returns. In order: write a temporary beside the target,
//  give it the target's metadata when replacing an existing file, flush it
//  to storage, then make it the target in one step. The metadata is copied
//  when it can be; a failure of any other step removes the temporary and
//  leaves the target as it was.
//
//  The caller's checks (whether another program holds the file, whether it
//  changed since it was read) come before this; progress records the step
//  reached, so the caller can say which one failed.
//
////////////////////////////////////////////////////////////////////////////////

class DurableCommit
{
public:
    static HRESULT  Commit (IDiskFileIo            & fileIo,
                            const std::string      & targetPath,
                            const std::vector<Byte> & bytes,
                            uint64_t                 invocationTag,
                            CommitMode               mode,
                            CommitPlan::Progress   & progress);

private:
    static HRESULT  FindTemporaryPath (IDiskFileIo & fileIo, const std::string & targetPath, uint64_t invocationTag, std::string & outTempPath);
};
