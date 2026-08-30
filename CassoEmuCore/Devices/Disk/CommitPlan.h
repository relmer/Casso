#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlan
//
//  The policy a crash-safe commit follows, expressed as decisions over data
//  rather than as syscalls. Nothing here opens, writes or deletes anything --
//  it answers what the temporary should be called, whether the target moved
//  under us since we read it, and whether a temporary may still be sitting
//  there once the sequence stopped.
//
//  The separation is the point. A commit's failure paths are the ones that
//  never run in normal use, so they are exactly the paths that go untested when
//  the rules are inlined among the calls that would have to fail to reach them.
//  Pulled out here, every rule is reachable from a test with no platform
//  involved at all.
//
////////////////////////////////////////////////////////////////////////////////

class CommitPlan
{
public:
    //
    //  The steps a commit performs, in order.
    //
    //  Recorded as the furthest step ATTEMPTED, never the furthest COMPLETED. A
    //  write that fails partway can still have created the file it was writing
    //  -- truncated, or short -- so "the write failed" must mean "a temporary
    //  may exist" rather than "no temporary exists".
    //
    enum class Step
    {
        None,
        Probe,
        Reverify,
        WriteTemporary,
        Replace,
    };

    //  How far a commit got, and whether the replace consumed the temporary.
    struct Progress
    {
        Step  furthestAttempted = Step::None;
        bool  replaceSucceeded  = false;
    };

    static std::string  GetTemporaryPath (const std::string & targetPath,
                                          uint64_t            invocationTag,
                                          unsigned            attempt);

    static bool         IsStale         (uint64_t recordedSize,
                                         int64_t  recordedTime,
                                         uint64_t observedSize,
                                         int64_t  observedTime);

    static bool         ShouldRemoveTemporary (const Progress & progress);

    static uint64_t     NextInvocationTag     ();

    //  How many names a caller may try before giving up. Generous, because
    //  every attempt costs one existence check and the alternative to giving up
    //  is looping forever against whatever is producing the collisions.
    static constexpr unsigned  kMaxAttempts = 64;

    //  Every temporary this scheme derives ends with it, so a leftover is
    //  recognizable as ours by a human reading a directory listing and by a
    //  test sweeping for one.
    static constexpr const char *  kSuffix = ".tmp";

    //  And every one carries this, so a leftover is attributable to this tool
    //  rather than to whatever else writes .tmp files beside a disk image.
    static constexpr const char *  kMarker = ".casso-";
};
