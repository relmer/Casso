#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Core/DxuiDpiScaler.h"
#include "MockDxuiTextRenderer.h"

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


        //  THE REAL STRING THAT FOUND THIS. The conflict report gained the
        //  question's layout, and it is the one banner-bound notice that
        //  carries paragraphs.
        TEST_METHOD (AConflictReportShapedNoticeIsSizedForEveryParagraph)
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

    };
}
