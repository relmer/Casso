#pragma once

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Ui/Debugger/OperandResultTip.h"





namespace OperandResultTipTests
{
    //  A host that does nothing; the tests read only the window's own state.
    using TipHost = NullDebuggerWindowHost;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceTipHost
    //
    //  A host with the viewing options the window starts with and one source
    //  file, main.a65, for a disassembly with source rows.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SourceTipHost : public NullDebuggerWindowHost
    {
    public:
        std::string  GetDebuggerDisassemblyOptions() override { return options; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.text  = sourceText;
            return lookup;
        }

        std::string  options;
        std::string  sourceText;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipWindow : public DebuggerWindow
    {
    public:
        TipWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OperandTipCell;
        using DebuggerWindow::OperandTipScreen;
        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetOperandTips;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetColorKeys;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::kStatusZoom;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::GetTipList;
        using DebuggerWindow::kBasicTipView;
        using DebuggerWindow::FindPaneForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipMetrics
    //
    //  The sizes the operand tip tests lay rows out with, and the lines of a
    //  tip laid out.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipMetrics
    {
    public:
        using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

        //  The mock renderer measures every character this wide.
        static constexpr int   kGlyphPx     = 7;
        static constexpr int   kRowHeight   = 16;
        static constexpr int   kPadPx       = 4;
        static constexpr int   kScreenRight = 4000;
        static constexpr int   kScreenBelow = 3000;
        static constexpr RECT  kScreen      = { 0, 0, kScreenRight, kScreenBelow };


        //  The runs on a line of the tip, left to right.
        static std::vector<OperandResultTip::Run>  GetLine (const OperandResultTip::Layout & layout, int line)
        {
            std::vector<OperandResultTip::Run>  runs;



            std::ranges::copy_if (layout.runs, std::back_inserter (runs), [line] (const OperandResultTip::Run & run) { return run.line == line; });
            return runs;
        }


        static std::wstring  GetLineText (const OperandResultTip::Layout & layout, int line)
        {
            std::vector<OperandResultTip::Run>  runs = GetLine (layout, line);



            Assert::AreEqual ((size_t) 1, runs.size(), L"one run on the line");
            return runs[0].text;
        }
    };
}
