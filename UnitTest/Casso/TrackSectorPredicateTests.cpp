#include "Pch.h"
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
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"", TrackSectorPredicate::Mode::Track, false);

            Assert::IsTrue  (p.IsMatchAll());
            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (12345));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (SingleValue_matchesOnlyThatValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17", TrackSectorPredicate::Mode::Track, false);

            Assert::IsFalse (p.IsMatchAll());
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (16));
            Assert::IsFalse (p.Matches (18));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (DecimalQuarterTrack_matchesQuarterTrackValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17.5", TrackSectorPredicate::Mode::Track, false);

            // 17 * 4 + 2 == 70.
            Assert::IsTrue  (p.MatchesQuarterTrack (70));
            Assert::IsFalse (p.MatchesQuarterTrack (69));
            Assert::IsFalse (p.MatchesQuarterTrack (71));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (Range_matchesEveryValueInside)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2", TrackSectorPredicate::Mode::Track, false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (-1));
        }


        TEST_METHOD (CommaSeparatedList_matchesEachListedValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0,17,34", TrackSectorPredicate::Mode::Track, false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (34));
            Assert::IsFalse (p.Matches (1));
            Assert::IsFalse (p.Matches (33));
        }


        TEST_METHOD (MixedRangeAndValues_unionMatches)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2,17,30-34", TrackSectorPredicate::Mode::Track, false);

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
            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (RawQtFlag_interpretsBareIntegersAsQuarterTracks)
        {
            TrackSectorPredicate  pRaw    = TrackSectorPredicate::Parse (L"68", TrackSectorPredicate::Mode::Track, true);

            // rawQt: bare 68 is quarter-track 68 (== whole-track 17).
            // Spec-006 bug 3: kMaxQuarterTrackExclusive == 160, so 68
            // is still in range and accepted.
            Assert::IsTrue  (pRaw.MatchesQuarterTrack (68));
            Assert::IsFalse (pRaw.MatchesQuarterTrack (67));
            Assert::AreEqual ((size_t) 0, pRaw.RejectedSpans().size());
        }


        TEST_METHOD (MalformedToken_isSilentlyDroppedFromMatching)
        {
            TrackSectorPredicate  pOnly = TrackSectorPredicate::Parse (L"abc", TrackSectorPredicate::Mode::Track, false);
            TrackSectorPredicate  pMix  = TrackSectorPredicate::Parse (L"1, abc, 3", TrackSectorPredicate::Mode::Track, false);

            // Per the explicit T027 clarification: an expression with
            // tokens but in which every token was rejected does NOT
            // match everything. Only literally empty input does.
            Assert::IsFalse (pOnly.IsMatchAll());
            Assert::IsFalse (pOnly.Matches (0));
            Assert::IsFalse (pOnly.Matches (42));

            // Valid tokens around a malformed one survive intact.
            Assert::IsTrue  (pMix.Matches (1));
            Assert::IsTrue  (pMix.Matches (3));
            Assert::IsFalse (pMix.Matches (2));
        }


        TEST_METHOD (Whitespace_toleratedAroundTokensAndSeparators)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"  0 - 2 , 17  ", TrackSectorPredicate::Mode::Track, false);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (16));
            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (LargeValue_outsideTrackRange_isRejectedWithSquiggle)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"999", TrackSectorPredicate::Mode::Track, false);

            // Spec-006 bug 3: kMaxWholeTrackExclusive == 40, so any
            // whole-track value >= 40 is now rejected and recorded
            // as a RejectedSpan so the dialog can squiggle it.
            Assert::AreEqual ((size_t) 1, p.RejectedSpans().size());
            Assert::IsFalse  (p.IsMatchAll());
            Assert::IsFalse  (p.Matches (999));
        }


        TEST_METHOD (TrackBoundary_track39Accepted_track40Rejected)
        {
            // Spec-006 bug 3: tracks 0..39 are accepted (full
            // quarter-track expansion); 40 and up rejected.
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"39", TrackSectorPredicate::Mode::Track, false);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"40", TrackSectorPredicate::Mode::Track, false);

            Assert::AreEqual ((size_t) 0, ok.RejectedSpans().size());
            Assert::IsTrue   (ok.Matches (39));

            Assert::AreEqual ((size_t) 1, bad.RejectedSpans().size());
            Assert::IsFalse  (bad.Matches (40));
        }


        TEST_METHOD (QuarterTrackBoundary_3975Accepted_4000Rejected)
        {
            // 39.75 = quarter-track 159 (< 160) -> accepted.
            // 40.0  = quarter-track 160         -> rejected.
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"39.75", TrackSectorPredicate::Mode::Track, false);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"40.0",  TrackSectorPredicate::Mode::Track, false);

            Assert::AreEqual ((size_t) 0, ok.RejectedSpans().size());
            Assert::IsTrue   (ok.MatchesQuarterTrack (159));

            Assert::AreEqual ((size_t) 1, bad.RejectedSpans().size());
            Assert::IsFalse  (bad.MatchesQuarterTrack (160));
        }


        TEST_METHOD (SectorBoundary_sector15Accepted_sector16Rejected)
        {
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"15", TrackSectorPredicate::Mode::Sector);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"16", TrackSectorPredicate::Mode::Sector);

            Assert::AreEqual ((size_t) 0, ok.RejectedSpans().size());
            Assert::IsTrue   (ok.Matches (15));

            Assert::AreEqual ((size_t) 1, bad.RejectedSpans().size());
            Assert::IsFalse  (bad.Matches (16));
        }


        TEST_METHOD (RangeWithOutOfRangeHi_clampsToCapAndSquigglesToken)
        {
            // "0-77" track -> clamp hi to 39, accept the partial
            // range, AND squiggle the whole token so the user sees
            // the explicit reject.
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-77", TrackSectorPredicate::Mode::Track, false);

            Assert::AreEqual ((size_t) 1, p.RejectedSpans().size());
            Assert::IsTrue   (p.Matches (0));
            Assert::IsTrue   (p.Matches (20));
            Assert::IsTrue   (p.Matches (39));
            Assert::IsFalse  (p.Matches (40));
            Assert::IsFalse  (p.Matches (77));
        }


        TEST_METHOD (RejectedSpan_recordsExactBeginEndOfMalformedToken)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, abc, 17", TrackSectorPredicate::Mode::Track, false);

            // "0-2, abc, 17"
            //  0123456789012
            //       abc is [5, 8).
            Assert::AreEqual ((size_t) 1, p.RejectedSpans().size());
            Assert::AreEqual (5, p.RejectedSpans()[0].beginUtf16);
            Assert::AreEqual (8, p.RejectedSpans()[0].endUtf16);
        }


        TEST_METHOD (RejectedSpan_recordsMultipleMalformedTokensIndividually)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"xx, 5, yy", TrackSectorPredicate::Mode::Track, false);

            // "xx, 5, yy"
            //  012345678
            // xx is [0, 2); yy is [7, 9).
            Assert::AreEqual ((size_t) 2, p.RejectedSpans().size());
            Assert::AreEqual (0, p.RejectedSpans()[0].beginUtf16);
            Assert::AreEqual (2, p.RejectedSpans()[0].endUtf16);
            Assert::AreEqual (7, p.RejectedSpans()[1].beginUtf16);
            Assert::AreEqual (9, p.RejectedSpans()[1].endUtf16);
            Assert::IsTrue   (p.Matches (5));
        }


        TEST_METHOD (FullyValidExpression_yieldsEmptyRejectedSpanList)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, 17, 30-34", TrackSectorPredicate::Mode::Track, false);

            Assert::AreEqual ((size_t) 0, p.RejectedSpans().size());
        }


        TEST_METHOD (AllJunkExpression_recordsAllSpansAndMatchesNothing)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"abc, def", TrackSectorPredicate::Mode::Track, false);

            // "abc, def"
            //  01234567
            // abc is [0, 3); def is [5, 8).
            Assert::AreEqual ((size_t) 2, p.RejectedSpans().size());
            Assert::AreEqual (0, p.RejectedSpans()[0].beginUtf16);
            Assert::AreEqual (3, p.RejectedSpans()[0].endUtf16);
            Assert::AreEqual (5, p.RejectedSpans()[1].beginUtf16);
            Assert::AreEqual (8, p.RejectedSpans()[1].endUtf16);

            // Critical: rejected-only != empty input. Matches nothing.
            Assert::IsFalse (p.IsMatchAll());
            Assert::IsFalse (p.Matches (0));
            Assert::IsFalse (p.Matches (100));
        }
    };
}
