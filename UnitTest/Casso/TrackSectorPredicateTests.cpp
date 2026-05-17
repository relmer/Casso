#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "TrackSectorPredicate.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorPredicateTests
//
//  Exercises the FR-014a / FR-014b filter-expression parser and the
//  FR-014e rejected-span recording used by the dialog's squiggle
//  layer. Pure data; no Win32 dependencies, no fixtures, no I/O.
//
////////////////////////////////////////////////////////////////////////////////

namespace TrackSectorPredicateTests
{
    TEST_CLASS (TrackSectorPredicateTests)
    {
    public:

        TEST_METHOD (EmptyExpression_matchesEverything_noRejectedSpans)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"", false);

            Assert::IsTrue  (p.IsMatchAll ());
            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (12345));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (SingleValue_matchesOnlyThatValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17", false);

            Assert::IsFalse (p.IsMatchAll ());
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (16));
            Assert::IsFalse (p.Matches (18));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (DecimalQuarterTrack_matchesQuarterTrackValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17.5", false);

            // 17 * 4 + 2 == 70.
            Assert::IsTrue  (p.MatchesQuarterTrack (70));
            Assert::IsFalse (p.MatchesQuarterTrack (69));
            Assert::IsFalse (p.MatchesQuarterTrack (71));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (Range_matchesEveryValueInside)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2", false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (-1));
        }


        TEST_METHOD (CommaSeparatedList_matchesEachListedValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0,17,34", false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (34));
            Assert::IsFalse (p.Matches (1));
            Assert::IsFalse (p.Matches (33));
        }


        TEST_METHOD (MixedRangeAndValues_unionMatches)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2,17,30-34", false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (30));
            Assert::IsTrue  (p.Matches (32));
            Assert::IsTrue  (p.Matches (34));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (16));
            Assert::IsFalse (p.Matches (35));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (RawQtFlag_interpretsBareIntegersAsQuarterTracks)
        {
            TrackSectorPredicate  pRaw    = TrackSectorPredicate::Parse (L"68", true);
            TrackSectorPredicate  pWhole  = TrackSectorPredicate::Parse (L"68", false);

            // rawQt: bare 68 is quarter-track 68 (== whole-track 17).
            Assert::IsTrue  (pRaw.MatchesQuarterTrack (68));
            Assert::IsFalse (pRaw.MatchesQuarterTrack (67));

            // Whole-track mode: 68 is whole-track 68; the parser does NOT
            // reject it even though no standard Disk II disk reaches that
            // track (FR-014a "no max-value validation" rule).
            Assert::IsTrue  (pWhole.Matches (68));
        }


        TEST_METHOD (MalformedToken_isSilentlyDroppedFromMatching)
        {
            TrackSectorPredicate  pOnly = TrackSectorPredicate::Parse (L"abc", false);
            TrackSectorPredicate  pMix  = TrackSectorPredicate::Parse (L"1, abc, 3", false);

            // Per the explicit T027 clarification: an expression with
            // tokens but in which every token was rejected does NOT
            // match everything. Only literally empty input does.
            Assert::IsFalse (pOnly.IsMatchAll ());
            Assert::IsFalse (pOnly.Matches (0));
            Assert::IsFalse (pOnly.Matches (42));

            // Valid tokens around a malformed one survive intact.
            Assert::IsTrue  (pMix.Matches (1));
            Assert::IsTrue  (pMix.Matches (3));
            Assert::IsFalse (pMix.Matches (2));
        }


        TEST_METHOD (Whitespace_toleratedAroundTokensAndSeparators)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"  0 - 2 , 17  ", false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (16));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (LargeValue_parsesWithoutMaxValidation)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"999", false);

            // FR-014a: NO max-value rejection. 999 parses, simply
            // fails to match any realistic Disk II track / sector.
            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
            Assert::IsTrue   (p.Matches (999));
            Assert::IsFalse  (p.Matches (998));
        }


        TEST_METHOD (RejectedSpan_recordsExactBeginEndOfMalformedToken)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, abc, 17", false);

            // "0-2, abc, 17"
            //  0123456789012
            //       abc is [5, 8).
            Assert::AreEqual ((size_t) 1, p.RejectedSpans ().size ());
            Assert::AreEqual (5, p.RejectedSpans ()[0].beginUtf16);
            Assert::AreEqual (8, p.RejectedSpans ()[0].endUtf16);
        }


        TEST_METHOD (RejectedSpan_recordsMultipleMalformedTokensIndividually)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"xx, 5, yy", false);

            // "xx, 5, yy"
            //  012345678
            // xx is [0, 2); yy is [7, 9).
            Assert::AreEqual ((size_t) 2, p.RejectedSpans ().size ());
            Assert::AreEqual (0, p.RejectedSpans ()[0].beginUtf16);
            Assert::AreEqual (2, p.RejectedSpans ()[0].endUtf16);
            Assert::AreEqual (7, p.RejectedSpans ()[1].beginUtf16);
            Assert::AreEqual (9, p.RejectedSpans ()[1].endUtf16);
            Assert::IsTrue   (p.Matches (5));
        }


        TEST_METHOD (FullyValidExpression_yieldsEmptyRejectedSpanList)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, 17, 30-34", false);

            Assert::AreEqual ((size_t) 0, p.RejectedSpans ().size ());
        }


        TEST_METHOD (AllJunkExpression_recordsAllSpansAndMatchesNothing)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"abc, def", false);

            // "abc, def"
            //  01234567
            // abc is [0, 3); def is [5, 8).
            Assert::AreEqual ((size_t) 2, p.RejectedSpans ().size ());
            Assert::AreEqual (0, p.RejectedSpans ()[0].beginUtf16);
            Assert::AreEqual (3, p.RejectedSpans ()[0].endUtf16);
            Assert::AreEqual (5, p.RejectedSpans ()[1].beginUtf16);
            Assert::AreEqual (8, p.RejectedSpans ()[1].endUtf16);

            // Critical: rejected-only != empty input. Matches nothing.
            Assert::IsFalse (p.IsMatchAll ());
            Assert::IsFalse (p.Matches (0));
            Assert::IsFalse (p.Matches (100));
        }
    };
}
