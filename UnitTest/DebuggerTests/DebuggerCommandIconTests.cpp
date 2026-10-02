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

    static std::vector<RecordedPaintCall> Paint (int id, bool enabled)
    {
        DebuggerCommands::Handlers  handlers;
        MockDxuiPainter             painter;
        DxuiToolbarIconBox          icon;



        handlers.isDark = [] { return true; };

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

        TEST_METHOD (RunIsAGreenTriangle)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kRun, true);



            Assert::AreEqual (size_t (1), calls.size());
            Assert::IsTrue   (calls[0].kind == RecordedPaintKind::FillConvexQuad);
            Assert::AreEqual (s_kDarkGreen, calls[0].argb);
        }



        TEST_METHOD (PauseIsTwoBarsInTheInk)
        {
            std::vector<RecordedPaintCall>  calls = Paint (DebuggerCommands::kPause, true);



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
            for (int id : { DebuggerCommands::kRun, DebuggerCommands::kStepInto, DebuggerCommands::kStepOver,
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
    };
}
