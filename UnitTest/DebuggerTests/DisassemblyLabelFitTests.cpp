#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DisassemblyLabelFitTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FitWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FitWindow : public DebuggerWindow
    {
    public:
        FitWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            DxuiDpiScaler  scaler;



            m_theme = &theme;
            m_host  = &host;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }

        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetCodeList;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DisassemblyLabelFitTests
    //
    //  Each load of symbols or of a debug file fits the Label column to the
    //  labels the rows then have, in place of a width the user dragged; a
    //  snapshot with no load doesn't change the width.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DisassemblyLabelFitTests)
    {
    public:
        static constexpr int  kDraggedLabelPx = 123;


        //  A stop with a line labeled START, with the symbols given, and a
        //  debug file when `hasSource` is set.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot (std::shared_ptr<const DebuggerViewSnapshot::SymbolsState> symbols, bool hasSource)
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  line;



            line.address     = 0x0300;
            line.bytes       = "A9 41";
            line.label       = "START";
            line.instruction = "LDA #$41";
            line.isCurrent   = true;

            snapshot->isPaused     = true;
            snapshot->codeOpen[0]  = true;
            snapshot->pc           = 0x0300;
            snapshot->codeViews[0] = { line };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->symbols      = std::move (symbols);

            if (hasSource)
            {
                snapshot->source                = DebuggerViewSnapshot::SourceState();
                snapshot->source->debugFilePath = L"C:\\Work\\main.dbg";
                snapshot->source->programKey    = "key";
            }

            return snapshot;
        }


        static std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>  MakeSymbols()
        {
            auto  symbols = std::make_shared<DebuggerViewSnapshot::SymbolsState>();



            symbols->symbols = { { "START", 0x0300 } };
            return symbols;
        }


        //  A stop before any load doesn't change a dragged width, and a symbol
        //  load fits the column. A width dragged after that is the same after
        //  the next stop, until a debug file loads and fits the column again.
        TEST_METHOD (ASymbolOrDebugFileLoadFitsTheLabelColumn)
        {
            CassoTheme                                                  theme   = CassoTheme::MakeSkeuomorphic();
            NullDebuggerWindowHost                                      host;
            FitWindow                                                   window  (theme, host);
            DxuiListView                                              * list    = window.GetCodeList (0);
            std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>   symbols = MakeSymbols();



            list->SetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn, kDraggedLabelPx);
            window.TakeSnapshot (MakeSnapshot (nullptr, false));
            Assert::AreEqual (kDraggedLabelPx, list->GetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn), L"a stop with no symbols loaded");

            window.TakeSnapshot (MakeSnapshot (symbols, false));
            Assert::AreEqual (-1, list->GetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn), L"the symbol load fits the column");

            list->SetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn, kDraggedLabelPx);
            window.TakeSnapshot (MakeSnapshot (symbols, false));
            Assert::AreEqual (kDraggedLabelPx, list->GetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn), L"the dragged width, at a stop with the same symbols");

            window.TakeSnapshot (MakeSnapshot (MakeSymbols(), true));
            Assert::AreEqual (-1, list->GetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn), L"the debug file load fits the column");
        }
    };
}
