#include "Pch.h"

#include "ControllerRig.h"
#include "EmuTests/InlineWorkQueue.h"
#include "Ui/Debugger/DebugViewPublisher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugViewPublisherTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugViewPublisherTests
    //
    //  The panes are built on the queue, not on the thread that submits, and
    //  only the newest input waiting is built.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugViewPublisherTests)
    {
    public:

        //  Submitting builds nothing; the queue's work builds and publishes.
        TEST_METHOD (TheBuildRunsOnTheQueueNotTheSubmittingThread)
        {
            ControllerRig                                             rig;
            DebuggerViewState                                         view;
            std::mutex                                                viewLock;
            InlineWorkQueue                                           queue;
            std::vector<std::shared_ptr<const DebuggerViewSnapshot>>  published;
            DebugViewPublisher                           publisher (view, viewLock,
                                                                    [&published] (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
                                                                    {
                                                                        published.push_back (std::move (snapshot));
                                                                    });



            publisher.SetQueue (&queue);
            publisher.Submit (MakeInput (rig, 0x0300), DebuggerViewSnapshot());

            Assert::AreEqual ((size_t) 0, published.size(), L"nothing is built on the submitting thread");
            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"the build waits on the queue");

            queue.WaitAll();

            Assert::AreEqual ((size_t) 1, published.size(), L"the queue built and published it");
            Assert::AreEqual ((Word) 0x0300, published[0]->pc);
            Assert::IsTrue   (!published[0]->code.empty(), L"with the panes built");
        }


        //  Inputs submitted while one waits replace it, so the queue builds
        //  once, from the newest.
        TEST_METHOD (OnlyTheNewestInputWaitingIsBuilt)
        {
            ControllerRig                                             rig;
            DebuggerViewState                                         view;
            std::mutex                                                viewLock;
            InlineWorkQueue                                           queue;
            std::vector<std::shared_ptr<const DebuggerViewSnapshot>>  published;
            DebugViewPublisher                           publisher (view, viewLock,
                                                                    [&published] (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
                                                                    {
                                                                        published.push_back (std::move (snapshot));
                                                                    });



            publisher.SetQueue (&queue);
            publisher.Submit (MakeInput (rig, 0x0300), DebuggerViewSnapshot());
            publisher.Submit (MakeInput (rig, 0x0400), DebuggerViewSnapshot());
            publisher.Submit (MakeInput (rig, 0x0500), DebuggerViewSnapshot());

            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"one piece of work for all three");

            queue.WaitAll();

            Assert::AreEqual ((size_t) 1, published.size(), L"one build");
            Assert::AreEqual ((Word) 0x0500, published[0]->pc, L"of the newest input");

            publisher.Submit (MakeInput (rig, 0x0600), DebuggerViewSnapshot());
            Assert::AreEqual ((size_t) 1, queue.GetPendingCount(), L"a later input queues work again");
        }


        //  What the machine's thread built goes out with the built panes.
        TEST_METHOD (TheLivePanesAreMergedIn)
        {
            ControllerRig                                             rig;
            DebuggerViewState                                         view;
            std::mutex                                                viewLock;
            DebuggerViewSnapshot                                      live;
            std::vector<std::shared_ptr<const DebuggerViewSnapshot>>  published;
            DebugViewPublisher                           publisher (view, viewLock,
                                                                    [&published] (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
                                                                    {
                                                                        published.push_back (std::move (snapshot));
                                                                    });



            live.panels.push_back ({ "disk", "Disk II", true });
            live.history.isBehindLive = true;

            publisher.Submit (MakeInput (rig, 0x0300), live);

            Assert::AreEqual ((size_t) 1, published.size(), L"with no queue the build runs at once");
            Assert::AreEqual ((size_t) 1, published[0]->panels.size(), L"the device panels came from the machine's thread");
            Assert::IsTrue   (published[0]->history.isBehindLive, L"and the history status");
            Assert::AreEqual ((Word) 0x0300, published[0]->pc, L"merged into the built panes");
        }

    private:

        static DebugViewInput MakeInput (ControllerRig & rig, Word pc)
        {
            DebugSession      & session   = rig.controller.GetSession();
            DebugViewInput      input;
            auto                capture   = std::make_shared<DebugViewCapture>();
            Cpu6502Registers    registers = session.GetTarget().GetRegisters();



            registers.pc = pc;
            session.GetTarget().SetRegisters (registers);

            DebugViewCapture::Take (session.GetTarget(), CallRecord(), 0, DebuggerViewState::kTraceRows, *capture);
            session.TakeView (input.session);

            input.capture  = capture;
            input.isPaused = true;
            return input;
        }
    };
}
