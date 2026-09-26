#include "Pch.h"

#include "Ui/Debugger/BranchArrow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BranchArrowTests
//
//  The line from the PC's branch to its target: off the mnemonic, round a
//  corner, along the left of the mnemonics, round again, into an arrowhead
//  that stops short of the target's mnemonic.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BranchArrowTests)
    {
    public:

        static BranchArrow::Input MakeInput (float sourceY, std::optional<float> targetY, bool isTargetBelow, float edgeY = 0.0f)
        {
            BranchArrow::Input  input;



            input.mnemonicX     = 100.0f;
            input.sourceY       = sourceY;
            input.targetY       = targetY;
            input.isTargetBelow = isTargetBelow;
            input.edgeY         = edgeY;
            return input;
        }



        static bool IsNear (float a, float b)
        {
            return std::fabs (a - b) < 0.01f;
        }



        TEST_METHOD (ADownwardBranchLeavesItsMnemonicAndPointsAtTheTarget)
        {
            BranchArrow::Input   input  = MakeInput (50.0f, 150.0f, true);
            BranchArrow::Result  result = BranchArrow::Build (input);
            float                end    = input.mnemonicX - input.marginPx;
            float                x      = end - input.stubPx;
            bool                 hasRun = false;



            Assert::IsTrue (IsNear (result.segments.front().x0, end) && IsNear (result.segments.front().y0, 50.0f), L"starts left of the mnemonic, with a margin");

            for (const BranchArrow::Segment & segment : result.segments)
            {
                hasRun |= IsNear (segment.x0, x) && IsNear (segment.x1, x) && segment.y1 > segment.y0 + 90.0f;
                Assert::IsTrue (segment.x0 <= end + 0.01f && segment.x1 <= end + 0.01f, L"nothing crosses into the mnemonics");
            }

            Assert::IsTrue (hasRun, L"a straight run down the left");
            Assert::IsTrue (result.segments.back().x1 > result.segments.back().x0 + 1.0f, L"a run into the head, past the second corner");
            Assert::IsTrue (IsNear (result.segments.back().y1, 150.0f), L"ends on the target's row");
            Assert::IsTrue (result.hasHead);
            Assert::IsTrue (IsNear (result.head[0], end) && IsNear (result.head[1], 150.0f), L"the head's tip stops short of the target's mnemonic");
            Assert::IsTrue (result.head[2] < result.head[0], L"and points right, at it");
        }



        TEST_METHOD (AnUpwardBranchTurnsUp)
        {
            BranchArrow::Result  result = BranchArrow::Build (MakeInput (150.0f, 50.0f, false));



            Assert::IsTrue (result.segments[1].y1 < 150.0f, L"the first corner turns up");
            Assert::IsTrue (IsNear (result.segments.back().y1, 50.0f));
            Assert::IsTrue (result.hasHead);
        }



        TEST_METHOD (ATargetOutOfViewRunsToTheEdgeWithoutAHead)
        {
            BranchArrow::Result  result = BranchArrow::Build (MakeInput (50.0f, std::nullopt, true, 300.0f));



            Assert::IsTrue  (IsNear (result.segments.back().y1, 300.0f), L"to the bottom of the rows");
            Assert::IsFalse (result.hasHead);
        }



        TEST_METHOD (ABranchToItselfHasNoLine)
        {
            BranchArrow::Result  result = BranchArrow::Build (MakeInput (50.0f, 50.0f, true));



            Assert::IsTrue  (result.segments.empty());
            Assert::IsFalse (result.hasHead);
        }
    };
}
