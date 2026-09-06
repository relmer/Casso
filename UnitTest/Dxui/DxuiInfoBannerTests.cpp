#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Core/DxuiDpiScaler.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBannerTests
//
//  The banner's renderer-free preferred-height heuristic -- the piece the page
//  relies on to reflow controls beneath it. No painter / text renderer needed;
//  height is estimated from the text length + width so Layout (which has no
//  renderer) can size the banner.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiInfoBannerTests
{
    static DxuiDpiScaler Scaler96()
    {
        DxuiDpiScaler  s;

        s.SetDpi (96);   // 1:1 DIP == px
        return s;
    }


    TEST_CLASS (DxuiInfoBannerTests)
    {
    public:

        TEST_METHOD (LongerTextWrapsTaller)
        {
            DxuiDpiScaler  scaler = Scaler96();
            float          hShort = 0.0f;
            float          hLong  = 0.0f;
            DxuiInfoBanner  shortBanner (L"Short.");
            DxuiInfoBanner  longBanner  (L"A considerably longer notice that has to wrap across several "
                                         L"lines when the banner is only a couple hundred pixels wide.");

            hShort = shortBanner.GetPreferredHeightPx (220.0f, scaler);
            hLong = longBanner.GetPreferredHeightPx  (220.0f, scaler);

            Assert::IsTrue (hShort > 0.0f, L"a one-line banner has a positive height");
            Assert::IsTrue (hLong > hShort, L"a longer message wraps to a taller banner");
        }


        //  A FORCED BREAK ENDS A LINE WHEREVER IT FALLS. The estimate used to
        //  divide the whole string by the characters that fit, which assumes
        //  every line is full and so loses one line for each partly-filled
        //  line a break leaves behind. Nothing here had ever been given a
        //  newline, and the change banner over the machine is laid out with
        //  this path rather than the measured one, so the first multi-paragraph
        //  notice would have been sized to show only its opening lines.
        TEST_METHOD (ForcedBreaksCountAsLinesTheLengthAloneCannotShow)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  flowed (L"aaaa bbbb cccc dddd");
            DxuiInfoBanner  broken (L"aaaa\n\nbbbb\n\ncccc\n\ndddd");



            //  Both hold the same words at a width that fits them on one line,
            //  so only the breaks can separate the two heights.
            Assert::IsTrue (broken.GetPreferredHeightPx (600.0f, scaler)
                          > flowed.GetPreferredHeightPx (600.0f, scaler),
                            L"a broken-up notice is taller than the same words flowing");
        }


        //  The blank line between two paragraphs occupies a line of its own,
        //  which is what makes a two-break notice taller than a one-break one.
        TEST_METHOD (ABlankLineBetweenParagraphsTakesItsOwnLine)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  single (L"aaaa\nbbbb");
            DxuiInfoBanner  spaced (L"aaaa\n\nbbbb");



            Assert::IsTrue (spaced.GetPreferredHeightPx (600.0f, scaler)
                          > single.GetPreferredHeightPx (600.0f, scaler));
        }


        //  A NOTICE-SIZED STRING, not a toy one. No shipped banner carries
        //  paragraphs today -- the disk notices that reach the bar were rewritten
        //  to flow once it turned out they land in a strip -- but the estimate is
        //  wrong for any that does, and the next one should not have to
        //  rediscover it.
        TEST_METHOD (AMultiParagraphNoticeIsSizedForEveryParagraph)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  banner (L"Another program modified this disk while it was mounted "
                                    L"in Casso:\n\nwork.dsk (Drive 1)\n\nYour disk has been renamed "
                                    L"to work.20260905-010203-01.dsk to avoid a conflict with "
                                    L"the other program, and has been remounted in Drive 1. No "
                                    L"changes were made to the other program's modified version "
                                    L"of work.dsk.");
            DxuiInfoBanner  runOn  (L"Another program modified this disk while it was mounted "
                                    L"in Casso: work.dsk (Drive 1) Your disk has been renamed "
                                    L"to work.20260905-010203-01.dsk to avoid a conflict with "
                                    L"the other program, and has been remounted in Drive 1. No "
                                    L"changes were made to the other program's modified version "
                                    L"of work.dsk.");



            Assert::IsTrue (banner.GetPreferredHeightPx (700.0f, scaler)
                          > runOn.GetPreferredHeightPx (700.0f, scaler),
                            L"the paragraphs are not being counted");
        }


        //  NARROWER THAN A GLYPH IS NOT AN ESTIMATE. The printer page sizes its
        //  banner from a page rect that is briefly tiny, and counting a line per
        //  character there would ask for hundreds of lines of height.
        TEST_METHOD (AWidthNarrowerThanOneGlyphStillAsksForOneLine)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  banner (L"A notice long enough that one line per character "
                                    L"would be hundreds of lines of banner, which is what "
                                    L"a degenerate width used to produce.");
            DxuiInfoBanner  tiny   (L"Short.");



            Assert::AreEqual (tiny.GetPreferredHeightPx   (1.0f, scaler),
                              banner.GetPreferredHeightPx (1.0f, scaler),
                              L"length must not matter once nothing can be estimated");
        }


        TEST_METHOD (WiderBannerWrapsShorter)
        {
            DxuiDpiScaler  scaler  = Scaler96();
            float          hNarrow = 0.0f;
            float          hWide   = 0.0f;
            DxuiInfoBanner  banner (L"A considerably longer notice that has to wrap across several lines "
                                    L"when the banner is narrow, but fits far fewer when it is wide.");

            hNarrow = banner.GetPreferredHeightPx (200.0f, scaler);
            hWide = banner.GetPreferredHeightPx (700.0f, scaler);

            Assert::IsTrue (hWide <= hNarrow, L"a wider banner wraps to fewer-or-equal lines");
        }


        TEST_METHOD (EmptyBannerStillClearsTheIcon)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  banner (L"");

            // Even with no text the banner is at least one icon tall plus padding,
            // so a blank message never collapses to nothing.
            Assert::IsTrue (banner.GetPreferredHeightPx (300.0f, scaler) > 0.0f,
                            L"an empty banner keeps a positive, icon-clearing height");
        }

        TEST_METHOD (MeasuredHeight_UsesTheRendererInsteadOfTheEstimate)
        {
            // PreferredHeightPx has no renderer, so it rounds up and never
            // clips -- harmless on an auto-sized surface, but on a fixed
            // dialog the slack shows as an empty line inside the box. A caller
            // that HAS a renderer can ask for the real height instead.
            DxuiInfoBanner        banner (L"a short warning");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler    = Scaler96();
            SIZE                  oneLine   = { 200, 16 };
            float                 measured  = 0.0f;
            float                 estimated = 0.0f;

            text.SetCannedMetrics (L"a short warning", oneLine);

            measured  = banner.GetMeasuredHeightPx (text, 400.0f, scaler);
            estimated = banner.GetPreferredHeightPx (400.0f, scaler);

            Assert::IsTrue (measured > 0.0f, L"a measured height must be produced");
            Assert::IsTrue (measured <= estimated,
                L"measuring must never be taller than the deliberately generous estimate");
        }



        TEST_METHOD (MeasuredHeight_TallerTextNeedsMoreRoom)
        {
            DxuiInfoBanner        banner (L"tall");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler = Scaler96();
            SIZE                  one    = { 200, 16 };
            SIZE                  three  = { 200, 48 };
            float                 hOne   = 0.0f;
            float                 hThree = 0.0f;

            text.SetCannedMetrics (L"tall", one);
            hOne = banner.GetMeasuredHeightPx (text, 400.0f, scaler);

            text.SetCannedMetrics (L"tall", three);
            hThree = banner.GetMeasuredHeightPx (text, 400.0f, scaler);

            Assert::IsTrue (hThree > hOne, L"three lines of text need more height than one");
        }



        TEST_METHOD (MeasuredHeight_FallsBackToTheEstimateWhenMeasurementFails)
        {
            // DirectWrite can transiently report a zero-width layout mid-resize.
            // Trusting that would collapse the banner to its padding, so a
            // failed measurement has to fall back rather than be believed.
            DxuiInfoBanner        banner (L"a warning long enough to wrap more than once in a narrow banner");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler = Scaler96();

            text.SetMeasureReturnsZero (true);

            Assert::AreEqual (banner.GetPreferredHeightPx (200.0f, scaler),
                              banner.GetMeasuredHeightPx (text, 200.0f, scaler),
                              L"a zero measurement must fall back to the estimate");
        }


        ////////////////////////////////////////////////////////////////////
        //  Centering: the cap, the even split, and what is left alone
        ////////////////////////////////////////////////////////////////////

        //  A LINE AS WIDE AS THE WINDOW IS NOT A LINE ANYONE READS. A message
        //  bar spans the client, so a centered banner caps its line and wraps
        //  past that even though there was room to keep going.
        TEST_METHOD (Centered_CapsTheLineOnAVeryWideBar)
        {
            DxuiDpiScaler   scaler = Scaler96();
            std::wstring    words (400, L'x');       // 400 * 7 dip = 2800 dip wide
            DxuiInfoBanner  plain  (words);
            DxuiInfoBanner  center (words);

            center.SetCentered (true);

            Assert::IsTrue (center.GetPreferredHeightPx (4000.0f, scaler)
                              > plain.GetPreferredHeightPx (4000.0f, scaler),
                            L"the centered banner wraps where the plain one runs on");
        }


        //  Short text is under the cap, so centering changes nothing about
        //  the height -- only where the line sits.
        TEST_METHOD (Centered_ShortLineKeepsItsHeight)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  plain  (L"Press Esc to release the mouse and exit paddle mode");
            DxuiInfoBanner  center (L"Press Esc to release the mouse and exit paddle mode");

            center.SetCentered (true);

            Assert::AreEqual (plain.GetPreferredHeightPx (1400.0f, scaler),
                              center.GetPreferredHeightPx (1400.0f, scaler),
                              L"a line that fits under the cap is not wrapped by centering");
        }


        //  EVENLY, so the last line is as full as the others. Filling each
        //  line to the cap leaves a stub under a full-width block; the split
        //  gives every line the same share of the text's own width.
        TEST_METHOD (Centered_WrapSplitsTheWidthEvenly)
        {
            DxuiDpiScaler         scaler  = Scaler96();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            std::wstring          words (400, L'x');   // 2800 dip on one line
            DxuiInfoBanner        banner (words);
            float                 drawnW  = 0.0f;
            float                 height  = 0.0f;

            banner.SetCentered (true);

            //  Three lines: 2800 dip of text against a 1024 dip cap. Evenly
            //  split that is ~933 each, NOT 1024 + 1024 + 752.
            height = banner.GetPreferredHeightPx (4000.0f, scaler);
            banner.Layout (RECT{ 0, 0, 4000, (LONG) height }, scaler);
            static_cast<IDxuiControl &> (banner).Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString)
                {
                    drawnW = call.width;
                }
            }

            Assert::IsTrue (drawnW > 0.0f, L"the banner drew its text");
            Assert::IsTrue (drawnW < 1000.0f, L"the line box is the even share, not the cap");
            Assert::IsTrue (drawnW > 860.0f,  L"...and not narrower than the share either");
        }


        //  The badge belongs to the words: the group is centered, so the text
        //  starts past the middle-left of the bar rather than at its edge.
        TEST_METHOD (Centered_GroupSitsInTheMiddle)
        {
            DxuiDpiScaler         scaler = Scaler96();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiInfoBanner        banner (L"Press Esc to release the mouse");
            float                 drawnX = 0.0f;
            DxuiTextHAlign        align  = DxuiTextHAlign::Left;

            banner.SetCentered (true);
            banner.Layout (RECT{ 0, 0, 1000, 40 }, scaler);
            static_cast<IDxuiControl &> (banner).Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString)
                {
                    drawnX = call.x;
                    align  = call.hAlign;
                }
            }

            Assert::IsTrue (drawnX > 300.0f, L"the group is centered, not held at the leading edge");
            Assert::IsTrue (align == DxuiTextHAlign::Center, L"centered banners center their lines");
        }


        //  OFF BY DEFAULT: a banner in a dialog is a box sized to its text and
        //  reads from the leading edge, exactly as it always did.
        TEST_METHOD (Uncentered_StillStartsAtTheLeadingEdge)
        {
            DxuiDpiScaler         scaler = Scaler96();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiInfoBanner        banner (L"Press Esc to release the mouse");
            float                 drawnX = 1000.0f;
            DxuiTextHAlign        align  = DxuiTextHAlign::Center;

            banner.Layout (RECT{ 0, 0, 1000, 40 }, scaler);
            static_cast<IDxuiControl &> (banner).Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString)
                {
                    drawnX = call.x;
                    align  = call.hAlign;
                }
            }

            Assert::IsTrue (drawnX < 60.0f, L"an uncentered banner still starts at the leading edge");
            Assert::IsTrue (align == DxuiTextHAlign::Left, L"...and still left-aligns its text");
        }


        //  THE HEIGHT AND THE PAINT MUST AGREE. Both go through the same
        //  measured, cached box now, so the lines a caller reserves room for
        //  are exactly the lines the paint lays down -- a wide face cannot
        //  make the estimate promise one line fewer than the text needs.
        TEST_METHOD (Centered_ReservedHeightHoldsThePaintedBox)
        {
            DxuiDpiScaler         scaler = Scaler96();
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            std::wstring          words (400, L'x');   // 2800 dip on one line
            DxuiInfoBanner        banner (words);
            float                 height = 0.0f;
            float                 drawnW = 0.0f;
            float                 drawnH = 0.0f;
            float                 wrapW  = 0.0f;
            float                 wrapH  = 0.0f;

            banner.SetCentered (true);

            height = banner.GetMeasuredHeightPx (text, 4000.0f, scaler);
            banner.Layout (RECT{ 0, 0, 4000, (LONG) height }, scaler);
            static_cast<IDxuiControl &> (banner).Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString)
                {
                    drawnW = call.width;
                    drawnH = call.height;
                }
            }

            Assert::IsTrue (drawnW > 0.0f, L"the banner drew its text");

            //  What the text actually needs in the box the paint chose, against
            //  the room the paint was given inside the reserved height.
            Assert::AreEqual (S_OK, text.MeasureStringWrapped (words.c_str(), 13.0f,
                                                               DxuiTheme::kBodyFace,
                                                               drawnW, wrapW, wrapH));
            Assert::IsTrue (wrapH <= drawnH + 0.5f,
                            L"the reserved height holds every line the painted box wraps to");
        }

    };
}