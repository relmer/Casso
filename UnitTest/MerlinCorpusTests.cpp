#include "Pch.h"

#include "MerlinCorpus/CorpusHarness.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinCorpusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CorpusHarnessTests
    //
    //  The comparison, tested against synthetic data before the corpus has real
    //  entries to hide a bug behind.
    //
    //  Four guards protect the corpus INPUTS -- non-empty, fresh, discriminating,
    //  provenance-correct -- and every one of them sits upstream of this. If the
    //  comparison is broken, each entry passes no matter how carefully it was
    //  captured: success reported, nothing checked, in the component least likely
    //  to be suspected precisely because it IS the check.
    //
    //  These are cheap now and impossible later. Once the corpus is populated a
    //  broken harness is invisible, because everything is green and everything
    //  looks right.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CorpusHarnessTests)
    {
    public:

        TEST_METHOD (IdenticalBytes_Match)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::Match);
            Assert::IsFalse (result.hasFirstDifference);
        }



        //  THE test. A length-only comparison passes this, and a same-length
        //  divergence in the middle is the likeliest real harness bug -- the
        //  bytes are the right shape and the wrong content.
        TEST_METHOD (SameLengthDifferentMiddle_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0xAD, 0x00, 0x04 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::ByteMismatch,
                            L"a same-length content difference must not pass");
            Assert::IsTrue (result.hasFirstDifference);
            Assert::AreEqual ((size_t) 2, result.firstDifference,
                              L"the offset of the first difference must be reported, not just that one exists");
        }



        //  A difference in the LAST byte catches a comparison that stops early.
        TEST_METHOD (SameLengthDifferentFinalByte_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00, 0x04 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D, 0x00, 0x05 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::ByteMismatch,
                            L"a comparison that stops short would miss this");
            Assert::AreEqual ((size_t) 4, result.firstDifference);
        }



        TEST_METHOD (ActualShorterThanExpected_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D };
            std::vector<Byte>  actual   = { 0xA9, 0x41 };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch);
            Assert::AreEqual ((size_t) 3, result.expectedLength);
            Assert::AreEqual ((size_t) 2, result.actualLength);
        }



        //  The other direction, because a comparison that walks only the
        //  expected bytes reports a match when the actual output is longer.
        TEST_METHOD (ActualLongerThanExpected_IsReported)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41 };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0x8D };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch,
                            L"extra trailing output is a mismatch, not a match");
            Assert::AreEqual ((size_t) 2, result.expectedLength);
            Assert::AreEqual ((size_t) 3, result.actualLength);
        }



        //  A length mismatch must still say WHERE content diverged, so a
        //  truncation is distinguishable from output that went wrong and then
        //  also ended early.
        TEST_METHOD (LengthMismatchWithDifferingContent_ReportsTheContentOffset)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D, 0x00 };
            std::vector<Byte>  actual   = { 0xA9, 0xFF };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::LengthMismatch);
            Assert::AreEqual ((size_t) 1, result.firstDifference,
                              L"content diverged at 1, before the length did");
        }



        //  T020a's guard, exercised THROUGH the harness rather than asserted
        //  about it. Nothing to compare against is indistinguishable from a
        //  passing comparison unless it is called out.
        TEST_METHOD (EmptyExpectation_IsAnErrorNotAMatch)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual = { 0xA9, 0x41 };
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::EmptyExpectation,
                            L"an empty expectation must be an error rather than a trivially satisfied comparison");
        }



        //  Both empty is the worst case: a naive comparison calls it a match and
        //  the entry claims to have verified something.
        TEST_METHOD (BothEmpty_IsStillAnError)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual;
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);

            Assert::IsTrue (result.verdict == CorpusVerdict::EmptyExpectation,
                            L"two empty vectors comparing equal is exactly the vacuous pass this guards against");
        }



        //  A failing entry has to say where it went wrong, or a corpus failure is
        //  a bare assertion with a byte count and nowhere to start looking.
        TEST_METHOD (Describe_NamesTheOffsetAndTheEntry)
        {
            std::vector<Byte>  expected = { 0xA9, 0x41, 0x8D };
            std::vector<Byte>  actual   = { 0xA9, 0x41, 0xAD };
            CorpusComparison   result   = CorpusHarness::Compare (expected, actual);
            std::string        text     = CorpusHarness::Describe ("hexdata", result);

            Assert::IsTrue (text.find ("hexdata") != std::string::npos, L"the entry must be named");
            Assert::IsTrue (text.find ("offset 2") != std::string::npos, L"the offset must be reported");
        }



        TEST_METHOD (Describe_CallsAnEmptyExpectationAnError)
        {
            std::vector<Byte>  expected;
            std::vector<Byte>  actual = { 0xA9 };
            CorpusComparison   result = CorpusHarness::Compare (expected, actual);
            std::string        text   = CorpusHarness::Describe ("hollow", result);

            Assert::IsTrue (text.find ("EMPTY EXPECTATION") != std::string::npos,
                            L"the message must not read like an ordinary mismatch");
        }
    };
}
