#include "Pch.h"

#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerIcons.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCommandIconTests
{
    static constexpr uint32_t  s_kDarkGreen  = 0xFF6CCB5Fu;
    static constexpr uint32_t  s_kDarkBlue   = 0xFF4FA8E8u;
    static constexpr uint32_t  s_kLightGreen = 0xFF1F883Du;
    static constexpr uint32_t  s_kLightBlue  = 0xFF1A6FC4u;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindEntry
    //
    //  One command's entry on the command bar.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static DxuiToolbar::Entry FindEntry (int id, bool isRunning = false, bool isDark = true)
    {
        DebuggerCommands::Handlers  handlers;
        DxuiToolbar::Entry          found;



        handlers.isDark    = [isDark] { return isDark; };
        handlers.isEnabled = [isRunning] (int each) { return each != DebuggerCommands::kPause || isRunning; };

        //  The entries' callbacks capture the commands, which outlive the test.
        static std::vector<std::unique_ptr<DebuggerCommands>>  s_commands;

        s_commands.push_back (std::make_unique<DebuggerCommands> (std::move (handlers)));

        for (const DxuiToolbar::Entry & entry : s_commands.back()->BuildEntries())
        {
            if (entry.command != nullptr && entry.command->id == id)
            {
                found = entry;
            }
        }

        return found;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CountLayers
    //
    //  How many of an icon's layers are drawn in the accent, or in the ink.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static size_t CountLayers (const DxuiVectorIcon & icon, bool accent)
    {
        size_t  count = 0;



        for (size_t i = 0; i < icon.layerCount; i++)
        {
            count += (icon.layers[i].accent == accent) ? 1 : 0;
        }

        return count;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IsIcon
    //
    //  The icons are static data, so each translation unit has a copy: two are
    //  the same icon when their path data is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static bool IsIcon (const DxuiVectorIcon * actual, const DxuiVectorIcon & other)
    {
        return actual != nullptr && wcscmp (actual->pathData, other.pathData) == 0;
    }





    TEST_CLASS (DebuggerCommandIconTests)
    {
    public:

        //  The bar's one Run or Pause button is Run while the machine is
        //  stopped.
        TEST_METHOD (RunIsAGreenTriangle)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kRunOrPause);



            Assert::IsTrue   (static_cast<bool> (entry.getVectorIcon));
            Assert::IsTrue   (IsIcon (entry.getVectorIcon(), DebuggerIcons::s_kRun));
            Assert::AreEqual (s_kDarkGreen, entry.getAccent());
            Assert::AreEqual (size_t (1), CountLayers (DebuggerIcons::s_kRun, true));
            Assert::AreEqual (size_t (0), CountLayers (DebuggerIcons::s_kRun, false));
        }



        //  And Pause while it runs.
        TEST_METHOD (PauseIsTwoBarsInTheInk)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kRunOrPause, true);



            Assert::IsTrue   (IsIcon (entry.getVectorIcon(), DebuggerIcons::s_kPause));
            Assert::AreEqual (size_t (0), CountLayers (DebuggerIcons::s_kPause, true));
            Assert::AreEqual (uint32_t (0x03u), DebuggerIcons::s_kPause.layers[0].subpaths);
        }



        TEST_METHOD (StepsAreBlueArrowsWithADot)
        {
            struct Case { int id; const DxuiVectorIcon * icon; };

            Case  cases[] =
            {
                { DebuggerCommands::kStepInto,     &DebuggerIcons::s_kStepInto     },
                { DebuggerCommands::kStepOver,     &DebuggerIcons::s_kStepOver     },
                { DebuggerCommands::kStepOut,      &DebuggerIcons::s_kStepOut      },
                { DebuggerCommands::kStepBackInto, &DebuggerIcons::s_kStepBackInto },
                { DebuggerCommands::kStepBackOver, &DebuggerIcons::s_kStepBackOver },
                { DebuggerCommands::kStepBackOut,  &DebuggerIcons::s_kStepBackOut  },
            };



            for (const Case & each : cases)
            {
                DxuiToolbar::Entry  entry = FindEntry (each.id);

                Assert::IsTrue   (IsIcon (entry.vectorIcon, *each.icon));
                Assert::AreEqual (s_kDarkBlue, entry.getAccent());
                Assert::AreEqual (size_t (1), CountLayers (*each.icon, true));
                Assert::AreEqual (size_t (0), CountLayers (*each.icon, false));
            }
        }



        TEST_METHOD (ReverseContinueIsAGreenTriangle)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kReverseContinue);



            Assert::IsTrue   (IsIcon (entry.vectorIcon, DebuggerIcons::s_kReverseRun));
            Assert::AreEqual (s_kDarkGreen, entry.getAccent());
        }



        TEST_METHOD (TheColorsAreDeeperOnALightGround)
        {
            Assert::AreEqual (s_kLightGreen, FindEntry (DebuggerCommands::kRunOrPause, false, false).getAccent());
            Assert::AreEqual (s_kLightBlue,  FindEntry (DebuggerCommands::kStepInto,   false, false).getAccent());
        }



        //  Run to cursor's arrow is in the steps' color and its bar in the ink.
        TEST_METHOD (RunToCursorIsABlueArrowAndABarInTheInk)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kRunToCursor);



            Assert::IsTrue   (IsIcon (entry.vectorIcon, DebuggerIcons::s_kRunToCursor));
            Assert::AreEqual (s_kDarkBlue, entry.getAccent());
            Assert::AreEqual (size_t (1), CountLayers (DebuggerIcons::s_kRunToCursor, true));
            Assert::AreEqual (size_t (1), CountLayers (DebuggerIcons::s_kRunToCursor, false));
        }



        TEST_METHOD (ShowNextIsAnArrowInTheInk)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kShowNext);



            Assert::IsTrue   (IsIcon (entry.vectorIcon, DebuggerIcons::s_kShowNext));
            Assert::AreEqual (size_t (0), CountLayers (DebuggerIcons::s_kShowNext, true));
        }



        //  Load file into memory is a two-by-two grid of digits: the 1s in
        //  the steps' color, and the 0s, each an outline around a hole, in the
        //  ink.
        TEST_METHOD (LoadFileIsAGridOfOnesAndZeros)
        {
            DxuiToolbar::Entry      entry = FindEntry (DebuggerCommands::kLoadFile);
            const DxuiVectorIcon  & icon  = DebuggerIcons::s_kLoadFile;



            Assert::IsTrue   (IsIcon (entry.vectorIcon, icon));
            Assert::AreEqual (size_t (2), icon.layerCount);
            Assert::IsTrue   (icon.layers[0].accent,  L"the 1s in the steps' color");
            Assert::IsFalse  (icon.layers[1].accent,  L"the 0s in the ink");
            Assert::AreEqual (uint32_t (0x03u), icon.layers[0].subpaths, L"two 1s, each a bar");
            Assert::AreEqual (uint32_t (0x3Cu), icon.layers[1].subpaths, L"two 0s, each an outline and its hole");
        }



        //  Trace is a toggle drawn as a glyph.
        TEST_METHOD (TraceIsAGlyph)
        {
            DxuiToolbar::Entry  entry = FindEntry (DebuggerCommands::kTrace);



            Assert::IsTrue (entry.vectorIcon == nullptr);
            Assert::IsFalse (static_cast<bool> (entry.getVectorIcon));
        }
    };
}
