#include "Pch.h"

#include "ControllerRig.h"
#include "Ui/Debugger/CommandCompletion.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletionTests
//
//  The command box's gray suggestion and PowerShell-style completion
//  (FR-128, FR-131).
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CommandCompletionTests)
    {
    public:
        TEST_METHOD (ConsoleLine_HandsItsSuggestionToTheWindow)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  before = rig.view.Build (rig.controller.GetSession(), true);
            DebuggerViewSnapshot  after;



            rig.view.ExecuteConsoleLine (rig.controller.GetSession(), "nobp 0");
            after = rig.view.Build (rig.controller.GetSession(), true);

            Assert::AreEqual (std::string ("bpc 0"), after.suggestion);
            Assert::AreNotEqual (before.suggestionSerial, after.suggestionSerial, L"each line gives a new serial");
        }



        TEST_METHOD (Suggestion_ShowsGrayAndTabTakesIt)
        {
            CommandCompletion  completion;
            std::wstring       result;



            completion.OfferSuggestion (L"bpc 0", L"");

            Assert::AreEqual (std::wstring (L"bpc 0"), completion.GetGhost (L"", true, {}));
            Assert::IsTrue   (completion.TryAcceptSuggestion (L"", result));
            Assert::AreEqual (std::wstring (L"bpc 0"), result);
            Assert::AreEqual (std::wstring(), completion.GetGhost (L"", true, {}), L"taken once");
        }



        TEST_METHOD (Suggestion_AnyTypingDropsIt)
        {
            CommandCompletion  completion;
            std::wstring       result;



            completion.OfferSuggestion (L"bpc 0", L"");

            Assert::AreEqual (std::wstring(), completion.GetGhost (L"x", true, {}));
            Assert::AreEqual (std::wstring(), completion.GetGhost (L"", true, {}), L"deleting the typing does not bring it back");
            Assert::IsFalse  (completion.TryAcceptSuggestion (L"", result));
        }



        TEST_METHOD (History_NewestMatchingLineShowsGrayAndRightTakesIt)
        {
            CommandCompletion          completion;
            std::vector<std::wstring>  history = { L"bpm 400", L"bp 300", L"r" };
            std::wstring               result;



            Assert::AreEqual (std::wstring (L" 300"), completion.GetGhost (L"bp", true, history));
            Assert::AreEqual (std::wstring(),         completion.GetGhost (L"bp", false, history), L"only at the end of the line");
            Assert::IsTrue   (completion.TryAcceptHistory (L"bp", history, result));
            Assert::AreEqual (std::wstring (L"bp 300"), result);
        }



        TEST_METHOD (Tab_CompletesTheModesCommandsAndCycles)
        {
            CommandCompletion  completion;
            std::wstring       first;
            std::wstring       second;
            std::wstring       back;



            completion.SetMode (CommandMode::WinDbg);

            Assert::IsTrue   (completion.TryComplete (L"b 300", true, first));
            Assert::IsTrue   (completion.TryComplete (first, true, second));
            Assert::AreNotEqual (first, second, L"a second press steps to the next match");
            Assert::IsTrue   (first.ends_with (L" 300") && second.ends_with (L" 300"), L"the arguments are kept");
            Assert::IsTrue   (completion.TryComplete (second, false, back));
            Assert::AreEqual (first, back, L"Shift+Tab steps back");
        }



        TEST_METHOD (Tab_WordNoCommandStartsWith_CompletesNothing)
        {
            CommandCompletion  completion;
            std::wstring       result;



            completion.SetMode (CommandMode::AppleWin);
            Assert::IsFalse (completion.TryComplete (L"zzzq", true, result));
        }



        TEST_METHOD (F8_StepsBackThroughLinesStartingWithTheTypedText)
        {
            CommandCompletion          completion;
            std::vector<std::wstring>  history = { L"bp 100", L"r", L"bp 200", L"bp 300" };
            std::wstring               result;



            Assert::IsTrue   (completion.TrySearchHistory (L"bp", history, result));
            Assert::AreEqual (std::wstring (L"bp 300"), result);
            Assert::IsTrue   (completion.TrySearchHistory (result, history, result));
            Assert::AreEqual (std::wstring (L"bp 200"), result);
            Assert::IsTrue   (completion.TrySearchHistory (result, history, result));
            Assert::AreEqual (std::wstring (L"bp 100"), result);
            Assert::IsFalse  (completion.TrySearchHistory (result, history, result), L"the oldest stays put");
        }
    };
}