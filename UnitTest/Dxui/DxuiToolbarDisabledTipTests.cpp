#include "Pch.h"

#include "Widgets/DxuiToolbar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDisabledTipTests
//
//  A disabled entry still shows its tooltip on hover, so the strip says what
//  a grayed button would do.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarDisabledTipTests)
{
public:

    static const wchar_t * GetTipAtEntry (bool enabled)
    {
        DxuiToolbar                      bar;
        DxuiDpiScaler                    scaler;
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();
        RECT                             rc      = {};
        RECT                             anchor  = {};
        static std::wstring              s_tip;


        command->id        = 1;
        command->label     = L"Step";
        command->glyph     = L"x";
        command->tip       = L"Step into";
        command->isEnabled = [enabled] () { return enabled; };

        entries[0].command = command;

        scaler.SetDpi (96);
        bar.SetEntries (std::move (entries));
        bar.Layout     (RECT { 0, 0, 2000, 40 }, scaler);

        Assert::IsTrue (bar.TryGetEntryRect (1, rc), L"the entry is on the strip");

        const wchar_t *  tip = bar.GetTooltipAt ((rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2, anchor);

        s_tip = tip != nullptr ? tip : L"";
        return s_tip.c_str();
    }


    TEST_METHOD (ADisabledEntryShowsItsExplicitTip)
    {
        Assert::AreEqual (L"Step into", GetTipAtEntry (false));
    }


    TEST_METHOD (AnEnabledEntryShowsItsExplicitTip)
    {
        Assert::AreEqual (L"Step into", GetTipAtEntry (true));
    }
};
