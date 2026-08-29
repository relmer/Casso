#include "Pch.h"
#include "Devices/Disk/CommitPlan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPlanTests
//
//  The commit policy on its own, with no platform underneath it. Every rule
//  here decides something a real commit only reaches once something else has
//  already gone wrong, which is exactly why they are separated out: a rule
//  inlined among the calls that must fail to reach it is a rule nobody tests.
//
////////////////////////////////////////////////////////////////////////////////


TEST_CLASS (CommitPlanTests)
{
public:

    static constexpr const char *  kTarget = "C:\\disks\\merlin.dsk";

    TEST_METHOD (TemporaryPath_SitsBesideTheTarget)
    {
        // Same directory, because an atomic replace cannot cross a volume and
        // the system temp folder is on a different one often enough that the
        // guarantee would hold only on the machine it was tried on. Deriving
        // the name by appending to the whole target path is what puts it there.
        std::string  temp = CommitPlan::TemporaryPathFor (kTarget, 1, 0);

        Assert::AreEqual (size_t (0), temp.find (kTarget),
            L"the temporary must be derived from the whole target path, so it lands beside it");

        Assert::IsFalse (temp == kTarget,
            L"and it must never BE the target -- that is the write this whole plan avoids");
    }

    TEST_METHOD (TemporaryPath_IsRecognizableAsALeftoverOfThisTool)
    {
        // Two separate jobs. The suffix is what makes a sweep for leftovers
        // possible at all; the marker is what stops that sweep claiming
        // somebody else's temporary file as ours.
        std::string  temp   = CommitPlan::TemporaryPathFor (kTarget, 0x1234, 3);
        std::string  suffix = CommitPlan::kSuffix;

        Assert::IsTrue (temp.size() > suffix.size());
        Assert::AreEqual (suffix, temp.substr (temp.size() - suffix.size()),
            L"a leftover has to be findable by suffix");

        Assert::IsTrue (temp.find (CommitPlan::kMarker) != std::string::npos,
            L"and attributable to this tool rather than to whatever else writes .tmp files");
    }

    TEST_METHOD (TemporaryPath_IsAFunctionOfItsArgumentsAndNothingElse)
    {
        // Purity is the property that makes every other test in this file
        // mean something. If the derivation consulted a clock or a counter,
        // the comparisons below would be measuring that instead of the rule.
        Assert::AreEqual (CommitPlan::TemporaryPathFor (kTarget, 7, 2),
                          CommitPlan::TemporaryPathFor (kTarget, 7, 2));
    }

    TEST_METHOD (TemporaryPath_DiffersByAttempt)
    {
        Assert::IsFalse (CommitPlan::TemporaryPathFor (kTarget, 7, 0)
                      == CommitPlan::TemporaryPathFor (kTarget, 7, 1),
            L"stepping the attempt must step the name, or the retry loop spins on one name");
    }

    TEST_METHOD (TemporaryPath_DiffersByInvocationAtTheSAMEAttempt)
    {
        // THE ATTEMPT COUNTER ALONE CANNOT CARRY THIS, which is why the tag
        // exists. Two invocations against one image both begin at attempt zero
        // and both look before they leap, so both can find the name free in the
        // same instant and take it -- and the loser's bytes then get committed
        // as the winner's. The comparison that matters is therefore at EQUAL
        // attempt: a test that varied the attempt as well would pass against a
        // derivation that ignored the tag completely.
        Assert::IsFalse (CommitPlan::TemporaryPathFor (kTarget, 7, 0)
                      == CommitPlan::TemporaryPathFor (kTarget, 8, 0),
            L"two invocations must not derive the same temporary name");
    }

    TEST_METHOD (TemporaryPath_DiffersByTarget)
    {
        Assert::IsFalse (CommitPlan::TemporaryPathFor ("C:\\a.dsk", 7, 0)
                      == CommitPlan::TemporaryPathFor ("C:\\b.dsk", 7, 0));
    }

    TEST_METHOD (InvocationTag_IsNeverTheSameTwice)
    {
        // What makes the test above reachable in practice. A tag that came back
        // constant would satisfy every purity check and still collide on every
        // concurrent write.
        uint64_t  first  = CommitPlan::NextInvocationTag();
        uint64_t  second = CommitPlan::NextInvocationTag();
        uint64_t  third  = CommitPlan::NextInvocationTag();

        Assert::IsFalse (first == second);
        Assert::IsFalse (second == third);
        Assert::IsFalse (first == third);

        Assert::IsFalse (CommitPlan::TemporaryPathFor (kTarget, first,  0)
                      == CommitPlan::TemporaryPathFor (kTarget, second, 0),
            L"and successive tags must actually reach the name");
    }

    TEST_METHOD (IsStale_WhenNothingMoved_IsFalse)
    {
        Assert::IsFalse (CommitPlan::IsStale (1024, 500, 1024, 500));
    }

    TEST_METHOD (IsStale_WhenOnlyTheTimeMoved_IsTrue)
    {
        // A same-size edit is the ordinary case: somebody opened the image in
        // another tool and wrote a file into a sector that was already there.
        Assert::IsTrue (CommitPlan::IsStale (1024, 500, 1024, 501),
            L"a rewrite that preserves the size still replaced the file");
    }

    TEST_METHOD (IsStale_WhenOnlyTheSizeMoved_IsTrue)
    {
        // And the reverse, because filesystem timestamps are coarse: two writes
        // inside one tick carry the same time.
        Assert::IsTrue (CommitPlan::IsStale (1024, 500, 2048, 500));
    }

    TEST_METHOD (IsStale_WhenTheTimeWentBACKWARD_IsTrue)
    {
        // THE COMPARISON IS INEQUALITY, NOT ORDERING. A file restored from a
        // backup carries the backup's older timestamp -- and a check reading
        // "observed is newer" would call that unchanged and overwrite the
        // restore. This case is the one that separates the two rules; every
        // other staleness case passes under either.
        Assert::IsTrue (CommitPlan::IsStale (1024, 500, 1024, 499),
            L"an older timestamp is a different file, not an unchanged one");
    }

    TEST_METHOD (IsStale_WhenTheFileSHRANK_IsTrue)
    {
        // The same trap on the other field: a truncating writer leaves a
        // smaller file, and "observed is larger" would wave it through.
        Assert::IsTrue (CommitPlan::IsStale (1024, 500, 512, 500),
            L"a smaller file is a different file too");
    }

    //  Every step, so the rule is swept rather than sampled. The enum is TOTAL
    //  over the decision -- there is no step the rule declines to answer for --
    //  which is what makes a sweep of the enum legal here and stronger than a
    //  table of the cases somebody happened to think of.
    static bool IsAfterTheTemporaryCouldExist (CommitPlan::Step step)
    {
        return step == CommitPlan::Step::WriteTemporary
            || step == CommitPlan::Step::Replace;
    }

    TEST_METHOD (Temporary_IsRemovedOnEveryFailure_AndOnlyASuccessfulReplaceExcusesIt)
    {
        const CommitPlan::Step  steps[] =
        {
            CommitPlan::Step::None,
            CommitPlan::Step::Probe,
            CommitPlan::Step::Reverify,
            CommitPlan::Step::WriteTemporary,
            CommitPlan::Step::Replace,
        };

        CommitPlan::Progress  progress;
        size_t                i = 0;

        Assert::AreEqual (size_t (5), sizeof (steps) / sizeof (steps[0]),
            L"a step was added to the plan without being decided about here");

        for (i = 0; i < sizeof (steps) / sizeof (steps[0]); i++)
        {
            progress.furthestAttempted = steps[i];
            progress.replaceSucceeded  = false;

            Assert::AreEqual (IsAfterTheTemporaryCouldExist (steps[i]),
                              CommitPlan::ShouldRemoveTemporary (progress),
                L"a temporary is removed exactly when one could be sitting there");
        }
    }

    TEST_METHOD (Temporary_IsRemovedWhenTheWriteITSELFFailed)
    {
        // THIS IS THE CASE THAT GETS WRITTEN WRONG. Having just been told the
        // write did not happen, the obvious conclusion is that there is nothing
        // to remove -- but a write that failed halfway created the file first
        // and put some of the bytes in it. The plan records the furthest step
        // ATTEMPTED for exactly this reason.
        CommitPlan::Progress  progress;

        progress.furthestAttempted = CommitPlan::Step::WriteTemporary;
        progress.replaceSucceeded  = false;

        Assert::IsTrue (CommitPlan::ShouldRemoveTemporary (progress));
    }

    TEST_METHOD (Temporary_IsNotRemovedWhenTheReplaceConsumedIt)
    {
        // The single outcome where nothing is left to remove: the temporary
        // became the target. Removing here would delete the image.
        CommitPlan::Progress  progress;

        progress.furthestAttempted = CommitPlan::Step::Replace;
        progress.replaceSucceeded  = true;

        Assert::IsFalse (CommitPlan::ShouldRemoveTemporary (progress));
    }

    TEST_METHOD (Temporary_IsRemovedWhenTheReplaceFailed)
    {
        CommitPlan::Progress  progress;

        progress.furthestAttempted = CommitPlan::Step::Replace;
        progress.replaceSucceeded  = false;

        Assert::IsTrue (CommitPlan::ShouldRemoveTemporary (progress));
    }

    TEST_METHOD (Temporary_IsNotRemovedWhenNoneWasEverNamed)
    {
        // A refusal before the name was chosen has nothing to clean up, and a
        // removal driven off an empty path would ask the platform to delete
        // whatever the empty string resolves to.
        CommitPlan::Progress  progress;

        Assert::IsTrue (progress.furthestAttempted == CommitPlan::Step::None,
            L"a fresh plan has attempted nothing");

        Assert::IsFalse (CommitPlan::ShouldRemoveTemporary (progress));
    }
};
