#include "Pch.h"

#include "ControllerRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSuggestionTests
//
//  A word of another mode is answered with that mode and this mode's
//  equivalent, a word no mode has with the closest command by spelling, and
//  wrong arguments with the command's syntax line.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CommandSuggestionTests)
    {
    public:
        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        TEST_METHOD (GSSquaredWordInWinDbg_SuggestsWinDbgsEquivalentWithTheSameEffect)
        {
            ControllerRig  rig;
            Reply          reply;
            Reply          accepted;



            rig.Run ("bp 300", CommandMode::WinDbg);
            reply = rig.Run ("nobp 0", CommandMode::WinDbg);

            Assert::IsTrue   (reply.status == CommandStatus::Unknown, Widen (reply.error.label + ": " + reply.error.detail).c_str());
            Assert::AreEqual (std::string ("nobp is a GSSquared command. WinDbg's is bc 0."), reply.error.detail);
            Assert::AreEqual (std::string ("bc 0"), reply.suggestion);

            accepted = rig.Run (reply.suggestion, CommandMode::WinDbg);

            Assert::IsTrue (accepted.status == CommandStatus::Ok, Widen (accepted.error.label + ": " + accepted.error.detail).c_str());
            Assert::IsTrue (rig.controller.GetSession().GetBreakpoints().GetAll().empty(), L"the suggestion has the same effect");
        }



        TEST_METHOD (GSSquaredWordInAppleWin_SuggestsAppleWinsEquivalent)
        {
            ControllerRig  rig;
            Reply          reply;



            reply = rig.Run ("nobp 0", CommandMode::AppleWin);

            Assert::AreEqual (std::string ("nobp is a GSSquared command. AppleWin's is bpc 0."), reply.error.detail);
            Assert::AreEqual (std::string ("bpc 0"), reply.suggestion);
        }



        TEST_METHOD (AppleWinWordInWinDbg_SuggestsWinDbgsEquivalent)
        {
            ControllerRig  rig;
            Reply          reply;



            reply = rig.Run ("bpc 0", CommandMode::WinDbg);

            Assert::AreEqual (std::string ("bpc is an AppleWin command. WinDbg's is bc 0."), reply.error.detail);
            Assert::AreEqual (std::string ("bc 0"), reply.suggestion);
        }



        TEST_METHOD (WordWithNoEquivalent_SaysSoAndSuggestsNothing)
        {
            ControllerRig  rig;
            Reply          reply;



            reply = rig.Run ("ea 300 \"hi\"", CommandMode::AppleWin);

            Assert::AreEqual (std::string ("ea is a WinDbg command. AppleWin has no equivalent."), reply.error.detail);
            Assert::IsTrue   (reply.suggestion.empty());
        }



        TEST_METHOD (WordNoModeHas_SuggestsTheClosestBySpelling)
        {
            ControllerRig  rig;
            Reply          reply;



            reply = rig.Run ("nbop 1", CommandMode::GSSquared);

            Assert::IsTrue   (reply.status == CommandStatus::Unknown);
            Assert::AreEqual (std::string ("nbop is not a command. The closest is nobp 1."), reply.error.detail);
            Assert::AreEqual (std::string ("nobp 1"), reply.suggestion);
        }



        TEST_METHOD (WordFarFromEveryCommand_SuggestsNothing)
        {
            ControllerRig  rig;
            Reply          reply;



            reply = rig.Run ("qqqqqqqqqq", CommandMode::GSSquared);

            Assert::IsTrue (reply.status == CommandStatus::Unknown);
            Assert::IsTrue (reply.suggestion.empty());
        }



        TEST_METHOD (WrongArguments_ShowTheCommandsSyntaxLine)
        {
            ControllerRig  rig;
            Reply          appleWin;
            Reply          gsSquared;
            std::string    text;



            appleWin  = rig.Run ("budget C000");
            gsSquared = rig.Run ("nobp", CommandMode::GSSquared);

            Assert::AreEqual (std::string ("BUDGET cycles"), appleWin.error.usage);
            Assert::AreEqual (std::string ("nobp id|addr"),  gsSquared.error.usage);

            for (const std::string & line : appleWin.text)
            {
                text += line + "\n";
            }

            Assert::IsTrue (text.find ("Usage: BUDGET cycles") != std::string::npos, Widen (text).c_str());
        }
    };
}
