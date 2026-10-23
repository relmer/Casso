#include "Pch.h"
#include "Ui/Debugger/TrackSectorPredicate.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorPredicateTests
//
//  The Disk II event log's track and sector filter parser, and the spans of
//  the tokens not taken. Pure data; no Win32 dependencies, no fixtures, no
//  I/O.
//
////////////////////////////////////////////////////////////////////////////////

namespace TrackSectorPredicateTests
{
    TEST_CLASS (TrackSectorPredicateTests)
    {
    public:

        TEST_METHOD (EmptyExpression_matchesEverything_noRejectedSpans)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"", TrackSectorPredicate::Mode::Track);

            Assert::IsTrue  (p.IsMatchAll());
            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (12345));
            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        //  A number too long for any track fails to parse instead of wrapping
        //  to a small one, and a sector has no quarter-tracks: each is listed
        //  as a bad span.
        TEST_METHOD (AHugeNumberAndASectorQuarterTrackAreBadSpans)
        {
            TrackSectorPredicate  track  = TrackSectorPredicate::Parse (L"4294967298", TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  sector = TrackSectorPredicate::Parse (L"1.5",        TrackSectorPredicate::Mode::Sector);



            Assert::IsFalse  (track.Matches (2), L"not wrapped to track 2");
            Assert::AreEqual ((size_t) 1, track.GetRejectedSpans().size());
            Assert::AreEqual ((size_t) 1, sector.GetRejectedSpans().size(), L"a sector has no quarter");
        }


        TEST_METHOD (SingleValue_matchesOnlyThatValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17", TrackSectorPredicate::Mode::Track);

            Assert::IsFalse (p.IsMatchAll());
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (16));
            Assert::IsFalse (p.Matches (18));
            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        TEST_METHOD (DecimalQuarterTrack_matchesQuarterTrackValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"17.5", TrackSectorPredicate::Mode::Track);

            // 17 * 4 + 2 == 70.
            Assert::IsTrue  (p.MatchesQuarterTrack (70));
            Assert::IsFalse (p.MatchesQuarterTrack (69));
            Assert::IsFalse (p.MatchesQuarterTrack (71));
            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        TEST_METHOD (Range_matchesEveryValueInside)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2", TrackSectorPredicate::Mode::Track);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (-1));
        }


        TEST_METHOD (CommaSeparatedList_matchesEachListedValue)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0,17,34", TrackSectorPredicate::Mode::Track);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsTrue  (p.Matches (34));
            Assert::IsFalse (p.Matches (1));
            Assert::IsFalse (p.Matches (33));
        }


        TEST_METHOD (MixedRangeAndValues_unionMatches)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2,17,30-34", TrackSectorPredicate::Mode::Track);

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
            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        TEST_METHOD (AWholeTrackBesideAQuarterTrackBecomesItsQuarterTracks)
        {
            TrackSectorPredicate  pUp   = TrackSectorPredicate::Parse (L"17-18.25", TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  pDown = TrackSectorPredicate::Parse (L"17.5-18",  TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  pBare = TrackSectorPredicate::Parse (L"68",       TrackSectorPredicate::Mode::Track);

            // 17-18.25 starts at 17.0 (qt 68); 17.5-18 ends at 18.75 (qt 75).
            Assert::IsTrue  (pUp.MatchesQuarterTrack   (68));
            Assert::IsTrue  (pUp.MatchesQuarterTrack   (73));
            Assert::IsFalse (pUp.MatchesQuarterTrack   (74));
            Assert::IsFalse (pDown.MatchesQuarterTrack (69));
            Assert::IsTrue  (pDown.MatchesQuarterTrack (70));
            Assert::IsTrue  (pDown.MatchesQuarterTrack (75));
            Assert::IsFalse (pDown.MatchesQuarterTrack (76));
            Assert::AreEqual ((size_t) 0, pUp.GetRejectedSpans().size());

            // A bare number is a whole track, and 68 is past track 39.
            Assert::AreEqual ((size_t) 1, pBare.GetRejectedSpans().size());
        }


        TEST_METHOD (MalformedToken_isSilentlyDroppedFromMatching)
        {
            TrackSectorPredicate  pOnly = TrackSectorPredicate::Parse (L"abc", TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  pMix  = TrackSectorPredicate::Parse (L"1, abc, 3", TrackSectorPredicate::Mode::Track);

            // An expression with tokens, none of them taken, matches
            // nothing. Only literally empty input matches everything.
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
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"  0 - 2 , 17  ", TrackSectorPredicate::Mode::Track);

            Assert::IsTrue  (p.Matches (0));
            Assert::IsTrue  (p.Matches (1));
            Assert::IsTrue  (p.Matches (2));
            Assert::IsTrue  (p.Matches (17));
            Assert::IsFalse (p.Matches (3));
            Assert::IsFalse (p.Matches (16));
            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        TEST_METHOD (LargeValue_outsideTrackRange_isRejected)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"999", TrackSectorPredicate::Mode::Track);

            // kMaxWholeTrackExclusive == 40, so any whole-track value
            // >= 40 is left out and its span listed.
            Assert::AreEqual ((size_t) 1, p.GetRejectedSpans().size());
            Assert::IsFalse  (p.IsMatchAll());
            Assert::IsFalse  (p.Matches (999));
        }


        TEST_METHOD (TrackBoundary_track39Accepted_track40Rejected)
        {
            // Tracks 0..39 are accepted (the head's full travel); 40
            // and up are left out.
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"39", TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"40", TrackSectorPredicate::Mode::Track);

            Assert::AreEqual ((size_t) 0, ok.GetRejectedSpans().size());
            Assert::IsTrue   (ok.Matches (39));

            Assert::AreEqual ((size_t) 1, bad.GetRejectedSpans().size());
            Assert::IsFalse  (bad.Matches (40));
        }


        TEST_METHOD (QuarterTrackBoundary_3975Accepted_4000Rejected)
        {
            // 39.75 = quarter-track 159 (< 160) -> accepted.
            // 40.0  = quarter-track 160         -> left out.
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"39.75", TrackSectorPredicate::Mode::Track);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"40.0",  TrackSectorPredicate::Mode::Track);

            Assert::AreEqual ((size_t) 0, ok.GetRejectedSpans().size());
            Assert::IsTrue   (ok.MatchesQuarterTrack (159));

            Assert::AreEqual ((size_t) 1, bad.GetRejectedSpans().size());
            Assert::IsFalse  (bad.MatchesQuarterTrack (160));
        }


        TEST_METHOD (SectorBoundary_sector15Accepted_sector16Rejected)
        {
            TrackSectorPredicate  ok  = TrackSectorPredicate::Parse (L"15", TrackSectorPredicate::Mode::Sector);
            TrackSectorPredicate  bad = TrackSectorPredicate::Parse (L"16", TrackSectorPredicate::Mode::Sector);

            Assert::AreEqual ((size_t) 0, ok.GetRejectedSpans().size());
            Assert::IsTrue   (ok.Matches (15));

            Assert::AreEqual ((size_t) 1, bad.GetRejectedSpans().size());
            Assert::IsFalse  (bad.Matches (16));
        }


        TEST_METHOD (RangeWithOutOfRangeHi_clampsToCapAndRejectsToken)
        {
            // "0-77" track -> clamp hi to 39, accept the partial
            // range, and list the whole token as not taken.
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-77", TrackSectorPredicate::Mode::Track);

            Assert::AreEqual ((size_t) 1, p.GetRejectedSpans().size());
            Assert::IsTrue   (p.Matches (0));
            Assert::IsTrue   (p.Matches (20));
            Assert::IsTrue   (p.Matches (39));
            Assert::IsFalse  (p.Matches (40));
            Assert::IsFalse  (p.Matches (77));
        }


        TEST_METHOD (RejectedSpan_recordsExactBeginEndOfMalformedToken)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, abc, 17", TrackSectorPredicate::Mode::Track);

            // "0-2, abc, 17"
            //  0123456789012
            //       abc is [5, 8).
            Assert::AreEqual ((size_t) 1, p.GetRejectedSpans().size());
            Assert::AreEqual (5, p.GetRejectedSpans()[0].beginUtf16);
            Assert::AreEqual (8, p.GetRejectedSpans()[0].endUtf16);
        }


        TEST_METHOD (RejectedSpan_recordsMultipleMalformedTokensIndividually)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"xx, 5, yy", TrackSectorPredicate::Mode::Track);

            // "xx, 5, yy"
            //  012345678
            // xx is [0, 2); yy is [7, 9).
            Assert::AreEqual ((size_t) 2, p.GetRejectedSpans().size());
            Assert::AreEqual (0, p.GetRejectedSpans()[0].beginUtf16);
            Assert::AreEqual (2, p.GetRejectedSpans()[0].endUtf16);
            Assert::AreEqual (7, p.GetRejectedSpans()[1].beginUtf16);
            Assert::AreEqual (9, p.GetRejectedSpans()[1].endUtf16);
            Assert::IsTrue   (p.Matches (5));
        }


        TEST_METHOD (FullyValidExpression_yieldsEmptyRejectedSpanList)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"0-2, 17, 30-34", TrackSectorPredicate::Mode::Track);

            Assert::AreEqual ((size_t) 0, p.GetRejectedSpans().size());
        }


        TEST_METHOD (AllJunkExpression_recordsAllSpansAndMatchesNothing)
        {
            TrackSectorPredicate  p = TrackSectorPredicate::Parse (L"abc, def", TrackSectorPredicate::Mode::Track);

            // "abc, def"
            //  01234567
            // abc is [0, 3); def is [5, 8).
            Assert::AreEqual ((size_t) 2, p.GetRejectedSpans().size());
            Assert::AreEqual (0, p.GetRejectedSpans()[0].beginUtf16);
            Assert::AreEqual (3, p.GetRejectedSpans()[0].endUtf16);
            Assert::AreEqual (5, p.GetRejectedSpans()[1].beginUtf16);
            Assert::AreEqual (8, p.GetRejectedSpans()[1].endUtf16);

            // Critical: no token taken != empty input. Matches nothing.
            Assert::IsFalse (p.IsMatchAll());
            Assert::IsFalse (p.Matches (0));
            Assert::IsFalse (p.Matches (100));
        }
    };
}
