#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "ControllerRig.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceLineOperandsTests
    //
    //  The source pane has the disassembly pane's operand, result and branch
    //  arrow columns: each line's operand in the PC's file, the result on the
    //  line at PC, and an arrow from that line to the line its branch goes to.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceLineOperandsTests)
    {
    public:
        //  LDA #$41 at line 2 and STA $0400 at line 3, the PC on line 2.
        static void LoadDebugFile (ControllerRig & rig)
        {
            DebugFile  file;



            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } },
                           { 1, 0, 3, DebugLineType::Asm, 0, { 1 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        //  JMP $0305 at line 2 and RTS at line 5, the PC on line 2.
        static void LoadJumpFile (ControllerRig & rig)
        {
            DebugFile  file;



            rig.machine.GetMemoryBus().WriteByte (0x0300, 0x4C);
            rig.machine.GetMemoryBus().WriteByte (0x0301, 0x05);
            rig.machine.GetMemoryBus().WriteByte (0x0302, 0x03);

            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", 0x0300, 6 });
            file.spans = { { 0, 0, 0, 3 }, { 1, 0, 5, 1 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm, 0, { 0 } },
                           { 1, 0, 5, DebugLineType::Asm, 0, { 1 } } };

            rig.controller.GetSession().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        TEST_METHOD (EachLineCarriesTheDisassemblyOperand)
        {
            ControllerRig                 rig;
            DebuggerViewSnapshot          snapshot;
            std::string                   storeAnnotation;
            std::string                   loadEffect;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
            {
                storeAnnotation = (line.address == 0x0302) ? line.annotation : storeAnnotation;
                loadEffect      = (line.address == 0x0300) ? line.effect     : loadEffect;
            }

            Assert::IsNotNull (snapshot.source->lineOperands.get());
            Assert::IsFalse   (storeAnnotation.empty(), L"the disassembly has an operand for STA $0400");
            Assert::AreEqual  (storeAnnotation, snapshot.source->lineOperands->at ({ 0, 3 }).first);
            Assert::AreEqual  (std::string(),   snapshot.source->lineOperands->at ({ 0, 3 }).second, L"a result only on the PC's line");
            Assert::IsFalse   (loadEffect.empty(), L"the disassembly has a result for the PC's LDA");
            Assert::AreEqual  (loadEffect,      snapshot.source->lineOperands->at ({ 0, 2 }).second);
        }



        TEST_METHOD (TheJumpAtPcCarriesItsTarget)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;
            std::optional<int>    targetLine;
            bool                  isBelow = false;



            LoadJumpFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue  (snapshot.source->pcTarget.has_value());
            Assert::AreEqual ((int) 0x0305, (int) *snapshot.source->pcTarget);
            Assert::IsTrue  (snapshot.source->isPcTargetTaken);
            Assert::IsTrue  (SourcePane::GetArrowTarget (*snapshot.source, 0, 2, targetLine, isBelow));
            Assert::AreEqual (5, targetLine.value_or (-1));
            Assert::IsTrue  (isBelow);
        }



        TEST_METHOD (NoArrowWithoutABranchAtPc)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot;
            std::optional<int>    targetLine;
            bool                  isBelow = false;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (snapshot.source->pcTarget.has_value());
            Assert::IsFalse (SourcePane::GetArrowTarget (*snapshot.source, 0, 2, targetLine, isBelow));
        }



        TEST_METHOD (ATargetInNoLineOfTheFileGoesByAddress)
        {
            DebuggerViewSnapshot::SourceState  state;
            std::optional<int>                 targetLine;
            bool                               isBelow = false;



            state.pcTarget      = (Word) 0x0200;
            state.lineAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                                      std::map<std::pair<int, int>, Word> { { { 0, 4 }, (Word) 0x0300 }, { { 1, 9 }, (Word) 0x0200 } });

            Assert::IsTrue  (SourcePane::GetArrowTarget (state, 0, 4, targetLine, isBelow));
            Assert::IsFalse (targetLine.has_value(), L"the target's line is in another file");
            Assert::IsFalse (isBelow, L"$0200 lies before the marked line's $0300");
        }



        TEST_METHOD (TheOperandFollowsWithItsResultLabeled)
        {
            SourcePane::Style               style;
            std::vector<DxuiTextView::Row>  rows;



            style.bytesArgb  = 0xFF808080;
            style.resultArgb = 0xFF00FFFF;
            rows = SourcePane::BuildRows ({ L"x", L"  lda $10", L"  .byte 1" }, 2, {}, {}, style, {},
                                          { { 2, { L"$10: 07", L"A=07" } }, { 3, { L"$01: 00", L"" } } });

            Assert::AreEqual ((size_t) 4,                       rows[1].cells.size());
            Assert::AreEqual (std::wstring (L"$10: 07  Result: A=07"), rows[1].cells[3]);
            Assert::AreEqual (std::wstring(),                   rows[2].cells[3], L"a directive is not one instruction");
            Assert::AreEqual (std::wstring(),                   rows[0].cells[3]);
            Assert::IsTrue   (std::any_of (rows[1].spans.begin(), rows[1].spans.end(), [] (const DxuiTextView::Span & span)
                              {
                                  return span.cell == 3 && span.start == 9 && span.length == 12 && span.argb == 0xFF00FFFFu;
                              }), L"the result is in the result color");
        }



        TEST_METHOD (TheTextViewPlacesACellsAnchor)
        {
            DxuiTextView  view;
            float         x      = 0.0f;
            float         y      = 0.0f;
            float         top    = 0.0f;
            float         bottom = 0.0f;



            view.SetBounds   ({ 0, 0, 400, 100 });
            view.SetCellSize (8, 16);
            view.SetRows     ({ { { L"a", L"12", L"text" } }, { { L"b", L"34", L"more" } } });

            Assert::IsTrue  (view.GetCellAnchorPx (1, 2, x, y));
            Assert::AreEqual ((float) (6 + (1 + 2 + 2 + 2) * 8), x, L"past the pad and two columns with their gaps");
            Assert::AreEqual ((float) (6 + 16 + 8),              y, L"the middle of the second line");
            Assert::IsFalse (view.GetCellAnchorPx (5, 2, x, y), L"a row that is not laid out");

            view.GetLinesSpanPx (top, bottom);
            Assert::AreEqual (6.0f,           top);
            Assert::AreEqual (6.0f + 2 * 16, bottom);
        }
    };
}
