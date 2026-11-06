#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "Ui/Debugger/DebuggerCommands.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCommandIconTests
{
    static constexpr uint32_t  s_kInk       = 0xFF808080u;
    static constexpr uint32_t  s_kDarkGreen = 0xFF6CCB5Fu;
    static constexpr uint32_t  s_kDarkBlue  = 0xFF4FA8E8u;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Paint
    //
    //  The calls one command bar entry's icon makes, on a dark ground.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::vector<RecordedPaintCall> Paint (int id, bool enabled, bool isRunning = false)
    {
        DebuggerCommands::Handlers  handlers;
        MockDxuiPainter             painter;
        DxuiToolbarIconBox          icon;



        handlers.isDark    = [] { return true; };
        handlers.isEnabled = [isRunning] (int each) { return each != DebuggerCommands::kPause || isRunning; };

        DebuggerCommands                 commands (std::move (handlers));
        std::vector<DxuiToolbar::Entry>  entries  = commands.BuildEntries();

        icon.x       = 0.0f;
        icon.top     = 0.0f;
        icon.size    = 16.0f;
        icon.rowH    = 24.0f;
        icon.ink     = s_kInk;
        icon.enabled = enabled;

        for (const DxuiToolbar::Entry & entry : entries)
        {
            if (entry.command != nullptr && entry.command->id == id && entry.icon)
            {
                entry.icon (painter, icon);
            }
        }

        return painter.Calls();
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CountIn
    //
    ////////////////////////////////////////////////////////////////////////////////

    static size_t CountIn (const std::vector<RecordedPaintCall> & calls, uint32_t argb)
    {
        return (size_t) std::count_if (calls.begin(), calls.end(), [argb] (const RecordedPaintCall & c) { return c.argb == argb; });
    }





    TEST_CLASS (DebuggerCommandIconTests)
    {
    public:

        //  The bar's one Run or Pause button is Run while the machine is
        //  stopped.
        TEST_METHOD (RunIsAGreenTriangle)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kRunOrPause, true);



            Assert::AreEqual (size_t (1), calls.size());
            Assert::IsTrue   (calls[0].kind == RecordedPaintKind::FillConvexQuad);
            Assert::AreEqual (s_kDarkGreen, calls[0].argb);
        }



        //  And Pause while it runs.
        TEST_METHOD (PauseIsTwoBarsInTheInk)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kRunOrPause, true, true);



            Assert::AreEqual (size_t (2), calls.size());
            Assert::AreEqual (size_t (2), CountIn (calls, s_kInk));
        }



        TEST_METHOD (StepsAreBlueArrowsWithADot)
        {
            for (int id : { DebuggerCommands::kStepInto, DebuggerCommands::kStepOver, DebuggerCommands::kStepOut })
            {
                std::vector<RecordedPaintCall>  calls = Paint (id, true);
                size_t                          dots  = (size_t) std::count_if (calls.begin(), calls.end(),
                                                            [] (const RecordedPaintCall & c) { return c.kind == RecordedPaintKind::FillCircle; });

                Assert::IsFalse  (calls.empty());
                Assert::AreEqual (calls.size(), CountIn (calls, s_kDarkBlue));
                Assert::AreEqual (size_t (1), dots);
            }
        }



        TEST_METHOD (DisabledIconsTakeTheDimmedInk)
        {
            for (int id : { DebuggerCommands::kRunOrPause, DebuggerCommands::kStepInto, DebuggerCommands::kStepOver,
                            DebuggerCommands::kStepOut, DebuggerCommands::kRunToCursor })
            {
                std::vector<RecordedPaintCall>  calls = Paint (id, false);

                Assert::IsFalse  (calls.empty());
                Assert::AreEqual (calls.size(), CountIn (calls, s_kInk));
            }
        }



        TEST_METHOD (ShowNextIsAnArrowInTheInk)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kShowNext, true);



            Assert::AreEqual (size_t (3), calls.size());
            Assert::AreEqual (size_t (3), CountIn (calls, s_kInk));
        }



        //  Load file into memory is a two-by-two grid of digits: a 1 at the
        //  top left and the bottom right in the steps' color, and a 0 at the
        //  other two corners in the ink.
        TEST_METHOD (LoadFileIsAGridOfOnesAndZeros)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kLoadFile, true);
            std::vector<RecordedPaintCall>  ones;
            std::vector<RecordedPaintCall>  zeros;



            for (const RecordedPaintCall & call : calls)
            {
                if (call.kind == RecordedPaintKind::DrawLine)
                {
                    ones.push_back (call);
                }
                else if (call.kind == RecordedPaintKind::OutlineRoundedRect)
                {
                    zeros.push_back (call);
                }
            }

            Assert::AreEqual (size_t (4), calls.size(), L"four digits");
            Assert::AreEqual (size_t (2), ones.size(),  L"two 1s, each a stroke");
            Assert::AreEqual (size_t (2), zeros.size(), L"two 0s, each an outline");

            for (const RecordedPaintCall & one : ones)
            {
                Assert::AreEqual (s_kDarkBlue, one.argb, L"a 1 in the steps' color");
            }

            for (const RecordedPaintCall & zero : zeros)
            {
                Assert::AreEqual (s_kInk, zero.argb, L"a 0 in the ink");
            }

            Assert::IsTrue (ones[0].x < zeros[0].x && ones[0].y < zeros[1].y, L"the first 1 at the top left");
            Assert::IsTrue (ones[1].x > zeros[1].x && ones[1].y > zeros[0].y, L"the second 1 at the bottom right");
            Assert::IsTrue (zeros[0].x > zeros[1].x && zeros[0].y < zeros[1].y, L"a 0 at the top right and one at the bottom left");
        }
    };
}
