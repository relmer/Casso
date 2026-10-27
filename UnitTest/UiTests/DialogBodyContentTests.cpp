#include "Pch.h"

#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/DialogDefinition.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContentTests
//
//  The body's measured height, which sizes its dialog. The mock renderer
//  measures 7 pixels per character and 16 per line at 96 DPI, and wraps at
//  words.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DialogBodyContentTests)
{
public:

    static constexpr int  kWidthPx = 200;


    static DialogTextRun MakeRun (const std::wstring & text)
    {
        DialogTextRun  run;



        run.text = text;
        return run;
    }


    //  Forty five-character words wrap five to a 200-pixel line, so eight
    //  lines of 16 pixels: 128. The character-count estimate, 52 columns to a
    //  line, puts the same run at four lines of 18 pixels, 72, which would
    //  clip it.
    TEST_METHOD (AWrappedRunIsAsTallAsTheRendererMeasures)
    {
        DialogBodyContent     body;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        std::wstring          words;
        int                   heightPx = 0;



        for (int i = 0; i < 40; ++i)
        {
            words += L"word ";
        }

        scaler.SetDpi (96);
        body.SetRuns ({ MakeRun (words) });
        body.Layout  (RECT { 0, 0, kWidthPx, 72 }, scaler);

        Assert::IsTrue   (body.TryMeasureHeightPx (text, theme, kWidthPx, heightPx));
        Assert::AreEqual (128, heightPx);

        body.Layout (RECT { 0, 0, kWidthPx, heightPx }, scaler);
        Assert::AreEqual (128, (int) (body.GetChild (0)->GetBounds().bottom - body.GetChild (0)->GetBounds().top),
                          L"the label is laid out at the measured height");
    }


    //  An empty run is a one-line gap, measured or not.
    TEST_METHOD (AnEmptyRunIsOneLine)
    {
        DialogBodyContent     body;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        int                   heightPx = 0;



        scaler.SetDpi (96);
        body.SetRuns ({ MakeRun (L"One line."), MakeRun (L""), MakeRun (L"Another.") });
        body.Layout  (RECT { 0, 0, kWidthPx, 54 }, scaler);

        Assert::IsTrue   (body.TryMeasureHeightPx (text, theme, kWidthPx, heightPx));
        Assert::AreEqual (54, heightPx, L"three runs of one 18-pixel line each");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialogBodyContentPictureTests
//
//  A dialog's picture is drawn at its own aspect: its longer side is the
//  display size and the shorter side is scaled to match, so a screen
//  thumbnail is not squashed into a square.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DialogBodyContentPictureTests)
{
public:

    TEST_METHOD (AWidePictureIsMeasuredAtItsOwnAspect)
    {
        constexpr int     kDisplayDip  = 280;
        constexpr int     kThumbWidth  = 280;
        constexpr int     kThumbHeight = 192;
        DialogBodyContent wide;
        DialogBodyContent square;



        Assert::IsTrue (wide.SetImage   (MakeImage (kThumbWidth, kThumbHeight, kDisplayDip)), L"the wide picture is accepted");
        Assert::IsTrue (square.SetImage (MakeImage (kThumbWidth, kThumbWidth,  kDisplayDip)), L"the square picture is accepted");

        Assert::AreEqual (kThumbWidth - kThumbHeight,
                          square.GetPreferredHeightDip() - wide.GetPreferredHeightDip(),
                          L"the wide picture is its own height, not the display size");
    }


private:

    static DialogImage MakeImage (int width, int height, int displayDip)
    {
        constexpr size_t  kChannels = 4;
        constexpr Byte    kOpaque   = 0xFF;
        DialogImage       image;



        image.width     = width;
        image.height    = height;
        image.displayDp = (float) displayDip;
        image.rgba.assign ((size_t) width * (size_t) height * kChannels, kOpaque);

        return image;
    }
};





