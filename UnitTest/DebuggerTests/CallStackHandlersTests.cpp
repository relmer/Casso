#include "Pch.h"

#include "Debugger/Handlers/CallStackHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHandlersTests
//
//  CALLS and CALLS MODE through the session, over the mock target: the
//  mechanism is session state reached from every dialect, and the record is
//  a hook only while it is kept (FR-064).
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CallStackHandlersTests)
    {
    public:

        TEST_METHOD (CallsWithNothingRecordedIsTheWalk)
        {
            HandlerRig<CallStackHandlers>  rig;
            Reply                          reply = rig.RunOk ("CALLS");



            Assert::IsTrue   (std::holds_alternative<CallStackData> (reply.data));
            Assert::IsTrue   (std::get<CallStackData> (reply.data).rows.empty());
            Assert::AreEqual (std::string ("No calls are on the stack."), reply.text.at (0));
        }


        TEST_METHOD (NothingAttachedInstallsNoHook)
        {
            HandlerRig<CallStackHandlers>  rig;



            Assert::IsFalse (rig.target.hookInstalled, L"FR-064: no debugger interest, no hook");

            rig.session.SetCallRecording (true);
            Assert::IsFalse   (rig.target.hookInstalled, L"recording needs no hook: the CPU reports the opcodes it needs");
            Assert::IsNotNull (rig.target.watchedOpcodes);

            rig.session.SetCallRecording (false);
            Assert::IsNull    (rig.target.watchedOpcodes);
        }


        TEST_METHOD (CallsModeReportsAndChoosesInEveryDialect)
        {
            HandlerRig<CallStackHandlers>  rig;



            Assert::AreEqual (std::string ("Call stack: HYBRID"), rig.RunOk ("CALLS MODE").text.at (0));
            Assert::AreEqual (std::string ("Call stack: WALK"),   rig.RunOk ("CALLS MODE WALK").text.at (0));
            Assert::IsTrue   (rig.session.GetCallMechanism() == CallStackMechanism::Walk);

            rig.RunOk ("MODE MONITOR");
            Assert::AreEqual (std::string ("Call stack: RECORDED"), rig.RunOk ("/calls mode recorded").text.at (0));
            rig.RunFails ("/calls mode sideways", "invalid arguments");
        }
    };
}
