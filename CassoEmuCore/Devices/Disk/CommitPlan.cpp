#include "Pch.h"

#include "CommitPlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlan::TemporaryPathFor
//
//  The temporary sits BESIDE the target -- same directory -- because an atomic
//  replace cannot cross a volume boundary, and a temporary in the system's temp
//  folder is on a different volume often enough that the guarantee would hold
//  only on whichever machine it was tried on.
//
//  Appending to the whole target path is what puts it there, without this
//  function needing to know anything about how paths are spelled. It also makes
//  the name obviously derived from the file it belongs to, which is what a
//  human wants when they find one left behind.
//
//  TWO INVOCATIONS OF THIS TOOL AGAINST THE SAME IMAGE MUST NOT DERIVE THE SAME
//  NAME, and the attempt counter alone cannot deliver that. Both invocations
//  begin at attempt zero, and both look before they leap, so both can see the
//  name free and take it -- the loser's bytes then go into the winner's
//  temporary and are committed as though they were the winner's. The invocation
//  tag is what separates them: it varies per process and per commit within a
//  process, so the two never start from the same name and the existence check
//  is left doing what it is actually good at, which is stepping over a
//  temporary somebody abandoned earlier.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommitPlan::TemporaryPathFor (
    const std::string & targetPath,
    uint64_t            invocationTag,
    unsigned            attempt)
{
    char  distinguisher[64] = {};



    snprintf (distinguisher, sizeof (distinguisher), "%016llX-%u",
              (unsigned long long) invocationTag, attempt);

    return targetPath + kMarker + distinguisher + kSuffix;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlan::IsStale
//
//  Whether the file changed between being read and being committed over.
//
//  BOTH COMPARISONS ARE INEQUALITY, NOT ORDERING, and that is deliberate. A
//  file restored from a backup carries the backup's older timestamp, and a
//  truncating writer leaves a smaller file -- so "the observed value is newer"
//  or "larger" would call both of those unchanged and overwrite somebody's
//  work. Any difference at all is a different file as far as this check is
//  concerned.
//
//  It cannot detect a write that lands AFTER the commit. Nothing can, short of
//  holding the file open for the whole operation, which would make the tool
//  unusable alongside anything else. The documentation says so rather than
//  implying a guarantee the check does not carry.
//
////////////////////////////////////////////////////////////////////////////////

bool CommitPlan::IsStale (
    uint64_t  recordedSize,
    int64_t   recordedTime,
    uint64_t  observedSize,
    int64_t   observedTime)
{
    return (recordedSize != observedSize) || (recordedTime != observedTime);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlan::ShouldRemoveTemporary
//
//  The ordering rule, in one place: a temporary is removed on ANY failure, and
//  only a successful replace excuses the removal -- because that is the one
//  outcome in which the temporary is no longer there, having become the target.
//
//  Stated the other way round, which is how it gets written wrong: this is NOT
//  "remove it if the write succeeded". A write that failed halfway is precisely
//  the case where a partial file is sitting there, and it is the case a caller
//  is most tempted to skip, having just been told the write did not happen.
//
////////////////////////////////////////////////////////////////////////////////

bool CommitPlan::ShouldRemoveTemporary (const Progress & progress)
{
    bool  couldExist = (progress.furthestAttempted == Step::WriteTemporary)
                    || (progress.furthestAttempted == Step::Replace);



    return couldExist && !progress.replaceSucceeded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlan::NextInvocationTag
//
//  A value that separates this commit from every other one, here or in another
//  process, so the names two of them derive cannot be the same.
//
//  This is the one thing in this file that is not a pure function of its
//  arguments, and it is kept apart for that reason rather than folded into the
//  name derivation. The process identifier is what separates two tool runs, and
//  two tool runs is what the concurrent case actually looks like today. The
//  sequence separates two takers inside one process, which costs a word and
//  keeps the guarantee true for a caller that commits more than once.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CommitPlan::NextInvocationTag()
{
    static std::atomic<uint32_t>  nextSequence  = 0;
    constexpr int                 kProcessShift = 32;



    return ((uint64_t) GetCurrentProcessId() << kProcessShift) | nextSequence++;
}
