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



        TEST_METHOD (AnOutOfRangeSlot_PrintsTheSlotTheRomUsed)
        {
            Rig    rig;
            Reply  output = rig.RunOk ("10^P");
            Reply  input  = rig.RunOk ("13^K");



            Assert::IsTrue (HasLine (output, "Slot 10 is out of range; the ROM uses slot 0"));
            Assert::AreEqual ((int) 0xF0, (int) rig.target.memory[0x36], L"the ROM's behavior is kept");

            Assert::IsTrue (HasLine (input, "Slot 13 is out of range; the ROM uses slot 3"));
            Assert::AreEqual ((int) 0xC3, (int) rig.target.memory[0x39], L"the ROM's behavior is kept");
        }

        TEST_METHOD (AnInRangeSlot_PrintsNoNote)
        {
            Rig    rig;
            Reply  reply = rig.RunOk ("7^P");



            Assert::IsFalse (HasLine (reply, "out of range"));
        }
    };
}
