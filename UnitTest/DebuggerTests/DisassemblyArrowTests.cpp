#include "Pch.h"

#include "ControllerRig.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "../Dxui/MockDxuiPainter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyArrowTests
//
//  The branch arrow with an end scrolled out of view, whether the PC's branch
//  will be taken, a press on the arrow, and the operand and result sharing
//  one column.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DisassemblyArrowTests)
    {
    public:

        static bool IsNear (float a, float b)
        {
            return std::fabs (a - b) < 0.01f;
        }



        static BranchArrow::Input MakeInput (std::optional<float> sourceY, std::optional<float> targetY, bool isTargetBelow)
        {
            BranchArrow::Input  input;



            input.mnemonicX     = 100.0f;
            input.sourceY       = sourceY;
            input.targetY       = targetY;
            input.isTargetBelow = isTargetBelow;
            input.edgeY         = isTargetBelow ? 300.0f : 10.0f;
            input.sourceEdgeY   = isTargetBelow ? 10.0f  : 300.0f;
            return input;
        }



        TEST_METHOD (ASourceOutOfViewEntersFromItsEdgeAndPointsAtTheTarget)
        {
            BranchArrow::Input   input  = MakeInput (std::nullopt, 150.0f, true);
            BranchArrow::Result  result = BranchArrow::Build (input);



            Assert::IsFalse (result.segments.empty(), L"the arrow is still drawn");
            Assert::IsTrue  (IsNear (result.segments.front().y0, 10.0f), L"from the top of the rows");
            Assert::IsTrue  (result.hasHead, L"into the target");
            Assert::IsTrue  (IsNear (result.head[1], 150.0f));
        }



        TEST_METHOD (BothEndsOutOfViewRunEdgeToEdge)
        {
            BranchArrow::Result  result = BranchArrow::Build (MakeInput (std::nullopt, std::nullopt, false));



            Assert::AreEqual ((size_t) 1, result.segments.size());
            Assert::IsTrue   (IsNear (result.segments[0].y0, 300.0f) && IsNear (result.segments[0].y1, 10.0f));
            Assert::IsFalse  (result.hasHead);
        }



        //  The arrow of the branch that led to the PC is dotted: round dots as
        //  wide as the line, two widths apart, in the same color; the PC's own
        //  is one line a segment.
        TEST_METHOD (TheSteppedArrowIsDottedAndThePcsIsSolid)
        {
            BranchArrow::Input   input  = MakeInput (50.0f, 150.0f, true);
            BranchArrow::Result  arrow  = BranchArrow::Build (input);
            MockDxuiPainter      solid;
            MockDxuiPainter      dotted;
            size_t               lines  = 0;
            float                width  = 1.5f;



            DisassemblyCells::PaintBranchArrow (solid,  input, true, 0xFFFFE34D, 0xFF808080);
            DisassemblyCells::PaintBranchArrow (dotted, input, true, 0xFFFFE34D, 0xFF808080, true);

            for (const RecordedPaintCall & call : solid.Calls())
            {
                lines += (call.kind == RecordedPaintKind::DrawLine) ? 1 : 0;
            }

            Assert::AreEqual (arrow.segments.size(), lines, L"solid: a line a segment");

            lines = 0;

            for (const RecordedPaintCall & call : dotted.Calls())
            {
                Assert::IsTrue (call.kind != RecordedPaintKind::DrawLine, L"no line, only dots");

                if (call.kind == RecordedPaintKind::FillCircle)
                {
                    lines++;
                    Assert::AreEqual (width,      call.width, 0.01f, L"a dot as wide as the line");
                    Assert::AreEqual (0xFFFFE34Du, call.argb,         L"the taken color");
                }
            }

            Assert::IsTrue (lines > 20, L"dotted: many dots along the arrow's hundred pixels");
        }


        TEST_METHOD (APressOnTheUprightHitsTheArrowAndOneAwayDoesNot)
        {
            BranchArrow::Input  input   = MakeInput (50.0f, 150.0f, true);
            float               upright = input.mnemonicX - input.marginPx - input.stubPx;



            Assert::IsTrue  (BranchArrow::HitTest (input, upright, 100.0f), L"on the upright");
            Assert::IsTrue  (BranchArrow::HitTest (input, upright + 6.0f, 150.0f), L"on the run into the head");
            Assert::IsFalse (BranchArrow::HitTest (input, upright, 200.0f), L"past the target");
            Assert::IsFalse (BranchArrow::HitTest (input, upright + 10.0f, 100.0f), L"beside the upright");
        }



        static bool IsTakenWithZ (bool z)
        {
            ControllerRig         rig;
            Cpu6502Registers      r = rig.controller.GetSession().GetTarget().GetRegisters();
            DebuggerViewSnapshot  snapshot;



            rig.machine.GetMemoryBus().WriteByte (0x0300, 0xF0);    // BEQ $0310
            rig.machine.GetMemoryBus().WriteByte (0x0301, 0x0E);

            r.p = (Byte) (z ? (r.p | 0x02) : (r.p & ~0x02));
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession());

            for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
            {
                if (line.isCurrent)
                {
                    Assert::IsTrue (line.target == std::optional<Word> (0x0310));
                    return line.isTargetTaken;
                }
            }

            Assert::Fail (L"no PC line");
            return false;
        }



        TEST_METHOD (APcBranchTheFlagsWillNotTakeIsMarkedNotTaken)
        {
            Assert::IsTrue  (IsTakenWithZ (true),  L"BEQ with Z set is taken");
            Assert::IsFalse (IsTakenWithZ (false), L"BEQ with Z clear is not");
        }



        //  The branch, jump or call that brought the machine to the PC, by a
        //  step or by a run of several instructions, is marked, so its arrow
        //  is drawn to the PC; a PC the machine moved on to in order marks
        //  nothing.
        TEST_METHOD (TheTransferThatBroughtThePcIsMarked)
        {
            ControllerRig         rig;
            Cpu6502Registers      r        = rig.controller.GetSession().GetTarget().GetRegisters();
            DebuggerViewSnapshot  snapshot;
            bool                  isMarked = false;



            rig.machine.GetMemoryBus().WriteByte (0x0300, 0xF0);    // BEQ $0310
            rig.machine.GetMemoryBus().WriteByte (0x0301, 0x0E);
            rig.machine.GetMemoryBus().WriteByte (0x0310, 0xEA);    // NOP
            rig.machine.GetMemoryBus().WriteByte (0x0311, 0x4C);    // JMP $0320
            rig.machine.GetMemoryBus().WriteByte (0x0312, 0x20);
            rig.machine.GetMemoryBus().WriteByte (0x0313, 0x03);
            rig.machine.GetMemoryBus().WriteByte (0x0320, 0xEA);    // NOP

            r.pc = 0x0300;
            r.p  = (Byte) (r.p | 0x02);
            rig.controller.GetSession().GetTarget().SetRegisters (r);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (snapshot.branchedFrom.has_value(), L"no transfer to $0300");

            rig.machine.StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (snapshot.branchedFrom == std::optional<Word> (0x0300), L"the BEQ was taken to the PC");

            for (const DebuggerViewSnapshot::CodeLine & line : snapshot.code)
            {
                isMarked = isMarked || (line.isBranchedFrom && line.address == 0x0300);
            }

            Assert::IsTrue (isMarked, L"its line is marked");

            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsTrue (snapshot.branchedFrom == std::optional<Word> (0x0300), L"the same stop, built again");

            //  A run of two instructions with no build between, as a run that
            //  a breakpoint stops: the JMP brought the machine to $0320.
            rig.machine.StepOne();
            rig.machine.StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((int) 0x0320, (int) snapshot.pc);
            Assert::IsTrue   (snapshot.branchedFrom == std::optional<Word> (0x0311), L"the JMP, found with no stop at it");

            rig.machine.StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse (snapshot.branchedFrom.has_value(), L"the PC moved on in order from the JMP's target, so nothing is marked");
        }



        TEST_METHOD (TheResultFollowsTheOperandInTheResultColor)
        {
            DxuiListView::Cell  cell = DisassemblyCells::GetOperandAndResultCell ("A=00", "A=41", 0xFF00FFFF);



            Assert::AreEqual (std::wstring (L"A=00  Result: A=41"), cell.text);
            Assert::AreEqual ((size_t) 1, cell.colorRanges.size());
            Assert::AreEqual (6,  std::get<0> (cell.colorRanges[0]));
            Assert::AreEqual (18, std::get<1> (cell.colorRanges[0]));
        }



        TEST_METHOD (AResultAloneTakesTheWholeCell)
        {
            DxuiListView::Cell  cell = DisassemblyCells::GetOperandAndResultCell ("", "A=41", 0xFF00FFFF);



            Assert::AreEqual (std::wstring (L"Result: A=41"), cell.text);
            Assert::AreEqual ((size_t) 1, cell.colorRanges.size());
            Assert::AreEqual (0, std::get<0> (cell.colorRanges[0]));
        }
    };
}
