#include "Pch.h"

#include "ControllerRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeConfirmationTests
//
//  Every command that changes the machine or the debugger's tables prints
//  one line saying what changed, in every mode.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ChangeConfirmationTests)
    {
    public:
        static std::string Joined (const Reply & reply)
        {
            std::string  text;



            for (const std::string & line : reply.text)
            {
                text += line + "\n";
            }

            return text;
        }



        TEST_METHOD (GSSquaredNobp_SaysWhichBreakpointWasRemoved)
        {
            ControllerRig  rig;
            Reply          clear;



            rig.Run ("bp 300", CommandMode::GSSquared);
            clear = rig.Run ("nobp 0", CommandMode::GSSquared);

            Assert::AreEqual (std::string ("Breakpoint #0 cleared.\n"), Joined (clear));
        }



        TEST_METHOD (GSSquaredNowatch_SaysWhichWatchWasRemoved)
        {
            ControllerRig  rig;
            Reply          clear;



            rig.Run ("watch 400", CommandMode::GSSquared);
            clear = rig.Run ("nowatch 0", CommandMode::GSSquared);

            Assert::AreEqual (std::string ("Watch #0 cleared.\n"), Joined (clear));
        }



        TEST_METHOD (GSSquaredDeposit_SaysWhatWasWritten)
        {
            ControllerRig  rig;
            Reply          deposit;



            deposit = rig.Run ("400: 1 2", CommandMode::GSSquared);

            Assert::AreEqual (std::string ("Wrote 2 bytes at 00/0400\n"), Joined (deposit));
        }



        TEST_METHOD (Budget_SaysWhatWasSetAndRemoved)
        {
            ControllerRig  rig;
            Reply          set;
            Reply          removed;



            set     = rig.Run ("budget 100");
            removed = rig.Run ("budget 0");

            Assert::AreEqual (std::string ("Budget set to 100 cycles.\n"), Joined (set));
            Assert::AreEqual (std::string ("Budget removed.\n"),          Joined (removed));
        }
    };
}
