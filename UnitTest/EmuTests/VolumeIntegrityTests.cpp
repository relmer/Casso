#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/ChainWalkGuard.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeIntegrityTests
//
//  The pass that answers what a volume's catalog actually references, and the
//  guard that keeps its walks from hanging on the damaged volumes it exists to
//  examine.
//
//  Every case is built from synthetic claims -- no disk, no image, no host
//  files. The report never needs to know what a file is, which is what makes
//  that possible.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (VolumeIntegrityTests)
{
public:

    static constexpr uint32_t  kUnits = 560;   // a 35-track, 16-sector volume

    //  Walks a chain described as an explicit next-pointer table, the way a
    //  corrupted on-disk chain behaves. Returns the number of steps taken.
    static int WalkChain (const vector<uint32_t> & next, uint32_t start, ChainWalkGuard & guard)
    {
        uint32_t  at    = start;
        int       steps = 0;



        while (guard.TryVisit (at))
        {
            steps++;

            if (at >= next.size())
            {
                break;
            }

            at = next[at];
        }

        return steps;
    }

    TEST_METHOD (ChainWalkGuard_SelfReferentialChain_Terminates)
    {
        // The simplest cycle, and the one a single corrupted byte produces.
        vector<uint32_t>  next (kUnits, 0);
        ChainWalkGuard    guard (kUnits);
        int               steps = 0;

        next[7] = 7;   // points at itself

        steps = WalkChain (next, 7, guard);

        Assert::AreEqual (1, steps, L"a self-reference must be visited once and then stop");
        Assert::IsTrue (guard.HitBound(), L"stopping on a cycle is a bound, not a clean finish");
        Assert::IsTrue (guard.SawCycle(), L"the cycle must be reported as such");
    }

    TEST_METHOD (ChainWalkGuard_LongCycle_Terminates)
    {
        // A cycle does not have to be tight. This one runs most of the volume
        // before closing, which is what defeats a naive step counter tuned to
        // "no file is longer than N".
        vector<uint32_t>  next (kUnits, 0);
        ChainWalkGuard    guard (kUnits);
        uint32_t          i     = 0;
        int               steps = 0;

        for (i = 0; i < kUnits - 1; i++)
        {
            next[i] = i + 1;
        }

        next[kUnits - 1] = 0;   // closes the loop

        steps = WalkChain (next, 0, guard);

        Assert::AreEqual ((int) kUnits, steps, L"every distinct unit is visited exactly once");
        Assert::IsTrue (guard.HitBound(), L"closing the loop must stop the walk");
    }

    TEST_METHOD (ChainWalkGuard_PointerOutsideTheVolume_Terminates)
    {
        // A chain that leaves the realm of things that could be true. Following
        // it further only compounds the corruption.
        vector<uint32_t>  next (kUnits, 0);
        ChainWalkGuard    guard (kUnits);
        int               steps = 0;

        next[3] = kUnits + 9000;

        steps = WalkChain (next, 3, guard);

        Assert::AreEqual (1, steps, L"the impossible pointer must not be followed");
        Assert::IsTrue (guard.HitBound(), L"leaving the volume is a bound");
        Assert::IsTrue (guard.ExceededLength(), L"an out-of-range unit is reported as over-run");
    }

    TEST_METHOD (ChainWalkGuard_HonestChain_CompletesWithoutHittingBound)
    {
        // The counterpart: a walk that reaches the end must NOT look like one
        // that gave up, or every clean file would be reported as damaged.
        vector<uint32_t>  next (kUnits, 0);
        ChainWalkGuard    guard (kUnits);
        uint32_t          i     = 0;
        int               steps = 0;

        for (i = 0; i < 9; i++)
        {
            next[i] = i + 1;
        }

        next[9] = kUnits;   // one past the end == "no more", handled by the caller

        steps = WalkChain (next, 0, guard);

        Assert::AreEqual (10, steps, L"a ten-unit chain must take ten steps");
        Assert::IsFalse (guard.SawCycle(), L"an honest chain has no cycle");
    }

    TEST_METHOD (IntegrityReport_UniqueOwnership_IsWhatDeleteMayFree)
    {
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (10, 1);
        report.AddClaim (11, 1);
        report.AddClaim (12, 2);
        report.Finish();

        Assert::IsTrue  (report.IsUniquelyOwnedBy (10, 1), L"an entry's own unit is freeable");
        Assert::IsFalse (report.IsUniquelyOwnedBy (12, 1), L"another entry's unit is not");
        Assert::IsFalse (report.IsUniquelyOwnedBy (99, 1), L"an unclaimed unit is not ours to free");
    }

    TEST_METHOD (IntegrityReport_CrossLinkedUnit_IsNeverUniquelyOwned)
    {
        // The case both naive answers get wrong. Refusing to delete strands the
        // volume; freeing what is shared destroys the other file. Reporting the
        // unit as not-uniquely-owned declines to free exactly the sector where
        // freeing would lose data, and nothing else.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (40, 1);
        report.AddClaim (40, 2);   // two files claim one unit
        report.AddClaim (41, 1);
        report.Finish();

        Assert::IsFalse (report.IsUniquelyOwnedBy (40, 1), L"a shared unit must not be freed");
        Assert::IsFalse (report.IsUniquelyOwnedBy (40, 2), L"not by either claimant");
        Assert::IsTrue  (report.IsUniquelyOwnedBy (41, 1), L"the rest of the file is unaffected");
        Assert::AreEqual (size_t (1), report.GetCrossLinked().size(), L"the cross-link is reported");
        Assert::IsFalse (report.IsClean(), L"a cross-linked volume is not clean");
    }

    TEST_METHOD (IntegrityReport_AllocatedButUnclaimed_IsReportedAndNeverReclaimed)
    {
        // Either already-leaked space or an invisible file's data, and there is
        // no way to tell which. Reporting it without reclaiming it is the
        // correct behavior, not a gap.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.SetAllocatedInFreeMap (100, true);
        report.SetAllocatedInFreeMap (101, true);
        report.AddClaim (101, 1);
        report.Finish();

        Assert::AreEqual (size_t (1), report.GetAllocatedButUnclaimed().size(),
            L"the orphan is reported");
        Assert::AreEqual (uint32_t (100), report.GetAllocatedButUnclaimed()[0]);
        Assert::IsFalse (report.IsUniquelyOwnedBy (100, 1),
            L"an orphan belongs to nobody, so nobody may free it");
    }

    TEST_METHOD (IntegrityReport_ClaimedButFree_IsTheDangerousDisagreement)
    {
        // A unit the catalog references while the free map calls it free: the
        // next allocation would hand out live data.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (55, 1);
        report.SetAllocatedInFreeMap (55, false);
        report.Finish();

        Assert::AreEqual (size_t (1), report.GetClaimedButFree().size(),
            L"the disagreement must be reported rather than allocated over");
        Assert::IsFalse (report.IsClean());
    }

    TEST_METHOD (IntegrityReport_UnfollowableChain_MakesTheVolumeUnclean)
    {
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.MarkChainUnfollowable (3);
        report.Finish();

        Assert::AreEqual (size_t (1), report.GetUnfollowableChains().size());
        Assert::IsFalse (report.IsClean(), L"a chain that could not be walked is damage");
    }

    TEST_METHOD (IntegrityReport_UnparsedCatalog_BoundsEveryOtherAnswer)
    {
        // The one case the unique-ownership rule can still lose data: a file
        // that could not be read claims nothing observable, so a unit it shares
        // looks solely owned. The report must not call that clean.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (20, 1);
        report.SetAllocatedInFreeMap (20, true);
        report.SetCatalogFullyParsed (false);
        report.Finish();

        Assert::IsFalse (report.IsCatalogFullyParsed());
        Assert::IsFalse (report.IsClean(),
            L"results derived from a catalog that did not fully parse are not trustworthy");
    }

    TEST_METHOD (IntegrityReport_ConsistentVolume_IsClean)
    {
        VolumeIntegrityReport  report;
        uint32_t               unit = 0;

        report.Reset (kUnits);

        for (unit = 0; unit < 30; unit++)
        {
            report.AddClaim (unit, (uint16_t) (unit / 10));
            report.SetAllocatedInFreeMap (unit, true);
        }

        report.Finish();

        Assert::IsTrue (report.IsClean(), L"catalog and free map agreeing is a clean volume");
    }

    TEST_METHOD (IntegrityReport_ClaimsOfAnEntry_AreRecoverableWithoutReWalking)
    {
        // The requirement asks which units each file claims, not merely how
        // many claimants a unit has. A structure that answers only the second
        // forces every caller to re-walk chains the pass already followed.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (5,  1);
        report.AddClaim (9,  1);
        report.AddClaim (14, 1);
        report.AddClaim (20, 2);
        report.Finish();

        Assert::AreEqual (size_t (3), report.GetClaimsOf (1).size(),
            L"the entry's own units must be listed");
        Assert::AreEqual (uint32_t (5),  report.GetClaimsOf (1)[0]);
        Assert::AreEqual (uint32_t (14), report.GetClaimsOf (1)[2]);
        Assert::AreEqual (size_t (1), report.GetClaimsOf (2).size());
        Assert::AreEqual (size_t (0), report.GetClaimsOf (99).size(),
            L"an entry the pass never saw claims nothing, which is not an error");
    }

    TEST_METHOD (IntegrityReport_CrossLink_NamesTheClaimantsNotJustTheCount)
    {
        // A damage report that says "two files share this sector" without
        // saying which two leaves the user unable to act on it.
        VolumeIntegrityReport  report;

        report.Reset (kUnits);
        report.AddClaim (33, 4);
        report.AddClaim (33, 7);
        report.Finish();

        Assert::AreEqual (size_t (2), report.GetClaimantsOf (33).size());
        Assert::AreEqual (uint16_t (4), report.GetClaimantsOf (33)[0]);
        Assert::AreEqual (uint16_t (7), report.GetClaimantsOf (33)[1]);
        Assert::AreEqual (size_t (0), report.GetClaimantsOf (kUnits + 1).size(),
            L"a unit outside the volume has no claimants and is not an error");
    }

    TEST_METHOD (FilePath_SingleComponent_IsWhatFlatVolumesExpress)
    {
        FilePath  path = FilePath::Parse ("HELLO");

        Assert::IsTrue (path.IsSingleComponent(), L"a bare name is one component");
        Assert::AreEqual (string ("HELLO"), path.GetLeaf());
        Assert::IsFalse (path.IsRooted());
    }

    TEST_METHOD (FilePath_MultipleComponents_AreKeptWholeNotTruncated)
    {
        // The reason paths exist from the outset: a volume that cannot yet walk
        // subdirectories must be able to REFUSE this, which it can only do if
        // it can see that more than one component was asked for.
        FilePath  path = FilePath::Parse ("/UTIL/TOOLS/DUMP");

        Assert::IsTrue (path.IsRooted());
        Assert::IsFalse (path.IsSingleComponent());
        Assert::AreEqual (size_t (3), path.GetDepth());
        Assert::AreEqual (string ("DUMP"), path.GetLeaf());
        Assert::AreEqual (string ("/UTIL/TOOLS/DUMP"), path.ToString());
    }

    TEST_METHOD (FilePath_RedundantSeparators_DoNotCreateNamelessSteps)
    {
        FilePath  path = FilePath::Parse ("//UTIL//DUMP/");

        Assert::AreEqual (size_t (2), path.GetDepth(),
            L"empty components are dropped rather than matched against nothing");
        Assert::AreEqual (string ("DUMP"), path.GetLeaf());
    }

    TEST_METHOD (FilePath_Empty_HasNoLeafAndIsNotSingleComponent)
    {
        FilePath  path = FilePath::Parse ("");

        Assert::IsTrue (path.IsEmpty());
        Assert::IsFalse (path.IsSingleComponent(), L"nothing is not one thing");
        Assert::AreEqual (string (""), path.GetLeaf());
    }
};
