#include "Pch.h"

#include "ControllerRig.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/CodeViewFind.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFindTests
//
//  A code view's find over the paused machine's lines for the whole address
//  space: matches in the address, bytes, label and instruction columns, none
//  in a hidden column or the annotation, the next match from the selected
//  line with a wrap at the end of memory, and the rows on screen searched
//  while the machine runs.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CodeViewFindTests)
    {
    public:

        using  Lines = CodeViewFind::Lines;


        static DebuggerViewSnapshot::CodeLine  MakeLine (Word address, const char * bytes, const char * label, const char * instruction)
        {
            DebuggerViewSnapshot::CodeLine  line;

            line.address     = address;
            line.bytes       = bytes;
            line.label       = label;
            line.instruction = instruction;
            return line;
        }


        //  Three lines far apart in memory, one with an annotation.
        static std::shared_ptr<const Lines>  MakeLines()
        {
            auto  lines = std::make_shared<Lines>();

            lines->push_back (MakeLine (0x0300, "A9 00",    "START", "LDA #$00"));
            lines->push_back (MakeLine (0x6000, "8D 00 C0", "",      "STA KBD"));
            lines->push_back (MakeLine (0xF000, "20 ED FD", "",      "JSR COUT"));
            (*lines)[1].annotation = "A=$00";
            return lines;
        }


        static CodeViewFind::Visible  AllVisible()
        {
            CodeViewFind::Visible  visible = {};

            visible.fill (true);
            return visible;
        }


        //  A view rig: a list with the disassembly's columns, the lines while
        //  paused, and the selection the find moves.
        struct Rig
        {
            DxuiListView                  list;
            DisassemblyOptions            options;
            std::shared_ptr<const Lines>  lines    = MakeLines();
            std::optional<Word>           selected;
            Word                          top      = 0;
            std::vector<Word>             selects;
            CodeViewFind                  find;

            Rig() :
                find (list, options,
                      [this] { return lines; },
                      [this] (bool isTop) { return isTop ? std::optional<Word> (top) : selected; },
                      [this] (Word address) { selects.push_back (address); selected = address; },
                      nullptr)
            {
                DisassemblyCells::Configure   (list);
                DisassemblyCells::ShowColumns (list, options);
            }
        };


        TEST_METHOD (ALabelBytesAddressAndInstructionAllMatch)
        {
            std::shared_ptr<const Lines>  lines = MakeLines();



            Assert::AreEqual ((size_t) 1, CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"START", false, false, false).size());
            Assert::AreEqual ((int) 0x6000, (int) CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"8D 00", false, false, false).at (0));
            Assert::AreEqual ((int) 0xF000, (int) CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"F000", false, false, false).at (0));
            Assert::AreEqual ((int) 0xF000, (int) CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"cout", false, false, false).at (0));
            Assert::IsTrue   (CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"cout", true, false, false).empty(), L"a case match");
        }


        TEST_METHOD (AHiddenColumnAndTheAnnotationDontMatch)
        {
            std::shared_ptr<const Lines>  lines   = MakeLines();
            CodeViewFind::Visible         visible = AllVisible();



            visible[DisassemblyCells::kBytesColumn] = false;

            Assert::IsTrue (CodeViewFind::FindLines (*lines, DisassemblyOptions(), visible, nullptr, L"8D 00", false, false, false).empty(), L"the bytes column is hidden");
            Assert::IsTrue (CodeViewFind::FindLines (*lines, DisassemblyOptions(), AllVisible(), nullptr, L"A=$00", false, false, false).empty(), L"the annotation");
        }


        //  With the Source option on, the source line above an instruction is
        //  searched, its match at the first instruction it produced; with
        //  the option off, it isn't.
        TEST_METHOD (ASourceLineMatchesWhileSourceLinesAreShown)
        {
            auto                    lines   = std::make_shared<Lines> (*MakeLines());
            DisassemblyOptions      options;
            CodeViewFind::SourceFn  source  = [] (int fileId, int line, std::wstring & text)
            {
                text = (fileId == 0 && line == 12) ? L"        LDA  #0    ; clear the count" : L"";
                return fileId == 0;
            };



            (*lines)[0].sourceFileId = 0;
            (*lines)[0].sourceLine   = 12;
            (*lines)[1].sourceFileId = 0;
            (*lines)[1].sourceLine   = 12;

            options.Set (DisassemblyOptions::Option::Source, true);

            std::vector<Word>  found = CodeViewFind::FindLines (*lines, options, AllVisible(), source, L"count", false, false, false);

            Assert::AreEqual ((size_t) 1, found.size(), L"once, at the line's first instruction");
            Assert::AreEqual ((int) 0x0300, (int) found[0]);

            options.Set (DisassemblyOptions::Option::Source, false);

            Assert::IsTrue (CodeViewFind::FindLines (*lines, options, AllVisible(), source, L"count", false, false, false).empty());
        }


        //  The next match after the selected line, then a wrap from the last
        //  to the first; and a needle with no match doesn't move the selection.
        TEST_METHOD (TheNextMatchIsAfterTheSelectionAndWraps)
        {
            Rig             rig;
            int             index = 0;
            int             count = 0;
            DxuiFindResult  result;



            rig.selected = 0x0300;

            result = rig.find.SelectMatch (L"A", false, false, false, true, index, count);

            Assert::IsTrue   (result == DxuiFindResult::Found);
            Assert::AreEqual ((int) 0x6000, (int) rig.selects.back(), L"STA, below the selection");
            Assert::AreEqual (2, count, L"the LDA and STA lines; the JSR's line has no A");

            rig.selected = 0xF000;
            result = rig.find.SelectMatch (L"A", false, false, false, true, index, count);

            Assert::IsTrue   (result == DxuiFindResult::Wrapped);
            Assert::AreEqual ((int) 0x0300, (int) rig.selects.back());

            result = rig.find.SelectMatch (L"A", false, false, false, false, index, count);

            Assert::AreEqual ((int) 0x6000, (int) rig.selects.back(), L"backward from the first wraps to the last match");

            rig.selects.clear();
            result = rig.find.SelectMatch (L"ZZZ", false, false, false, true, index, count);

            Assert::IsTrue   (result == DxuiFindResult::NotFound);
            Assert::IsTrue   (rig.selects.empty());
            Assert::AreEqual (0, count);
        }


        //  As the needle is typed, the first match at or after the top line,
        //  far below anything on screen.
        TEST_METHOD (TypingFindsTheFirstMatchFromTheTop)
        {
            Rig  rig;
            int  index = 0;
            int  count = 0;



            rig.top = 0x0400;

            (void) rig.find.SelectMatchInView (L"COUT", false, false, false, index, count);

            Assert::AreEqual ((int) 0xF000, (int) rig.selects.back());
            Assert::AreEqual (1, index);
        }


        //  While the machine runs there are no lines, and the rows on screen
        //  are searched instead; nothing is selected through the machine.
        TEST_METHOD (WhileRunningTheRowsOnScreenAreSearched)
        {
            Rig  rig;
            int  index = 0;
            int  count = 0;



            rig.lines = nullptr;

            (void) rig.find.SelectMatch (L"COUT", false, false, false, true, index, count);

            Assert::IsTrue (rig.selects.empty());
        }

        //  Steps that follow each other quickly don't make the find text; a
        //  pause with no step for the idle delay does, and the views are
        //  built again for it.
        TEST_METHOD (TheFindTextIsMadeOncePausedWithoutAStep)
        {
            ControllerRig         rig;
            DebugSession        & session = rig.controller.GetSession();
            Cpu6502Registers      r       = session.GetTarget().GetRegisters();
            DebuggerViewSnapshot  snapshot;



            snapshot = rig.view.BuildCaptured (session, true);
            r.pc     = (Word) (r.pc + 1);
            session.GetTarget().SetRegisters (r);
            snapshot = rig.view.BuildCaptured (session, true);

            Assert::IsTrue  (snapshot.codeFind == nullptr, L"not between steps");
            Assert::IsFalse (rig.view.IsFindTextDue (GetTickCount64()), L"not before the delay");
            Assert::IsTrue  (rig.view.IsFindTextDue (GetTickCount64() + DebuggerViewState::kFindTextIdleMs), L"due after it");

            rig.view.SetFindTextIdle (0);
            snapshot = rig.view.BuildCaptured (session, true);

            Assert::IsTrue  (snapshot.codeFind != nullptr && !snapshot.codeFind->empty(), L"made once idle");
        }


        //  A symbol added at a line's address is in the find text, with no
        //  change to memory.
        TEST_METHOD (TheFindTextHasASymbolAddedSinceItWasMade)
        {
            ControllerRig         rig;
            DebugSession        & session = rig.controller.GetSession();
            Word                  pc      = session.GetTarget().GetRegisters().pc;
            DebuggerViewSnapshot  snapshot;
            bool                  isFound = false;



            rig.view.SetFindTextIdle (0);
            (void) rig.view.BuildCaptured (session, true);

            session.GetSymbols().Add (SymbolTableId::User, "ZZFINDME", pc);
            snapshot = rig.view.BuildCaptured (session, true);

            Assert::IsTrue (snapshot.codeFind != nullptr);

            for (const DebuggerViewSnapshot::CodeLine & line : *snapshot.codeFind)
            {
                isFound = isFound || (line.address == pc && line.label == "ZZFINDME");
            }

            Assert::IsTrue (isFound, L"the new label");
        }


        //  Bytes written while paused, as F writes them, are in the find
        //  text, and what they replaced isn't.
        TEST_METHOD (TheFindTextHasMemoryWrittenWhilePaused)
        {
            ControllerRig         rig;
            DebugSession        & session = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;
            bool                  isOld   = false;
            bool                  isNew   = false;



            rig.view.SetFindTextIdle (0);
            (void) session.ExecuteLine ("F 6000,3 4C 34 12", CommandMode::AppleWin);
            (void) rig.view.BuildCaptured (session, true);
            snapshot = rig.view.BuildCaptured (session, true);

            (void) session.ExecuteLine ("F 6000,3 4C 78 56", CommandMode::AppleWin);
            (void) rig.view.BuildCaptured (session, true);
            snapshot = rig.view.BuildCaptured (session, true);

            Assert::IsTrue (snapshot.codeFind != nullptr);

            for (const DebuggerViewSnapshot::CodeLine & line : *snapshot.codeFind)
            {
                isOld = isOld || (line.address == 0x6000 && line.bytes.find ("34 12") != std::string::npos);
                isNew = isNew || (line.address == 0x6000 && line.bytes.find ("78 56") != std::string::npos);
            }

            Assert::IsTrue  (isNew, L"the bytes written");
            Assert::IsFalse (isOld, L"not the ones they replaced");
        }
    };
}
