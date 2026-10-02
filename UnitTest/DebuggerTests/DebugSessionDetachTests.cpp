#include "Pch.h"

#include "Debugger/DebugSession.h"
#include "Debugger/IDebugNotificationSink.h"
#include "MockDebugTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSessionDetachTests
//
//  Detach takes the debugger's CPU hook off the machine and keeps the
//  breakpoints; attaching again puts the hook back for them.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebugSessionDetachTests
{
    class NullSink : public IDebugNotificationSink
    {
    public:
        void OnStopped        (const StopEvent &) override    {}
        void OnResumed        () override                     {}
        void OnReset          (bool) override                 {}
        void OnMachineChanged (const std::string &) override  {}
        void OnModeChanged    (CommandMode) override          {}
    };





    TEST_CLASS (DetachTests)
    {
    public:

        TEST_METHOD (DetachRemovesTheHookAndAttachRestoresItWithTheBreakpoints)
        {
            MockDebugTarget  target;
            NullSink         sink;
            DebugSession     session (target, sink, RunState::FreeRunning);



            session.GetBreakpoints().AddAddress (0x0300, 0x0300);
            session.OnStopConditionsChanged();
            Assert::IsTrue (target.hookInstalled);

            session.SetAttached (false);
            Assert::IsFalse (target.hookInstalled, L"a detached machine runs with no debugger hook");
            Assert::IsFalse (session.IsAttached());

            session.GetBreakpoints().AddAddress (0x0302, 0x0302);
            session.OnStopConditionsChanged();
            Assert::IsFalse (target.hookInstalled, L"a breakpoint set while detached does not attach");

            session.SetAttached (true);
            Assert::IsTrue   (target.hookInstalled);
            Assert::AreEqual ((size_t) 2, session.GetBreakpoints().GetAll().size());
        }
    };
}
