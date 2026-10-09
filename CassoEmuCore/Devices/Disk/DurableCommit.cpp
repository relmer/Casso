#include "Pch.h"

#include "Devices/Disk/DurableCommit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DurableCommit::Commit
//
//  THE CLEANUP IS DRIVEN BY THE PLAN, NOT BY EACH FAILURE PATH: it is asked
//  once, at the single exit, from a record of the furthest step attempted,
//  since a step that failed partway can still have left a temporary behind.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DurableCommit::Commit (
    IDiskFileIo             & fileIo,
    const std::string       & targetPath,
    const std::vector<Byte> & bytes,
    uint64_t                  invocationTag,
    CommitMode                mode,
    CommitPlan::Progress    & progress)
{
    HRESULT      hr       = S_OK;
    HRESULT      removeHr = S_OK;
    bool         isTaken  = fileIo.Exists (targetPath);
    std::string  tempPath;



    if (mode == CommitMode::CreateNew)
    {
        CBREx (!isTaken, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));
    }

    hr = FindTemporaryPath (fileIo, targetPath, invocationTag, tempPath);
    CHR (hr);

    progress.furthestAttempted = CommitPlan::Step::WriteTemporary;

    hr = fileIo.WriteAllBytes (tempPath, bytes);
    CHR (hr);

    if (mode == CommitMode::Replace && isTaken)
    {
        progress.furthestAttempted = CommitPlan::Step::CopyMetadata;

        hr = fileIo.CopyFileMetadata (targetPath, tempPath);
        CHR (hr);
    }

    progress.furthestAttempted = CommitPlan::Step::FlushTemporary;

    hr = fileIo.FlushToStorage (tempPath);
    CHR (hr);

    progress.furthestAttempted = CommitPlan::Step::Replace;

    hr = (mode == CommitMode::Replace) ? fileIo.ReplaceAtomically (tempPath, targetPath)
                                       : fileIo.RenameWithoutReplacing (tempPath, targetPath);
    CHR (hr);

    progress.replaceSucceeded = true;

Error:
    if (!tempPath.empty() && CommitPlan::ShouldRemoveTemporary (progress))
    {
        removeHr = fileIo.Remove (tempPath);
        IGNORE_RETURN_VALUE (removeHr, S_OK);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DurableCommit::FindTemporaryPath
//
//  Steps over anything already at the names this invocation would take; the
//  invocation tag in the name keeps two live commits apart.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DurableCommit::FindTemporaryPath (IDiskFileIo & fileIo, const std::string & targetPath, uint64_t invocationTag, std::string & outTempPath)
{
    HRESULT   hr      = S_OK;
    bool      isFree  = false;
    unsigned  attempt = 0;



    for (attempt = 0; attempt < CommitPlan::kMaxAttempts && !isFree; attempt++)
    {
        outTempPath = CommitPlan::GetTemporaryPath (targetPath, invocationTag, attempt);
        isFree      = !fileIo.Exists (outTempPath);
    }

    if (!isFree)
    {
        outTempPath.clear();
    }

    CBREx (isFree, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

Error:
    return hr;
}
