#include "Pch.h"

#include "Debugger/Handlers/TraceHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlersTests
//
//  HISTORY over the mock target: switching the trace, the window a command
//  reports, and HISTORY SAVE through the mock file system.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (TraceHandlersTests)
    {
    public:

        using Rig = HandlerRig<TraceHandlers>;

        TestCpu  cpu;

        //  count NOPs at $0300 on, each one cycle after the last.
        void Fill (Rig & rig, size_t count)
        {
            TraceRecord  record;



            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();

            for (size_t i = 0; i < count; i++)
            {
                record        = TraceRecord();
                record.cycles = i * 3;
                record.pc     = (Word) (0x0300 + i);
                record.opcode = 0xEA;
                rig.target.trace.push_back (record);
            }
        }

        static const TraceData & GetData (const Reply & reply)
        {
            const TraceData  * data = std::get_if<TraceData> (&reply.data);



            Assert::IsNotNull (data);
            return *data;
        }



        TEST_METHOD (OnAndOff_SwitchTheTarget)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Trace on."), rig.RunOk ("HISTORY ON").text.at (0));
            Assert::IsTrue   (rig.target.traceOn);

            Fill (rig, 3);
            Assert::AreEqual (std::string ("Trace off, 3 entries retained."), rig.RunOk ("HISTORY OFF").text.at (0));
            Assert::IsFalse  (rig.target.traceOn);
        }



        TEST_METHOD (OneEntry_IsSingular)
        {
            Rig  rig;



            rig.RunOk ("HISTORY ON");
            Fill (rig, 1);
            Assert::AreEqual (std::string ("Saved 1 trace entry to C:\\Work\\trace.txt."), rig.RunOk ("HISTORY SAVE trace.txt").text.at (0));
            Assert::AreEqual (std::string ("Trace off, 1 entry retained."),      rig.RunOk ("HISTORY OFF").text.at (0));
        }


        //  Each entry's cycle is three times its place in the trace, so a cycle
        //  between two entries starts at the later one.
        TEST_METHOD (Window_NewestByDefault_FromACycleWhenGiven)
        {
            Rig      rig;
            Reply    reply;



            Fill (rig, 50);

            reply = rig.RunOk ("HISTORY");
            Assert::AreEqual ((uint64_t) 50,                          GetData (reply).total);
            Assert::AreEqual ((size_t) TraceHandlers::kDefaultCount,  GetData (reply).entries.size());
            Assert::AreEqual ((uint64_t) 90,                          GetData (reply).entries.front().cycles);
            Assert::AreEqual ((uint64_t) 147,                         GetData (reply).entries.back().cycles);
            Assert::AreEqual (std::string ("NOP"),                    GetData (reply).entries.back().instruction);

            reply = rig.RunOk ("HISTORY 135 10");
            Assert::AreEqual ((size_t) 5,    GetData (reply).entries.size(), L"cut at the newest entry");
            Assert::AreEqual ((uint64_t) 135, GetData (reply).entries.front().cycles);

            reply = rig.RunOk ("HISTORY 134 1");
            Assert::AreEqual ((uint64_t) 135, GetData (reply).entries.front().cycles, L"the first entry at or after the cycle");

            reply = rig.RunOk ("HISTORY 0 1");
            Assert::AreEqual ((uint64_t) 0,   GetData (reply).entries.front().cycles, L"the oldest");

            reply = rig.RunOk ("HISTORY 148");
            Assert::AreEqual ((size_t) 0,    GetData (reply).entries.size(), L"nothing past the newest");
        }



        TEST_METHOD (Save_WritesEveryEntry)
        {
            Rig                       rig;
            std::string               saved;
            size_t                    lines = 0;



            Fill (rig, 25);

            Assert::AreEqual (std::string ("Saved 25 trace entries to C:\\Work\\trace.txt."), rig.RunOk ("HISTORY SAVE trace.txt").text.at (0));

            saved = rig.files.PeekContent (L"C:\\Work\\trace.txt");
            lines = (size_t) std::count (saved.begin(), saved.end(), '\n');

            Assert::AreEqual ((size_t) 25, lines, L"every entry, not the default window");
            Assert::IsTrue   (saved.starts_with ("           0  0300"));
        }



        TEST_METHOD (Save_EmptyTraceOrNoFiles_Fails)
        {
            Rig  rig;



            rig.RunFails ("HISTORY SAVE trace.txt", "no trace");

            Fill (rig, 1);
            rig.session.SetFileSystem (nullptr);
            rig.RunFails ("HISTORY SAVE trace.txt", "no file access");
        }



        TEST_METHOD (MachineSwitch_ClearsTheTargetTrace)
        {
            Rig  rig;



            Fill (rig, 4);
            rig.session.OnMachineChanged ("Apple ][+");

            Assert::AreEqual (1,          rig.target.traceClears);
            Assert::AreEqual ((size_t) 0, rig.target.GetTraceSize());
        }
    };
}
