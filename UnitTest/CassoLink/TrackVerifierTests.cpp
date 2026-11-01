#include "Pch.h"

#include "CppUnitTest.h"
#include "../EhmTestHelper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/Session/TrackVerifier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifierTests
//
//  The vote over a scripted drive: each sector's reads, in order, as a byte to
//  fill its data with, or -1 for a read that fails.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackVerifierTests)
{
public:

    struct Drive : ITrackVerifierIo
    {
        std::array<std::vector<int>, 16>  script;
        std::array<size_t, 16>            nexts   = {};
        int                               moves   = 0;
        int                               dataReads = 0;

        int  Next (Byte sector)
        {
            std::vector<int>  & reads = script[sector];
            size_t              at    = std::min (nexts[sector]++, reads.size() - 1);

            return reads[at];
        }

        HRESULT  ReadData (std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead) override
        {
            outData.assign (sectors.size() * 256, 0);
            outRead.assign (sectors.size(), false);
            dataReads++;

            for (size_t i = 0; i < sectors.size(); i++)
            {
                int  value = Next (sectors[i]);

                if (value >= 0)
                {
                    std::fill_n (outData.begin() + (ptrdiff_t) (i * 256), 256, (Byte) value);
                    outRead[i] = true;
                }
            }

            return S_OK;
        }

        HRESULT  ReadCrcs (std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead) override
        {
            outCrcs.assign (sectors.size(), 0);
            outRead.assign (sectors.size(), false);

            for (size_t i = 0; i < sectors.size(); i++)
            {
                int  value = Next (sectors[i]);

                if (value >= 0)
                {
                    std::vector<Byte>  data (256, (Byte) value);

                    outCrcs[i] = CassoLinkCodec::ComputeCrc (data);
                    outRead[i] = true;
                }
            }

            return S_OK;
        }

        HRESULT  MoveHead() override
        {
            moves++;
            return S_OK;
        }
    };

    static Drive  AllRead (int value = 7)
    {
        Drive  drive;

        for (std::vector<int> & reads : drive.script)
        {
            reads = { value };
        }

        return drive;
    }

    TEST_METHOD (AGoodTrack_IsReadThreeTimes_AndClean)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        for (TrackVerifier::Outcome outcome : result.outcomes)
        {
            Assert::IsTrue (outcome == TrackVerifier::Outcome::Clean);
        }

        Assert::AreEqual ((size_t) 3, drive.nexts[0]);
        Assert::AreEqual (1, drive.dataReads, L"only the first read carries data");
        Assert::AreEqual (0, drive.moves);
        Assert::AreEqual<int> (7, result.data[15 * 256]);
    }

    TEST_METHOD (ReadsThatDisagree_AreRepeated_AndTheMajorityKept)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { 1, 2, 2, 2, 1, 2, 2 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.outcomes[4] == TrackVerifier::Outcome::Recovered);
        Assert::AreEqual<int> (2, result.data[4 * 256], L"the majority, not the first read");
        Assert::IsTrue   (drive.moves > 0, L"the head moved before the retries");
    }

    TEST_METHOD (NoMajority_IsUnreliable_AfterNineReads)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.outcomes[4] == TrackVerifier::Outcome::Unreliable);
        Assert::AreEqual ((size_t) 9, drive.nexts[4]);
    }

    TEST_METHOD (ASectorWithErrors_IsAccepted_AtTwoMatchingGoodReads)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { -1, 3, -1, 3 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.outcomes[4] == TrackVerifier::Outcome::Recovered);
        Assert::AreEqual<int> (3, result.data[4 * 256]);
    }

    TEST_METHOD (OneGoodRead_IsKept_AndUnreliable)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { 3, -1 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.outcomes[4] == TrackVerifier::Outcome::Unreliable);
        Assert::AreEqual<int> (3, result.data[4 * 256]);
        Assert::AreEqual ((size_t) 9, drive.nexts[4], L"every try was spent looking for a second");
    }

    TEST_METHOD (NoGoodRead_IsUnreadable_AndZeros)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { -1 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.outcomes[4] == TrackVerifier::Outcome::Unreadable);
        Assert::AreEqual<int> (0, result.data[4 * 256]);
        Assert::IsFalse  (result.noSectors);
    }

    //  A nonstandard track, as on a copy-protected disk, doesn't take nine tries.
    TEST_METHOD (ATrackWithNothingOnIt_IsGivenUpAfterTwoReads)
    {
        Drive                  drive = AllRead (-1);
        TrackVerifier::Result  result;

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::IsTrue   (result.noSectors);
        Assert::AreEqual ((size_t) 2, drive.nexts[0]);
        Assert::IsTrue   (result.outcomes[0] == TrackVerifier::Outcome::Unreadable);
    }

    //  The majority was only ever seen as CRCs, so its data is read again.
    TEST_METHOD (AMajoritySeenOnlyAsCrcs_HasItsDataFetched)
    {
        Drive                  drive = AllRead();
        TrackVerifier::Result  result;

        drive.script[4] = { 1, 2, 2, 2, 2, 2, 2 };

        AssertSucceeded (TrackVerifier::Verify (drive, result));

        Assert::AreEqual<int> (2, result.data[4 * 256]);
        Assert::IsTrue (drive.dataReads > 1);
    }

    TEST_METHOD (AnIoFailure_Stops)
    {
        struct Failing : Drive
        {
            HRESULT  ReadCrcs (std::span<const Byte>, std::vector<Word> &, std::vector<bool> &) override { return E_FAIL; }
        };

        Failing                drive;
        TrackVerifier::Result  result;

        for (std::vector<int> & reads : drive.script)
        {
            reads = { 7 };
        }

        Assert::AreEqual (E_FAIL, TrackVerifier::Verify (drive, result));
    }
};
