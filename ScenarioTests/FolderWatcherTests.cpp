#include "Pch.h"
#include "Seams/Win32FolderWatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatcherTests
//
//  The folder watcher against a real folder: a callback that has begun
//  finishes before Unwatch returns, so the object it captured can be
//  destroyed once Unwatch returns.
//
//  Each test works in a scratch folder of its own and removes it before it
//  asserts anything, so a failure leaves nothing behind.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FolderWatcherTests)
{
public:

    static constexpr DWORD  kEnterWaitMs  = 5000;
    static constexpr DWORD  kStillRunning = 200;



    TEST_METHOD (UnwatchWaitsForACallbackThatHasBegun)
    {
        std::error_code          ec;
        std::filesystem::path    dir         = std::filesystem::temp_directory_path (ec)
                                               / std::format (L"casso-watch-{}-{}", GetCurrentProcessId(), GetTickCount64());
        Win32FolderWatcher       watcher;
        HANDLE                   entered     = CreateEventW (nullptr, TRUE, FALSE, nullptr);
        HANDLE                   release     = CreateEventW (nullptr, TRUE, FALSE, nullptr);
        std::atomic<bool>        isDone      = false;
        std::atomic<bool>        isUnwatched = false;
        std::thread              unwatch;
        bool                     isWatched   = false;
        DWORD                    waited      = 0;
        bool                     wasEarly    = false;



        std::filesystem::create_directories (dir, ec);

        isWatched = watcher.Watch (dir.wstring(), [&] (const std::wstring &)
        {
            SetEvent (entered);
            WaitForSingleObject (release, INFINITE);
            isDone = true;
        });

        std::ofstream (dir / L"a.txt") << "x";

        waited = WaitForSingleObject (entered, kEnterWaitMs);

        unwatch = std::thread ([&] { watcher.Unwatch (dir.wstring()); isUnwatched = true; });

        Sleep (kStillRunning);
        wasEarly = isUnwatched.load();

        SetEvent (release);
        unwatch.join();

        CloseHandle (entered);
        CloseHandle (release);
        std::filesystem::remove_all (dir, ec);

        Assert::IsTrue  (isWatched);
        Assert::AreEqual (WAIT_OBJECT_0, waited, L"the change was reported");
        Assert::IsFalse (wasEarly, L"Unwatch did not return while the callback ran");
        Assert::IsTrue  (isDone.load(), L"the callback finished first");
    }
};
