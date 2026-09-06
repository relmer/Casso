#include "Pch.h"

#include "Capture/CaptureOutcome.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureOutcomeTests
//
//  Every reachable outcome gets the right sentence.
//
//  This is here because the alternative -- conditionals at the notice call
//  site in the shell -- is reporting logic no test can reach, and reporting
//  logic is exactly where a wrong message survives for releases: it only shows
//  up when something has already gone wrong, which is when nobody is looking
//  closely at the wording.
//
////////////////////////////////////////////////////////////////////////////////

namespace CaptureOutcomeTests
{
    static CaptureOutcome Succeeded()
    {
        CaptureOutcome   outcome;
        outcome.clipboardOk    = true;
        outcome.fileWritten    = true;
        outcome.writeAttempted = true;
        outcome.path           = L"C:\\Pictures\\Casso Screenshots\\Casso 2026-09-05 143207.png";
        return outcome;
    }




    TEST_CLASS (CaptureOutcomeTests)
    {
    public:

        TEST_METHOD (BothSinksSucceededNamesTheFile)
        {
            wstring   text = CaptureOutcome::DescribeResult (Succeeded());

            Assert::IsTrue (text.find (L"Casso 2026-09-05 143207.png") != wstring::npos);
        }


        // The bare filename, never the full path: a notice sits across the
        // picture, and a path would carry the user's account name into any
        // screenshot of the notice itself.
        TEST_METHOD (TheNoticeNamesTheFileNotThePath)
        {
            wstring   text = CaptureOutcome::DescribeResult (Succeeded());

            Assert::IsTrue (text.find (L"C:\\") == wstring::npos);
            Assert::IsTrue (text.find (L"Pictures") == wstring::npos);
        }


        TEST_METHOD (ClipboardOnlyByPreferenceSaysSoWithoutClaimingAFailure)
        {
            CaptureOutcome   outcome;
            outcome.clipboardOk    = true;
            outcome.writeAttempted = false;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"clipboard") != wstring::npos);
            Assert::IsTrue (text.find (L"could not") == wstring::npos,
                L"Saving was switched off -- nothing failed");
            Assert::IsTrue (text.find (L"fail") == wstring::npos);
        }


        // Distinct from the case above, and the reason writeAttempted exists:
        // both leave fileWritten false, but only one of them is a failure.
        TEST_METHOD (ClipboardOnlyAfterAFailedWriteReportsTheFailure)
        {
            CaptureOutcome   outcome;
            outcome.clipboardOk    = true;
            outcome.writeAttempted = true;
            outcome.fileWritten    = false;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"could not be saved") != wstring::npos);
        }


        TEST_METHOD (FileOnlyReportsTheClipboardFailureAndStillNamesTheFile)
        {
            CaptureOutcome   outcome = Succeeded();
            outcome.clipboardOk = false;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"Casso 2026-09-05 143207.png") != wstring::npos);
            Assert::IsTrue (text.find (L"clipboard") != wstring::npos);
        }


        TEST_METHOD (NeitherSinkWorkedSaysSoPlainly)
        {
            CaptureOutcome   outcome;
            outcome.clipboardOk    = false;
            outcome.fileWritten    = false;
            outcome.writeAttempted = true;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"failed") != wstring::npos);
        }


        // A refusal is reported alone. Nothing was attempted, so naming the
        // clipboard or the file would only invite the reader to wonder which
        // of them broke.
        TEST_METHOD (ARefusalIsReportedOnItsOwn)
        {
            CaptureOutcome   outcome;
            outcome.refusal = CaptureRefusal::NothingRendered;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"minimized") != wstring::npos);
            Assert::IsTrue (text.find (L"clipboard") == wstring::npos);
        }


        // A refusal wins even if a sink somehow reports success, so a stale
        // flag cannot turn "nothing happened" into "saved".
        TEST_METHOD (ARefusalOutranksTheSinkFlags)
        {
            CaptureOutcome   outcome = Succeeded();
            outcome.refusal = CaptureRefusal::NothingRendered;

            wstring   text = CaptureOutcome::DescribeResult (outcome);

            Assert::IsTrue (text.find (L"minimized") != wstring::npos);
            Assert::IsTrue (text.find (L".png") == wstring::npos);
        }


        TEST_METHOD (EveryOutcomeSaysSomething)
        {
            CaptureOutcome   outcomes[6] = {};
            size_t           i           = 0;

            outcomes[0] = Succeeded();
            outcomes[1].clipboardOk = true;
            outcomes[2].clipboardOk = true;  outcomes[2].writeAttempted = true;
            outcomes[3] = Succeeded();       outcomes[3].clipboardOk    = false;
            outcomes[4].writeAttempted = true;
            outcomes[5].refusal = CaptureRefusal::NothingRendered;

            for (i = 0; i < std::size (outcomes); i++)
            {
                Assert::IsFalse (CaptureOutcome::DescribeResult (outcomes[i]).empty());
            }
        }
    };
}
