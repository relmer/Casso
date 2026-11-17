#include "Pch.h"

#include "CommandLineParser.h"
#include "Ui/Dialogs/CommandLineHelpDialog.h"
#include "Ui/Dialogs/CommandLineHelpPanel.h"
#include "Version.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineHelpPanelTests
//
//  The command-line help dialog's content and its measured layout. The mock
//  renderer measures 7 pixels per character and 16 per line at 96 DPI, and
//  wraps at words.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandLineHelpPanelTests)
{
public:

    static constexpr int  kWidthPx = 660;


    //  The panel's measured height at kWidthPx, laid out first so its scaler
    //  is set.
    static int MeasurePanel (CommandLineHelpPanel & panel, const CommandLineHelpPanel::Content & content)
    {
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        int                   heightPx = 0;



        scaler.SetDpi (DxuiDpiScaler::kBaseDpi);
        panel.Init   (content);
        panel.Layout (RECT { 0, 0, kWidthPx, 400 }, scaler);

        Assert::IsTrue (panel.TryMeasureHeightPx (text, theme, kWidthPx, heightPx));

        return heightPx;
    }


    //  THE CONTENT IS THE NAME, THE VERSION AND ONE ROW PER FLAG, with the
    //  refusal only when there is one.
    TEST_METHOD (ContentHasTheNameVersionAndEveryFlag)
    {
        CommandLineHelpPanel::Content  clean   = CommandLineHelpDialog::MakeContent ('-', "");
        CommandLineHelpPanel::Content  refused = CommandLineHelpDialog::MakeContent ('/', "Error: unknown option /dsik1");
        size_t                         flags   = CommandLineParser::GetEmulatorFlags().size();



        Assert::IsTrue   (flags > 0, L"a sweep over an empty table checks nothing");
        Assert::AreEqual (std::wstring (L"Casso"), clean.title);
        Assert::IsTrue   (clean.version.find (_CRT_WIDE (VERSION_STRING)) != std::wstring::npos, L"the version is the build's");
        Assert::AreEqual (std::wstring (L"Casso [options]"), clean.usage);
        Assert::AreEqual (flags, clean.rows.size());
        Assert::IsTrue   (clean.refusal.empty());

        Assert::AreEqual (std::wstring (L"Error: unknown option /dsik1"), refused.refusal);
        Assert::IsTrue   (refused.rows[1].option.starts_with (L"/disk1"), L"rows follow the prefix the reader typed");
    }


    //  THE SWITCH COLUMN IS AS WIDE AS THE WIDEST SWITCH THE RENDERER
    //  MEASURES, not an estimate.
    TEST_METHOD (SwitchColumnIsTheMeasuredWidestSwitch)
    {
        constexpr int  kCharPx = 7;

        CommandLineHelpPanel           panel;
        CommandLineHelpPanel::Content  content = CommandLineHelpDialog::MakeContent ('-', "");
        size_t                         widest  = 0;



        for (const CommandLineHelpPanel::Row & row : content.rows)
        {
            widest = std::max (widest, row.option.size());
        }

        MeasurePanel (panel, content);

        Assert::AreEqual ((int) widest * kCharPx + 1, panel.GetTable().optionWidthPx);
        Assert::IsTrue   (panel.GetTable().descriptionLeftPx > panel.GetTable().optionWidthPx,
                          L"the descriptions start right of the switches");
    }


    //  A LAYOUT WITH NO WIDTH, AS A MINIMIZED WINDOW GETS, DOES NOT CHANGE THE
    //  MEASURED TABLE. It once replaced the table with an estimate made at no
    //  width while the table was marked as measured, and the restored dialog
    //  drew every switch in a column of no width.
    TEST_METHOD (MinimizedLayoutKeepsTheMeasuredTable)
    {
        CommandLineHelpPanel           panel;
        CommandLineHelpPanel::Content  content = CommandLineHelpDialog::MakeContent ('-', "");
        DxuiDpiScaler                  scaler;
        int                            before  = 0;



        MeasurePanel (panel, content);

        before = panel.GetTable().optionWidthPx;

        scaler.SetDpi (DxuiDpiScaler::kBaseDpi);
        panel.Layout (RECT { 0, 0, -40, -100 }, scaler);
        panel.Layout (RECT { 0, 0, kWidthPx, 400 }, scaler);

        Assert::IsTrue   (before > 0);
        Assert::AreEqual (before, panel.GetTable().optionWidthPx);
    }


    //  A LONG DESCRIPTION WRAPS INTO A TALLER ROW, and a refusal adds to the
    //  dialog's height rather than pushing the table out of it.
    TEST_METHOD (WrappedRowsAndRefusalAddHeight)
    {
        CommandLineHelpPanel           cleanPanel;
        CommandLineHelpPanel           refusedPanel;
        CommandLineHelpPanel::Content  clean      = CommandLineHelpDialog::MakeContent ('-', "");
        CommandLineHelpPanel::Content  refused    = CommandLineHelpDialog::MakeContent ('-', "Error: unknown option --dsik1");
        int                            cleanH     = MeasurePanel (cleanPanel, clean);
        int                            refusedH   = MeasurePanel (refusedPanel, refused);
        int                            shortest   = INT_MAX;
        int                            tallest    = 0;



        for (const CommandLineHelpTable::Row & row : cleanPanel.GetTable().rows)
        {
            shortest = std::min (shortest, row.heightPx);
            tallest  = std::max (tallest,  row.heightPx);
        }

        Assert::IsTrue (cleanPanel.GetTable().rows.size() == clean.rows.size());
        Assert::IsTrue (tallest > shortest, L"the longest description wraps to more than one line");
        Assert::IsTrue (refusedH > cleanH, L"the error line adds to the height");
    }
};
