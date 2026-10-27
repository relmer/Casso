#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DxuiImageStripMarkTests
{
    static constexpr float     kAspect     = 560.0f / 384.0f;
    static constexpr float     kLabelRoom  = 20.0f;
    static constexpr RECT      kStripRect  = { 0, 0, 600, 82 };
    static constexpr wchar_t   kResetGlyph = L'\xE72C';
    static constexpr wchar_t   kForkGlyph  = L'\xE8B1';
    static constexpr uint64_t  kForkMark   = 4500;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MarkingSource
    //
    //  A strip source with no pictures and the marks a test gives it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MarkingSource : public IDxuiImageStripSource
    {
    public:
        void   SetCellLayout   (int count, SIZE cellPx) override { cells = count; size = cellPx; }
        Image  GetCellImage    (int index) override              { (void) index; return nullptr; }
        Image  GetPreviewImage (int index) override              { (void) index; return nullptr; }
        void   OnCellClicked   (int index) override              { (void) index; }

        void   GetMarks (std::vector<DxuiStripMark> & outMarks) override { outMarks = marks; }

        void   GetForkRows (uint64_t markId, std::vector<DxuiStripForkRow> & outRows) override
        {
            outRows = (markId == forkMark) ? rows : std::vector<DxuiStripForkRow>();
        }

        void   OnForkRowHovered (uint64_t rowId) override { hovered = rowId; hovers++; }
        void   OnForkRowClicked (uint64_t rowId) override { clicked = rowId; }

        static DxuiStripMark  MakeMark (float offset, wchar_t glyph, const wchar_t * tip, int count = 1)
        {
            DxuiStripMark  mark;



            mark.offset = offset;
            mark.glyph  = glyph;
            mark.tip    = tip;
            mark.count  = count;
            return mark;
        }

        int                             cells    = 0;
        SIZE                            size     = {};
        std::vector<DxuiStripMark>      marks;
        std::vector<DxuiStripForkRow>   rows;
        uint64_t                        forkMark = 0;
        uint64_t                        hovered  = 0;
        uint64_t                        clicked  = 0;
        int                             hovers   = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  StripRig
    //
    //  A strip lying down, laid out at 96 DPI over a marking source.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class StripRig
    {
    public:
        DxuiImageStrip        strip;
        MarkingSource         source;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;



        StripRig()
        {
            strip.SetSource       (&source);
            strip.SetAspect       (kAspect);
            strip.SetTextRenderer (&text);
            strip.SetLabelRoomDp  (kLabelRoom);
            strip.Layout          (kStripRect, true, scaler);
        }



        void Paint()
        {
            strip.Paint (painter, text, theme, false, false, true);
        }



        int CountStrings (const std::wstring & label) const
        {
            int  count = 0;



            for (const RecordedTextCall & call : text.Calls())
            {
                count += (call.kind == RecordedTextKind::DrawString && call.text == label) ? 1 : 0;
            }

            return count;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DxuiImageStripMarkTests
    //
    //  The marks a source gives a strip: each a line and a glyph badge at its
    //  offset, a count beside a glyph above 1, marks too close to tell apart
    //  merged into one badge, and each badge's tip.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DxuiImageStripMarkTests)
    {
    public:

        TEST_METHOD (MarksPaintAtTheirOffsetsWithAGlyphBadge)
        {
            StripRig  rig;
            RECT      pictures = {};



            rig.source.marks = { MarkingSource::MakeMark (1.0f, kResetGlyph, L"Reset"), MarkingSource::MakeMark (3.5f, kResetGlyph, L"Reset") };
            rig.Paint();

            pictures = rig.strip.GetPicturesRect();

            Assert::AreEqual<size_t> (2, rig.strip.GetMarkBadges().size(), L"a badge for each mark");
            Assert::AreEqual (rig.strip.GetLineAlong (1.0f), rig.strip.GetMarkBadges()[0].along, L"the first at its offset");
            Assert::AreEqual (rig.strip.GetLineAlong (3.5f), rig.strip.GetMarkBadges()[1].along, L"the second at its offset");
            Assert::AreEqual (pictures.bottom, rig.strip.GetMarkBadges()[0].rect.bottom, L"on the pictures' bottom edge");
            Assert::AreEqual (2, rig.CountStrings (std::wstring (1, kResetGlyph)), L"each with its glyph");
        }


        TEST_METHOD (AMarkWithACountDrawsItBesideTheGlyph)
        {
            StripRig  rig;
            RECT      single = {};
            RECT      wide   = {};



            rig.source.marks = { MarkingSource::MakeMark (1.0f, kResetGlyph, L"Reset"), MarkingSource::MakeMark (4.0f, kForkGlyph, L"Fork", 3) };
            rig.Paint();

            Assert::AreEqual<size_t> (2, rig.strip.GetMarkBadges().size(), L"a badge for each mark");

            single = rig.strip.GetMarkBadges()[0].rect;
            wide   = rig.strip.GetMarkBadges()[1].rect;

            Assert::AreEqual (1, rig.CountStrings (std::wstring (1, kForkGlyph) + L"3"), L"the count beside the glyph");
            Assert::IsTrue   (wide.right - wide.left > single.right - single.left, L"in a wider badge");
        }


        TEST_METHOD (MarksTooCloseToTellApartShareABadge)
        {
            StripRig  rig;



            rig.source.marks = { MarkingSource::MakeMark (2.0f, kResetGlyph, L"Reset"), MarkingSource::MakeMark (2.01f, kResetGlyph, L"Power cycle") };
            rig.Paint();

            Assert::AreEqual<size_t> (1, rig.strip.GetMarkBadges().size(), L"one badge");
            Assert::AreEqual (std::wstring (L"Reset\nPower cycle"), rig.strip.GetMarkBadges().front().tip, L"whose tip lists both");
        }


        TEST_METHOD (ABadgeHasItsTip)
        {
            StripRig         rig;
            RECT             badge  = {};
            RECT             anchor = {};
            const wchar_t  * tip    = nullptr;



            rig.source.marks = { MarkingSource::MakeMark (2.0f, kResetGlyph, L"Reset at 10:15:03") };
            rig.Paint();

            Assert::AreEqual<size_t> (1, rig.strip.GetMarkBadges().size(), L"a badge for the mark");

            badge = rig.strip.GetMarkBadges()[0].rect;
            tip   = rig.strip.GetTooltipAt ((badge.left + badge.right) / 2, (badge.top + badge.bottom) / 2, anchor);

            Assert::IsNotNull (tip, L"the badge has a tip");
            Assert::AreEqual  (std::wstring (L"Reset at 10:15:03"), std::wstring (tip));
            Assert::IsTrue    (EqualRect (&anchor, &badge) != FALSE, L"anchored on the badge");
        }


        //  Two rows for the fork mark kForkMark.
        static void AddForkRows (MarkingSource & source)
        {
            DxuiStripForkRow  row;



            source.forkMark = kForkMark;

            row.id     = 11;
            row.ranFor = L"Ran 0:05 past the fork";
            row.leftAt = L"Left at 10:15:03 PM";
            source.rows.push_back (row);

            row.id = 12;
            source.rows.push_back (row);
        }


        TEST_METHOD (AFlyoutOpensAfterTheDelayWithARowPerBranch)
        {
            MarkingSource    source;
            DxuiStripFlyout  flyout;



            AddForkRows (source);

            flyout.Track (&source, true, kForkMark, RECT {}, 0);
            flyout.Sync  (DxuiStripFlyout::kOpenDelayMs - 1);

            Assert::IsFalse (flyout.IsOpen(), L"not before the delay");

            flyout.Sync (DxuiStripFlyout::kOpenDelayMs);

            Assert::IsTrue           (flyout.IsOpen(), L"open once the pointer has rested");
            Assert::AreEqual<size_t> (2, flyout.GetRows().size(), L"a row per branch");
        }


        TEST_METHOD (AHoverGoesToTheSourceAndAClickSwitchesAndCloses)
        {
            MarkingSource    source;
            DxuiStripFlyout  flyout;
            int              rowPx = (int) (DxuiStripFlyout::kPictureDip + 2.0f * DxuiStripFlyout::kPadDip);



            AddForkRows (source);

            flyout.Track (&source, true, kForkMark, RECT {}, 0);
            flyout.Sync  (DxuiStripFlyout::kOpenDelayMs);

            flyout.HoverAt (POINT { 10, rowPx + 1 });

            Assert::AreEqual<uint64_t> (12, source.hovered, L"the second row is hovered");

            flyout.ClickAt (POINT { 10, rowPx + 1 });

            Assert::AreEqual<uint64_t> (12, source.clicked, L"a click goes to the source");
            Assert::AreEqual<uint64_t> (0,  source.hovered, L"and the hover ends");
            Assert::IsFalse            (flyout.IsOpen(), L"the flyout closes");
        }


        TEST_METHOD (MovingOffTheMarkClosesTheFlyout)
        {
            MarkingSource    source;
            DxuiStripFlyout  flyout;



            AddForkRows (source);

            flyout.Track (&source, true, kForkMark, RECT {}, 0);
            flyout.Sync  (DxuiStripFlyout::kOpenDelayMs);
            flyout.Track (&source, false, 0, RECT {}, DxuiStripFlyout::kOpenDelayMs);

            Assert::IsFalse (flyout.IsOpen(), L"closed");
        }


        TEST_METHOD (RowsAreHitByHeight)
        {
            Assert::AreEqual (0,  DxuiStripFlyout::HitTestRow (0,   76, 2), L"the first row's top");
            Assert::AreEqual (1,  DxuiStripFlyout::HitTestRow (76,  76, 2), L"the second row's top");
            Assert::AreEqual (-1, DxuiStripFlyout::HitTestRow (152, 76, 2), L"below the rows");
            Assert::AreEqual (-1, DxuiStripFlyout::HitTestRow (-1,  76, 2), L"above them");
        }


        TEST_METHOD (AMarkWithRowsHasNoTip)
        {
            StripRig       rig;
            RECT           anchor = {};
            DxuiStripMark  mark   = MarkingSource::MakeMark (2.0f, kForkGlyph, L"Other branches");
            RECT           badge  = {};



            mark.hasRows     = true;
            rig.source.marks = { mark };
            rig.Paint();

            Assert::AreEqual<size_t> (1, rig.strip.GetMarkBadges().size(), L"a badge for the mark");

            badge = rig.strip.GetMarkBadges()[0].rect;

            Assert::IsNull (rig.strip.GetTooltipAt ((badge.left + badge.right) / 2, (badge.top + badge.bottom) / 2, anchor), L"its flyout opens in place of a tip");
        }


        TEST_METHOD (ASourceWithNoMarksPaintsNone)
        {
            StripRig  rig;



            rig.Paint();

            Assert::IsTrue (rig.strip.GetMarkBadges().empty(), L"no badges");
        }
    };
}
