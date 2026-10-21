#include "Pch.h"

#include "Ui/Dialogs/DialogBodyContent.h"
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
