#include "Pch.h"

#include "Core/ThreadOwnership.h"
#include "Shell/CpuManager.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




// Generous enough that a loaded build agent never flakes, short enough
// that a genuine regression fails the test rather than hanging it.
static constexpr int  s_kWaitMs = 5000;
static constexpr int  s_kPollMs = 5;

// One settle window. Comfortably longer than the loop's ~16.6 ms frame
// period, so a still-running thread is guaranteed to tick the counter.
static constexpr int  s_kParkMs = 60;





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

        // Wait for the thread to actually PARK before posting, so the test
        // covers the wake path rather than a command that happened to be
        // queued while the loop was still spinning. Frames going quiet is
        // the observable proof it reached the pause wait; IsPaused only
        // reports what this thread just asked for.
        WaitForFramesToStop (frames);

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
        WaitForFramesToStop (frames);

        int  baseline = frames.load (std::memory_order_acquire);

        cpu.PostCommand (IDM_DISK_EJECT2);
        WaitFor ([&] { return commands.load (std::memory_order_acquire) > 0; });

        std::this_thread::sleep_for (std::chrono::milliseconds (50));

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
        std::this_thread::sleep_for (std::chrono::milliseconds (s_kParkMs));

        // The paused-and-idle loop must still observe the run flag dropping;
        // a Stop that hangs here would deadlock app shutdown.
        cpu.Stop();

        Assert::IsFalse (cpu.IsRunning(),
                         L"Stop must bring the thread down even from a paused wait");
    }


    //  A deploy hold stops frames, the command drain and the service pass
    //  until it is released; a command posted while held runs after.
    TEST_METHOD (HoldForDeploy_HoldsFramesCommandsAndService)
    {
        CpuManager        cpu;
        std::atomic<int>  frames   { 0 };
        std::atomic<int>  commands { 0 };
        std::atomic<int>  services { 0 };
        int               frameMark   = 0;
        int               serviceMark = 0;
        HRESULT           hr          = S_OK;



        cpu.SetServiceFunction ([&] { services.fetch_add (1, std::memory_order_acq_rel); });

        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand &) { commands.fetch_add (1, std::memory_order_acq_rel); },
                        [&] ()                        { frames.fetch_add   (1, std::memory_order_acq_rel); },
                        nullptr);
        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        WaitFor ([&] { return frames.load (std::memory_order_acquire) > 0; });

        hr = cpu.HoldForDeploy (s_kWaitMs);
        Assert::AreEqual (S_OK, hr, L"the hold takes effect");
        Assert::IsTrue  (cpu.IsHeldForDeploy(),       L"the thread is held");
        Assert::IsTrue  (cpu.IsDeployHoldRequested(), L"and the request is in place");

        frameMark   = frames.load (std::memory_order_acquire);
        serviceMark = services.load (std::memory_order_acquire);

        cpu.PostCommand (IDM_DISK_EJECT1);
        std::this_thread::sleep_for (std::chrono::milliseconds (s_kParkMs));

        Assert::AreEqual (frameMark,   frames.load (std::memory_order_acquire),   L"no frame runs while held");
        Assert::AreEqual (serviceMark, services.load (std::memory_order_acquire), L"no service pass runs while held");
        Assert::AreEqual (0,           commands.load (std::memory_order_acquire), L"and no command is drained");

        cpu.ReleaseDeployHold();
        WaitFor ([&] { return commands.load (std::memory_order_acquire) > 0; });

        Assert::AreEqual (1, commands.load (std::memory_order_acquire), L"the queued command runs after the release");
        Assert::IsFalse (cpu.IsDeployHoldRequested(), L"and nothing is requested");

        cpu.Stop();
    }


    //  A paused machine with no service function waits for a command or a
    //  resume; the hold request ends that wait, so the hold takes effect
    //  well before the deadline.
    TEST_METHOD (HoldForDeploy_WhilePausedWithNoServiceFunction_IsHeldBeforeTheDeadline)
    {
        CpuManager  cpu;
        auto        start = std::chrono::steady_clock::now();
        HRESULT     hr    = S_OK;



        cpu.SetPaused (true);

        hr = cpu.Start (nullptr, nullptr, nullptr, nullptr);
        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        Assert::IsTrue (cpu.TryWaitUntilParked (std::chrono::milliseconds (s_kWaitMs)), L"the thread parks");

        start = std::chrono::steady_clock::now();
        hr    = cpu.HoldForDeploy (s_kWaitMs);

        Assert::AreEqual (S_OK, hr, L"the hold takes effect on a paused thread");
        Assert::IsTrue  (std::chrono::steady_clock::now() - start < std::chrono::milliseconds (s_kWaitMs / 2),
                         L"well before the deadline");

        cpu.ReleaseDeployHold();
        cpu.Stop();
    }


    TEST_METHOD (Stop_WhileHeldForDeploy_JoinsThread)
    {
        CpuManager  cpu;
        HRESULT     hr = S_OK;



        hr = cpu.Start (nullptr, nullptr, nullptr, nullptr);
        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        hr = cpu.HoldForDeploy (s_kWaitMs);
        Assert::AreEqual (S_OK, hr, L"the hold takes effect");

        // A Stop that hangs here would deadlock a failed update's shutdown.
        cpu.Stop();

        Assert::IsFalse (cpu.IsRunning(),             L"Stop brings the thread down from a hold");
        Assert::IsFalse (cpu.IsHeldForDeploy(),       L"the hold has ended");
        Assert::IsFalse (cpu.IsDeployHoldRequested(), L"and nothing is requested");
    }


    //  A hold that misses its deadline is withdrawn, so the thread is not
    //  held later, after its caller has given up.
    TEST_METHOD (HoldForDeploy_TimesOutAndWithdrawsWhileACommandRuns)
    {
        CpuManager         cpu;
        std::atomic<bool>  isInCommand  = false;
        std::atomic<bool>  isLetGo      = false;
        std::atomic<int>   commands     { 0 };
        HRESULT            hr           = S_OK;



        hr = cpu.Start (nullptr,
                        [&] (const EmulatorCommand & cmd)
                        {
                            if (cmd.id == IDM_DISK_EJECT1)
                            {
                                auto  deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (s_kWaitMs);

                                isInCommand = true;

                                while (!isLetGo.load() && std::chrono::steady_clock::now() < deadline)
                                {
                                    std::this_thread::sleep_for (std::chrono::milliseconds (s_kPollMs));
                                }
                            }

                            commands.fetch_add (1, std::memory_order_acq_rel);
                        },
                        nullptr,
                        nullptr);
        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        cpu.PostCommand (IDM_DISK_EJECT1);
        WaitFor ([&] { return isInCommand.load(); });
        Assert::IsTrue (isInCommand.load(), L"the command is running");

        hr = cpu.HoldForDeploy (s_kParkMs);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), hr, L"the hold times out while the command runs");
        Assert::IsFalse (cpu.IsDeployHoldRequested(), L"and is withdrawn");

        isLetGo = true;
        cpu.PostCommand (IDM_DISK_EJECT2);
        WaitFor ([&] { return commands.load (std::memory_order_acquire) == 2; });

        Assert::AreEqual (2, commands.load (std::memory_order_acquire), L"the thread runs on: it was never held");
        Assert::IsFalse (cpu.IsHeldForDeploy(), L"and is not held");

        cpu.Stop();
    }


    //  The hold hooks move the disk store's token: the emulation thread
    //  releases it before it is held, the test thread claims it while held,
    //  and the emulation thread claims it again after the release.
    TEST_METHOD (HoldForDeploy_LetsAnotherThreadClaimTheStore)
    {
        CpuManager                         cpu;
        ThreadOwnership                    ownership;
        std::atomic<bool>                  isClaimedAfter = false;
        HRESULT                            hr             = S_OK;



        ownership.Release();

        cpu.SetDeployHoldHooks ([&] { ownership.Release(); }, [&] { ownership.Claim(); });

        hr = cpu.Start ([&] { ownership.Claim(); },
                        [&] (const EmulatorCommand &) { isClaimedAfter = ownership.IsHeldByCurrentThread(); },
                        nullptr,
                        [&] { ownership.Release(); });
        Assert::AreEqual (S_OK, hr, L"CpuManager::Start should succeed");

        hr = cpu.HoldForDeploy (s_kWaitMs);
        Assert::AreEqual (S_OK, hr, L"the hold takes effect");

        ownership.Claim();
        Assert::IsTrue (ownership.IsHeldByCurrentThread(), L"the test thread has the store while the thread is held");

        ownership.Release();
        cpu.ReleaseDeployHold();

        cpu.PostCommand (IDM_DISK_EJECT1);
        WaitFor ([&] { return isClaimedAfter.load(); });

        Assert::IsTrue (isClaimedAfter.load(), L"the emulation thread has the store again after the release");

        cpu.Stop();
        ownership.Claim();
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

    // Blocks until the frame counter holds still across a settle window --
    // the observable "the CPU thread has reached its pause wait" signal.
    static void WaitForFramesToStop (const std::atomic<int> & frames)
    {
        for (int waited = 0; waited < s_kWaitMs; waited += s_kParkMs)
        {
            int  before = frames.load (std::memory_order_acquire);

            std::this_thread::sleep_for (std::chrono::milliseconds (s_kParkMs));

            if (frames.load (std::memory_order_acquire) == before)
            {
                return;
            }
        }
    }
};
