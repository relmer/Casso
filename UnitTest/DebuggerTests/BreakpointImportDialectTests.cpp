#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointImportDialectTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ImportRig
    //
    //  Import from the breakpoints pane, run on a real session with no window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ImportRig : public ControllerRig
    {
    public:
        std::vector<std::string>  Import (const std::string & script)
        {
            BreakpointStep  step;

            files.WriteAllText (L"C:\\Work\\import.txt", script);
            step.kind = BreakpointStep::Kind::Import;
            step.path = "C:\\Work\\import.txt";
            return view.ExecuteBreakpointStep (controller.GetSession(), step);
        }

        std::vector<BreakpointInfo>  List()
        {
            BreakpointListData  list;

            BreakpointHandlers::ListAll (controller.GetSession(), list);
            return list.breakpoints;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectTests)
    {
    public:

        TEST_METHOD (WinDbgBreakpointsImportWithTheirStates)
        {
            ImportRig                    rig;
            std::vector<BreakpointInfo>  list;
            std::vector<std::string>     lines;



            lines = rig.Import ("bp 300\nba w1 400\nbd 1\n");
            list  = rig.List();

            Assert::AreEqual ((size_t) 2, list.size());
            Assert::AreEqual ((Word) 0x0300, list[0].address);
            Assert::IsTrue   (list[0].enabled);
            Assert::AreEqual ((Word) 0x0400, list[1].address);
            Assert::IsTrue   (list[1].access == WatchAccess::Write);
            Assert::IsFalse  (list[1].enabled, L"bd 1 disables the script's second breakpoint");
            Assert::IsTrue   (lines.back().find ("skipped 0 lines") != std::string::npos);
        }


        TEST_METHOD (GSSquaredDataBreakpointIsNotReadAsAppleWinDisable)
        {
            ImportRig                    rig;
            std::vector<BreakpointInfo>  list;



            rig.Import ("bp 300\nbpd 400 r\n");
            list = rig.List();

            Assert::AreEqual ((size_t) 2, list.size(), L"bpd 400 r is GSSquared's read breakpoint");
            Assert::AreEqual ((Word) 0x0400, list[1].address);
            Assert::IsTrue   (list[1].access == WatchAccess::Read);
            Assert::IsTrue   (list[0].enabled);
        }


        TEST_METHOD (AModeLineSetsTheDialectOfTheLinesAfterIt)
        {
            ImportRig                    rig;
            std::vector<BreakpointInfo>  list;
            std::vector<std::string>     lines;



            lines = rig.Import ("MODE WINDBG\nbp 300\nbd 0\n");
            list  = rig.List();

            Assert::AreEqual ((size_t) 1, list.size());
            Assert::IsFalse  (list[0].enabled);
            Assert::IsTrue   (lines.back().find ("skipped 0 lines") != std::string::npos, L"the MODE line is read, not skipped");
            Assert::IsTrue   (rig.controller.GetSession().GetMode() == CommandMode::AppleWin, L"the import leaves the console's dialect alone");
        }
    };
}
