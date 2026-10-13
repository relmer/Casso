#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "HandlerTestRig.h"
#include "MockDebugTarget.h"
#include "UiTests/InMemoryFileSystem.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorSlotNoteTests
    //
    //  FR-132: `n^P` and `n^K` with a slot past 7 keep the ROM's behavior and
    //  print one line saying which slot the ROM used.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorSlotNoteTests)
    {
    public:
        struct Rig
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            InMemoryFileSystem         files;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;

            Rig()
            {
                handlers.Attach (session);
                session.SetFileSystem       (&files);
                session.SetCurrentDirectory (L"C:\\Work");
                session.ExecuteLine         ("MODE MONITOR");
            }

            Reply RunOk (const std::string & line)
            {
                Reply  reply = session.ExecuteLine (line);



                session.FormatReply (reply);
                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
                return reply;
            }
        };



        static bool HasLine (const Reply & reply, const std::string & text)
        {
            for (const std::string & line : reply.text)
            {
                if (line.find (text) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        }



        TEST_METHOD (AnOutOfRangeSlot_UsesItsLowHexDigit)
        {
            Rig    rig;
            Reply  output = rig.RunOk ("10^P");
            Reply  input  = rig.RunOk ("13^K");
            Reply  nine;



            Assert::IsTrue   (HasLine (output, "There is no slot 10; only its low hex digit is used."));
            Assert::AreEqual ((int) 0xF0, (int) rig.target.memory[0x36], L"slot 0 restores the ROM's output routine, as the ROM does");

            Assert::IsTrue   (HasLine (input, "There is no slot 13; only its low hex digit is used."));
            Assert::AreEqual ((int) 0xC3, (int) rig.target.memory[0x39], L"slot 3, as the ROM does");

            nine = rig.RunOk ("9^K");

            Assert::IsTrue   (HasLine (nine, "There is no slot 9; only its low hex digit is used."));
            Assert::AreEqual ((int) 0xC9, (int) rig.target.memory[0x39], L"$C900, as the ROM does");
        }

        TEST_METHOD (AnInRangeSlot_PrintsNoNote)
        {
            Rig    rig;
            Reply  reply = rig.RunOk ("7^P");



            Assert::IsFalse (HasLine (reply, "out of range"));
        }
    };
}
