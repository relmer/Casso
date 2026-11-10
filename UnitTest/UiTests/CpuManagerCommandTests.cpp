#include "Pch.h"

#include "Shell/CpuManager.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




// Generous enough that a loaded build agent never flakes, short enough
// that a genuine regression fails the test rather than hanging it.
static constexpr int  s_kWaitMs = 5000;
static constexpr int  s_kPollMs = 5;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerCommandTests
//
//  The UI -> CPU command queue, exercised against a real running thread.
//  The behavior under test is that PAUSING THE MACHINE DOES NOT PARK THE
//  QUEUE: mount / eject / write-protect are user intents whose chrome
//  feedback is immediate (the drive door swings open the instant eject is
//  clicked), so a command stranded until resume leaves the door open over a
//  disk the store still says is mounted -- stale label, stale padlock,
//  stale tooltip.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CpuManagerCommandTests)
{
public:

    //  The command gate sees each command first, and a false drops it before it
    //  is queued.
    TEST_METHOD (TheGateDropsTheCommandsItDoesNotAllow)
    {
        CpuManager  cpu;



        cpu.SetCommandGate ([] (WORD id, const std::string &) { return id != IDM_MACHINE_RESET; });

        cpu.PostCommand (IDM_MACHINE_RESET);
        Assert::IsFalse (cpu.HasPendingCommands(), L"not allowed, not queued");

        cpu.PostCommand (IDM_DISK_EJECT1);
        Assert::IsTrue (cpu.HasPendingCommands(), L"allowed, queued");
    }


    TEST_METHOD (PostCommand_WhilePaused_StillDispatches)
    {
        CpuManager         cpu;
        std::atomic<int>   seen    { 0 };
        std::atomic<int>   frames  { 0 };
        std::atomic<WORD>  lastId  { 0 };
        std::string        lastPayload;
        std::mutex         payloadMutex;
        HRESULT            hr      = S_OK;

        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand & cmd)
                        {
                            {
                                std::lock_guard<std::mutex>  lock (payloadMutex);

                                lastPayload = cmd.payload;
                            }

                            lastId.store (cmd.id, std::memory_order_release);
                            seen.fetch_add (1, std::memory_order_acq_rel);
                        },
                        [&] () { frames.fetch_add (1, std::memory_order_acq_rel); },
                        nullptr);

        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        cpu.SetPaused (true);

        // Wait for the park before posting, so that the command ends the
        // pause wait rather than being queued while the loop ran. IsPaused
        // returns the request, not the park.
        WaitUntilParked (cpu);

        cpu.PostCommand (IDM_DISK_EJECT1, "disk.woz");

        WaitFor ([&] { return seen.load (std::memory_order_acquire) > 0; });

        Assert::AreEqual (1, seen.load (std::memory_order_acquire),
                          L"a command posted to a PAUSED machine must be dispatched, "
                          L"not parked until resume");
        Assert::AreEqual<WORD> (IDM_DISK_EJECT1, lastId.load (std::memory_order_acquire));

        {
            std::lock_guard<std::mutex>  lock (payloadMutex);

            Assert::AreEqual (std::string ("disk.woz"), lastPayload,
                              L"the payload must survive the paused dispatch path");
        }

        Assert::IsTrue (cpu.IsPaused(),
                        L"servicing the queue must not resume the machine");

        cpu.Stop();
    }

    TEST_METHOD (PostCommand_WhilePaused_RunsNoFrames)
    {
        CpuManager        cpu;
        std::atomic<int>  frames   { 0 };
        std::atomic<int>  commands { 0 };
        HRESULT           hr       = S_OK;

        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand &) { commands.fetch_add (1, std::memory_order_acq_rel); },
                        [&] ()                        { frames.fetch_add   (1, std::memory_order_acq_rel); },
                        nullptr);

        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        cpu.SetPaused (true);
        WaitUntilParked (cpu);

        int  baseline = frames.load (std::memory_order_acquire);

        cpu.PostCommand (IDM_DISK_EJECT2);
        WaitFor ([&] { return commands.load (std::memory_order_acquire) > 0; });

        // The second command is dispatched one pass of the loop after the
        // first, so a frame run after the first command is counted by then.
        cpu.PostCommand (IDM_DISK_EJECT2);
        WaitFor ([&] { return commands.load (std::memory_order_acquire) > 1; });

        Assert::AreEqual (2, commands.load (std::memory_order_acquire), L"both commands were dispatched");
        Assert::AreEqual (baseline, frames.load (std::memory_order_acquire),
                          L"waking for a command must not step the emulation");

        cpu.Stop();
    }

    TEST_METHOD (PostCommand_WhileRunning_StillDispatches)
    {
        CpuManager        cpu;
        std::atomic<int>  seen { 0 };
        HRESULT           hr   = S_OK;

        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand &) { seen.fetch_add (1, std::memory_order_acq_rel); },
                        nullptr,
                        nullptr);

        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        cpu.PostCommand (IDM_DISK_EJECT1);

        WaitFor ([&] { return seen.load (std::memory_order_acquire) > 0; });

        Assert::IsTrue (seen.load (std::memory_order_acquire) > 0,
                        L"the running path must keep dispatching commands");

        cpu.Stop();
    }

    TEST_METHOD (Stop_WhilePaused_JoinsThread)
    {
        CpuManager  cpu;
        HRESULT     hr = S_OK;

        hr = cpu.Start (nullptr, nullptr, nullptr, nullptr);

        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        cpu.SetPaused (true);
        WaitUntilParked (cpu);

        // The paused-and-idle loop must still observe the run flag dropping;
        // a Stop that hangs here would deadlock app shutdown.
        cpu.Stop();

        Assert::IsFalse (cpu.IsRunning(),
                         L"Stop must bring the thread down even from a paused wait");
    }

private:

    // Spins until the predicate holds or the budget expires. Returning
    // instead of asserting keeps the failure message at the call site,
    // where it can name what was actually expected.
    template <typename Predicate>
    static void WaitFor (Predicate pred)
    {
        for (int waited = 0; waited < s_kWaitMs && !pred(); waited += s_kPollMs)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (s_kPollMs));
        }
    }

    // Blocks until the CPU thread parks: it is outside a frame, and the
    // next frame runs after the resume.
    static void WaitUntilParked (CpuManager & cpu)
    {
        bool  isParked = cpu.TryWaitUntilParked (std::chrono::milliseconds (s_kWaitMs));

        Assert::IsTrue (isParked, L"the CPU thread parked");
    }
};
