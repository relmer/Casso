#include "Pch.h"

#include "Debugger/Reverse/HistoryRecorder.h"
#include "EmuTests/TestMachine.h"
#include "Shell/CpuManager.h"
#include "Shell/FrameCycleBudget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CpuManagerPauseTests
{
    // Generous enough that a loaded build agent never flakes, short enough
    // that a genuine regression fails the test rather than hanging it.
    static constexpr std::chrono::milliseconds  s_kWait { 5000 };

    // Comfortably longer than the loop's ~16.6 ms frame period, so a thread
    // that is still running frames is certain to run one in it.
    static constexpr std::chrono::milliseconds  s_kSettle { 60 };

    static constexpr std::chrono::milliseconds  s_kPoll { 5 };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MidFrameGate
    //
    //  Holds the CPU thread before the instruction that starts at a chosen
    //  cycle, partway through a frame, until the test lets it go. A pause asked
    //  for while it holds is asked for in the middle of a slice, with the
    //  instruction about to run, which is the moment the frame loop must stop
    //  at. Its waits are bounded, so a failing test cannot hang the CPU thread.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MidFrameGate : public HistoryRecorder
    {
    public:
        void OnMediaChanged  (MachineHost &) override {}
        void OnMachineEdited (MachineHost &) override {}

        void Arm (uint64_t cycle) { m_nextDueCycle = cycle; }

        //  Test thread: waits for the CPU thread to reach the cycle, and gives
        //  the cycle the held instruction starts at.
        bool TryWaitUntilReached (uint64_t & outCycle)
        {
            std::unique_lock<std::mutex>  lock (m_mutex);
            bool                          isReached = m_cv.wait_for (lock, s_kWait, [this] { return m_isReached; });



            outCycle = m_reachedCycle;
            return isReached;
        }

        //  Test thread: lets the CPU thread run the instruction it holds.
        void Release()
        {
            std::lock_guard<std::mutex>  lock (m_mutex);



            m_isReleased = true;
            m_cv.notify_all();
        }

    protected:
        void OnCaptureDue (uint64_t cycle) override
        {
            std::unique_lock<std::mutex>  lock (m_mutex);



            m_nextDueCycle = UINT64_MAX;
            m_reachedCycle = cycle;
            m_isReached    = true;
            m_cv.notify_all();

            m_cv.wait_for (lock, s_kWait, [this] { return m_isReleased; });
        }

    private:
        std::mutex               m_mutex;
        std::condition_variable  m_cv;
        uint64_t                 m_reachedCycle = 0;
        bool                     m_isReached    = false;
        bool                     m_isReleased   = false;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Rig
    //
    //  A //e running INX / JMP at $0300 on a real CpuManager thread, with the
    //  machine wired to the pause flag as EmulatorShell wires it, and the gate
    //  set a quarter of the way into the third frame.
    //
    //  THE FRAME IS STOOD IN FOR: EmulatorShell::ExecuteCpuSlices needs a built
    //  shell, which a test cannot have. RunFrame is its slice loop with the
    //  audio, paste and debugger reporting left out -- slices to the next frame
    //  boundary, ending at a pause or a slice that ran nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Rig
    {
    public:
        static constexpr Word      kLoop       = 0x0300;
        static constexpr uint64_t  kFrame      = VideoTiming::kCyclesPerFrame;
        static constexpr uint64_t  kGateOffset = kFrame * 2 + kFrame / 4;     // a quarter into the third frame

        TestMachine        machine;
        CpuManager         cpu;
        MidFrameGate       gate;
        std::atomic<int>   frames { 0 };



        explicit Rig (SpeedMode speed) :
            machine (std::string ("Apple2e"), TestMachine::Slots::Empty)
        {
            static constexpr Byte  kProgram[] =
            {
                0xE8,                   // 0300  INX
                0x4C, 0x00, 0x03,       // 0301  JMP $0300
            };

            uint64_t  now = machine.GetCpu()->GetTotalCycles();
            size_t    i   = 0;



            for (i = 0; i < sizeof (kProgram); i++)
            {
                machine.GetMemoryBus().WriteByte (static_cast<Word> (kLoop + i), kProgram[i]);
            }

            machine.GetCpu()->SetPC (kLoop);

            gate.Arm (now - now % kFrame + kGateOffset);

            machine.SetHistoryRecorder (&gate);
            machine.SetStopFlag        (&cpu.GetPauseFlag());

            cpu.SetSpeedMode (speed, SpeedChooser::User);
        }



        ~Rig()
        {
            gate.Release();
            cpu.Stop();
        }



        void Start()
        {
            HRESULT  hr = cpu.Start (nullptr, nullptr, [this] { RunFrame(); }, nullptr);



            AssertSucceeded (hr, L"the CPU thread started");
        }



        void RunFrame()
        {
            uint32_t  target   = FrameCycleBudget::GetTarget ((uint32_t) kFrame, machine.GetCpu()->GetTotalCycles());
            uint32_t  executed = 0;
            uint32_t  slice    = 0;
            uint32_t  actual   = 0;



            while (executed < target && !cpu.IsPaused())
            {
                slice     = std::min (target - executed, FrameCycleBudget::kSliceCycles);
                actual    = (uint32_t) machine.RunCycles (slice);
                executed += actual;

                if (actual == 0)
                {
                    break;
                }
            }

            frames.fetch_add (1, std::memory_order_acq_rel);
        }



        //  Polls until the frame count passes `count` or the wait runs out.
        bool HasRunFramePast (int count)
        {
            auto  deadline = std::chrono::steady_clock::now() + s_kWait;



            while (frames.load (std::memory_order_acquire) <= count && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for (s_kPoll);
            }

            return frames.load (std::memory_order_acquire) > count;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PacingRig
    //
    //  A CpuManager paced at 1x whose frame does nothing but count, so the CPU
    //  thread spends nearly all of each frame period in its pacing wait.
    //  TryWaitUntilBetweenFrames returns kIntoTheWait after a frame ends, with
    //  the thread well into that wait and well short of the next frame, so a
    //  pause asked for then lands between frames rather than in one.
    //
    //  A service function, once the test sets one, is held whenever it runs on
    //  a paused machine until the test lets it go, as a slow service pass holds
    //  the thread. The hold is bounded, so a failing test cannot hang the CPU
    //  thread.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PacingRig
    {
    public:
        //  Long enough after a frame ends for the thread to have left it, and
        //  far short of the ~16.7 ms period the pacing wait fills at 1x.
        static constexpr std::chrono::milliseconds  kIntoTheWait { 3 };

        CpuManager  cpu;



        PacingRig()
        {
            cpu.SetSpeedMode (SpeedMode::Authentic, SpeedChooser::User);
        }



        ~PacingRig()
        {
            ReleaseService();
            cpu.Stop();
        }



        void HoldServiceWhilePaused()
        {
            cpu.SetServiceFunction ([this] { HoldIfPaused(); });
        }



        void Start()
        {
            HRESULT  hr = cpu.Start (nullptr, nullptr, [this] { CountFrame(); }, nullptr);



            AssertSucceeded (hr, L"the CPU thread started");
        }



        //  Test thread: waits for the next frame to end, then until
        //  kIntoTheWait after it. Spins rather than sleeps for the second
        //  part, since a sleep can overshoot its duration by a timer tick.
        bool TryWaitUntilBetweenFrames()
        {
            std::unique_lock<std::mutex>           lock (m_mutex);
            int                                    count   = m_frames;
            bool                                   isEnded = m_cv.wait_for (lock, s_kWait, [&] { return m_frames > count; });
            std::chrono::steady_clock::time_point  until   = m_frameEnd + kIntoTheWait;



            lock.unlock();

            while (std::chrono::steady_clock::now() < until)
            {
                std::this_thread::yield();
            }

            return isEnded;
        }



        int GetFrameCount()
        {
            std::lock_guard<std::mutex>  lock (m_mutex);



            return m_frames;
        }



        //  Test thread: lets a held service pass, and every later one, return.
        void ReleaseService()
        {
            std::lock_guard<std::mutex>  lock (m_mutex);



            m_isServiceReleased = true;
            m_cv.notify_all();
        }

    private:
        void CountFrame()
        {
            std::lock_guard<std::mutex>  lock (m_mutex);



            m_frames++;
            m_frameEnd = std::chrono::steady_clock::now();
            m_cv.notify_all();
        }



        void HoldIfPaused()
        {
            std::unique_lock<std::mutex>  lock (m_mutex);



            if (cpu.IsPaused())
            {
                m_cv.wait_for (lock, s_kWait, [this] { return m_isServiceReleased; });
            }
        }


        std::mutex                             m_mutex;
        std::condition_variable                m_cv;
        int                                    m_frames            = 0;
        std::chrono::steady_clock::time_point  m_frameEnd          = {};
        bool                                   m_isServiceReleased = false;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CpuManagerPauseTests
    //
    //  A pause asked for while the CPU thread is in the middle of a frame stops
    //  the machine on the next instruction boundary rather than at the end of
    //  the frame, and the CPU thread acknowledges it: IsParked reports where
    //  the thread is, where IsPaused reports what was asked for, and the UI
    //  waits for it with a bounded wait. Paced at 1x the thread spends most of
    //  each frame period waiting; at Maximum it never does, so both are run.
    //  A pause that lands in that wait, between frames, parks the thread on its
    //  way into the pause wait, with or without a service function to wake it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CpuManagerPauseTests)
    {
    public:

        TEST_METHOD (APauseMidFrameStopsOnTheNextInstructionAtAuthenticSpeed)
        {
            PauseMidFrame (SpeedMode::Authentic);
        }



        TEST_METHOD (APauseMidFrameStopsOnTheNextInstructionAtMaximumSpeed)
        {
            PauseMidFrame (SpeedMode::Maximum);
        }



        //  The wait is bounded: a CPU thread held in its frame, as by a long
        //  command, times the wait out rather than hanging the caller, and the
        //  park is still reported once the thread gets there.
        TEST_METHOD (TheWaitForTheParkIsBounded)
        {
            constexpr std::chrono::milliseconds  kShortWait { 50 };
            Rig                                  rig (SpeedMode::Authentic);
            uint64_t                             reached   = 0;
            bool                                 isReached = false;
            bool                                 isParked  = false;



            rig.Start();

            isReached = rig.gate.TryWaitUntilReached (reached);
            Assert::IsTrue (isReached, L"the CPU thread reached the middle of its frame");

            rig.cpu.TogglePaused();

            isParked = rig.cpu.TryWaitUntilParked (kShortWait);
            Assert::IsFalse (isParked,            L"a thread held in its frame has not parked, and the wait gave up");
            Assert::IsFalse (rig.cpu.IsParked(),  L"nor does IsParked say it has");

            rig.gate.Release();

            isParked = rig.cpu.TryWaitUntilParked (s_kWait);
            Assert::IsTrue (isParked, L"let go, the thread parks");
        }



        //  A pause that lands between frames, in the pacing wait, where the
        //  thread spends most of each frame period at 1x and Double. With no
        //  service function nothing but a resume ends the pause wait that
        //  follows, so the thread has to park on its way into that wait.
        TEST_METHOD (APauseBetweenFramesParksTheThreadWithNoServiceFunction)
        {
            PacingRig  rig;
            bool       isBetween = false;
            bool       isParked  = false;
            int        frames    = 0;



            rig.Start();

            isBetween = rig.TryWaitUntilBetweenFrames();
            Assert::IsTrue (isBetween, L"the CPU thread ran a frame and is in the pacing wait after it");

            rig.cpu.TogglePaused();

            isParked = rig.cpu.TryWaitUntilParked (s_kWait);
            Assert::IsTrue (isParked,            L"a pause that lands in the pacing wait parks the CPU thread with no command or service pass to wake it");
            Assert::IsTrue (rig.cpu.IsParked(),  L"and IsParked returns true");

            frames = rig.GetFrameCount();
            std::this_thread::sleep_for (s_kSettle);

            Assert::AreEqual (frames, rig.GetFrameCount(), L"a parked CPU thread runs no frame");
        }



        //  The same pause with a service function set, as the shell sets one.
        //  The thread parks before the first service pass after the pause, so
        //  a slow pass, held here until the test lets it go, does not hold
        //  back the park the menu's pause waits for.
        TEST_METHOD (APauseBetweenFramesParksTheThreadBeforeTheServicePass)
        {
            constexpr std::chrono::milliseconds  kParkWait { 1000 };
            PacingRig                            rig;
            bool                                 isBetween = false;
            bool                                 isParked  = false;



            rig.HoldServiceWhilePaused();
            rig.Start();

            isBetween = rig.TryWaitUntilBetweenFrames();
            Assert::IsTrue (isBetween, L"the CPU thread ran a frame and is in the pacing wait after it");

            rig.cpu.TogglePaused();

            isParked = rig.cpu.TryWaitUntilParked (kParkWait);
            Assert::IsTrue (isParked, L"the CPU thread parks while the service pass after the pause is still held");

            rig.ReleaseService();
        }



        //  The pause flag flips once per toggle however many threads toggle it.
        //  Each thread toggles an even number of times, so the machine ends as
        //  it began; a toggle lost between its read and its write would leave
        //  it paused about half the time, so the round runs repeatedly.
        TEST_METHOD (TogglesFromSeveralThreadsAreNotLost)
        {
            constexpr int             kRounds  = 20;
            constexpr int             kThreads = 4;
            constexpr int             kToggles = 20000;
            CpuManager                cpu;
            std::vector<std::thread>  threads;
            int                       round    = 0;
            int                       i        = 0;



            for (round = 0; round < kRounds; round++)
            {
                threads.clear();

                for (i = 0; i < kThreads; i++)
                {
                    threads.emplace_back ([&cpu]
                    {
                        for (int toggle = 0; toggle < kToggles; toggle++)
                        {
                            cpu.TogglePaused();
                        }
                    });
                }

                for (std::thread & thread : threads)
                {
                    thread.join();
                }

                Assert::IsFalse (cpu.IsPaused(), std::format (L"round {}: an even number of toggles left the machine paused", round).c_str());
            }
        }

    private:

        static void PauseMidFrame (SpeedMode speed)
        {
            constexpr uint64_t  kLongestInstruction = 7;
            Rig                 rig (speed);
            uint64_t            reached   = 0;
            uint64_t            stopped   = 0;
            uint64_t            frameEnd  = 0;
            int                 frames    = 0;
            bool                isReached = false;
            bool                isParked  = false;



            rig.Start();

            isReached = rig.gate.TryWaitUntilReached (reached);
            Assert::IsTrue (isReached, L"the CPU thread reached the middle of its frame");

            rig.cpu.TogglePaused();

            Assert::IsTrue  (rig.cpu.IsPaused(), L"the pause is asked for");
            Assert::IsFalse (rig.cpu.IsParked(), L"and not yet acted on, since the CPU thread is in its frame");

            rig.gate.Release();

            isParked = rig.cpu.TryWaitUntilParked (s_kWait);
            Assert::IsTrue (isParked, L"the CPU thread acknowledges the pause");

            stopped  = rig.machine.GetCpu()->GetTotalCycles();
            frameEnd = reached - reached % Rig::kFrame + Rig::kFrame;

            Assert::IsTrue (stopped - reached <= kLongestInstruction,
                            std::format (L"stopped {} cycles past the instruction it was asked during, not on the next boundary", stopped - reached).c_str());
            Assert::IsTrue (stopped < frameEnd, L"well short of the end of its frame");

            frames = rig.frames.load (std::memory_order_acquire);
            std::this_thread::sleep_for (s_kSettle);

            Assert::AreEqual (frames,  rig.frames.load (std::memory_order_acquire), L"a parked CPU thread runs no frame");
            Assert::AreEqual (stopped, rig.machine.GetCpu()->GetTotalCycles(),      L"and no instruction");
            Assert::IsTrue   (rig.cpu.IsParked(),                                   L"and stays parked");

            rig.cpu.TogglePaused();

            Assert::IsTrue  (rig.HasRunFramePast (frames), L"resumed, the machine runs again");
            Assert::IsFalse (rig.cpu.IsParked(),           L"and the CPU thread has left its park");
        }
    };
}
