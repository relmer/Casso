#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/HistoryBand.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepKeyTests
//
//  Visual Studio's Alt step back keys and Ctrl+R chords, the commands those
//  keys take, and the history band's words.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseStepKeyTests)
{
public:

    using Action = DebuggerKeySchemes::Action;


    TEST_METHOD (VisualStudioStepsBackWithAlt)
    {
        for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio })
        {
            const DxuiKeyMap &  map = DebuggerKeySchemes::GetMap (scheme);
            int                 id  = 0;



            Assert::IsTrue (map.TryTranslate (VK_F11, false, true, false, id) && id == (int) Action::StepBackInto, map.GetName().c_str());
            Assert::IsTrue (map.TryTranslate (VK_F10, false, true, false, id) && id == (int) Action::StepBackOver, map.GetName().c_str());
            Assert::IsTrue (map.TryTranslate (VK_F11, false, true, true,  id) && id == (int) Action::StepBackOut,  map.GetName().c_str());
        }
    }


    TEST_METHOD (VisualStudioTakesItsCtrlRChords)
    {
        const DxuiKeyMap &  map    = DebuggerKeySchemes::GetMap (DebuggerKeyScheme::VisualStudio);
        DxuiKeyStroke       ctrlR  = { 'R', true, false, false };
        int                 id     = 0;



        Assert::IsTrue (map.Match (ctrlR, std::nullopt, id) == DxuiKeyMatch::Prefix);
        Assert::IsTrue (map.Match ({ VK_F11, false, false, false }, ctrlR, id) == DxuiKeyMatch::Command && id == (int) Action::StepBackInto);
        Assert::IsTrue (map.Match ({ VK_F10, false, false, false }, ctrlR, id) == DxuiKeyMatch::Command && id == (int) Action::StepBackOver);
        Assert::IsTrue (map.Match ({ VK_F11, false, false, true  }, ctrlR, id) == DxuiKeyMatch::Command && id == (int) Action::StepBackOut);

        Assert::AreEqual (std::wstring (L"Alt+F11 or Ctrl+R, F11"),             map.GetChordText (DebuggerCommands::kStepBackInto));
        Assert::AreEqual (std::wstring (L"Alt+F10 or Ctrl+R, F10"),             map.GetChordText (DebuggerCommands::kStepBackOver));
        Assert::AreEqual (std::wstring (L"Alt+Shift+F11 or Ctrl+R, Shift+F11"), map.GetChordText (DebuggerCommands::kStepBackOut));

        Assert::IsTrue (DebuggerKeySchemes::GetMap (DebuggerKeyScheme::AppleWin).Match (ctrlR, std::nullopt, id) == DxuiKeyMatch::None,
                        L"the chords are Visual Studio's alone");
    }


    TEST_METHOD (TheKeysTakeTheReverseCommands)
    {
        struct Case
        {
            Action        action;
            DebugVerb     verb;
            const char  * echo;
        };

        static constexpr Case  kCases[] =
        {
            { Action::StepBackInto, DebugVerb::StepBack,     "T-"  },
            { Action::StepBackOver, DebugVerb::StepBackOver, "P-"  },
            { Action::StepBackOut,  DebugVerb::StepBackOut,  "GU-" },
        };



        for (const Case & each : kCases)
        {
            std::optional<DebuggerAction>  taken = DebuggerActions::GetForKey (each.action, nullptr, -1, CommandMode::AppleWin);



            Assert::IsTrue (taken.has_value(), L"a key with an action");
            Assert::IsTrue (taken->command.verb == each.verb, L"the reverse verb");
            Assert::AreEqual (std::string (each.echo), taken->echo, L"echoed as typed");
        }

        Assert::AreEqual (std::string ("LIVE"), DebuggerActions::GetReverse (DebugVerb::GoLive,    CommandMode::AppleWin).echo);
        Assert::AreEqual (std::string ("G-"),   DebuggerActions::GetReverse (DebugVerb::ReverseGo, CommandMode::AppleWin).echo);
    }


    TEST_METHOD (TheBandShowsBehindLiveOrAfterAStopShort)
    {
        HistoryStatus  status;



        Assert::IsFalse (HistoryBand::IsShown (status), L"live, no outcome");

        status.outcome = ReverseOutcome::Moved;
        Assert::IsFalse (HistoryBand::IsShown (status), L"a move that landed is nothing to report");

        status.outcome = ReverseOutcome::HistoryCut;
        Assert::IsTrue  (HistoryBand::IsShown (status), L"a cut is reported even though the machine is live");
        Assert::IsFalse (HistoryBand::CanGoLive (status), L"but there is nowhere to go live to");

        status.outcome.reset();
        status.isBehindLive = true;
        Assert::IsTrue (HistoryBand::IsShown (status));
        Assert::IsTrue (HistoryBand::CanGoLive (status));
    }


    TEST_METHOD (TheBandSaysHowFarBehindAndWhatHappened)
    {
        HistoryStatus  status;



        status.isBehindLive       = true;
        status.instructionsBehind = 1234567;
        status.cyclesBehind       = 2040968;

        Assert::AreEqual (std::wstring (L"1,234,567 instructions (2.00 s) behind live."), HistoryBand::GetText (status));

        status.instructionsBehind = 1;
        status.cyclesBehind       = 2041;
        Assert::AreEqual (std::wstring (L"1 instruction (2.0 ms) behind live."), HistoryBand::GetText (status));

        status.cyclesBehind       = 3;
        Assert::AreEqual (std::wstring (L"1 instruction (0.003 ms) behind live."), HistoryBand::GetText (status), L"under a millisecond, to three places");

        Assert::AreEqual (std::wstring (L"Behind live"), HistoryBand::GetShortText (status), L"the narrow pane's text");

        status.outcome = ReverseOutcome::AtHistoryStart;
        Assert::IsTrue (HistoryBand::GetText (status).starts_with (L"Stopped at the start of the recorded history"), HistoryBand::GetText (status).c_str());
        Assert::IsTrue (HistoryBand::GetText (status).ends_with   (L"behind live."), L"and how far behind, after it");
        Assert::AreEqual (std::wstring (L"Start of history. 1 instruction (0.003 ms) behind live."), HistoryBand::GetCompactText (status), L"the compact text");
        Assert::AreEqual (std::wstring (L"Start of history"), HistoryBand::GetShortText (status));

        for (ReverseOutcome outcome : { ReverseOutcome::AtHistoryStart, ReverseOutcome::AtHistoryGap, ReverseOutcome::HistoryCut, ReverseOutcome::NoCaller })
        {
            Assert::IsFalse (HistoryBand::GetOutcomeText (outcome).empty(), L"every stop short is put in words");
        }

        Assert::IsTrue (HistoryBand::GetOutcomeText (ReverseOutcome::AtHistoryGap).find (L"Maximum speed") != std::wstring::npos);
        Assert::IsTrue (HistoryBand::GetOutcomeText (ReverseOutcome::HistoryCut).find (L"live") != std::wstring::npos);
    }
};