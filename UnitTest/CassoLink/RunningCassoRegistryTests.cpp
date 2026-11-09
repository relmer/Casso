#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/IProcessProbe.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RunningCassoRegistryTests
//
//  The files Casso writes for each serial port on TCP, and Casso Explorer's
//  reading of them: a running Casso is listed, and a file whose process has
//  exited, or whose process id is a newer process's, is deleted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RunningCassoRegistryTests)
{
public:

    struct FakeProbe : IProcessProbe
    {
        std::map<DWORD, uint64_t>  running;

        bool  TryGetStartTime (DWORD pid, uint64_t & outStarted) override
        {
            auto  found = running.find (pid);

            outStarted = found != running.end() ? found->second : 0;
            return found != running.end();
        }
    };

    static constexpr const wchar_t *  kFolder = L"C:\\Local\\Casso\\Links";




    TEST_METHOD (Publish_ThenEnumerate_ListsThePort)
    {
        InMemoryFileSystem         fs;
        FakeProbe                  probe;
        RunningCassoRegistry       registry (fs, probe, kFolder);
        std::vector<RunningCasso>  running;

        probe.running[4242] = 133'000'000'000'000'123ull;

        AssertSucceeded (registry.Publish (4242, 2, "Apple //c", 6502));
        AssertSucceeded (registry.Enumerate (running));

        Assert::AreEqual ((size_t) 1, running.size());
        Assert::AreEqual ((DWORD) 4242,                  running[0].pid);
        Assert::AreEqual (133'000'000'000'000'123ull,    running[0].started, L"a FILETIME, exactly");
        Assert::AreEqual (std::string ("Apple //c"),      running[0].machine);
        Assert::AreEqual (2,                              running[0].serialPort);
        Assert::IsFalse  (running[0].onCard, L"a //c's own port");
        Assert::AreEqual (std::string ("127.0.0.1:6502"), running[0].address);
    }




    TEST_METHOD (ACardsSlot_IsListedAsOnACard)
    {
        InMemoryFileSystem         fs;
        FakeProbe                  probe;
        RunningCassoRegistry       registry (fs, probe, kFolder);
        std::vector<RunningCasso>  running;

        probe.running[17] = 99;

        AssertSucceeded (registry.Publish (17, 5, "Apple //e", 6505, true));
        AssertSucceeded (registry.Enumerate (running));

        Assert::AreEqual ((size_t) 1, running.size());
        Assert::AreEqual (5, running[0].serialPort);
        Assert::IsTrue   (running[0].onCard);
    }




    TEST_METHOD (Withdraw_RemovesThePort)
    {
        InMemoryFileSystem         fs;
        FakeProbe                  probe;
        RunningCassoRegistry       registry (fs, probe, kFolder);
        std::vector<RunningCasso>  running;

        probe.running[10] = 5;

        AssertSucceeded (registry.Publish (10, 1, "Apple //c", 6501));
        AssertSucceeded (registry.Publish (10, 2, "Apple //c", 6502));
        registry.Withdraw (10, 1);

        AssertSucceeded (registry.Enumerate (running));
        Assert::AreEqual ((size_t) 1, running.size());
        Assert::AreEqual (2, running[0].serialPort);
    }




    TEST_METHOD (AnExitedOrReplacedProcess_IsDeleted)
    {
        InMemoryFileSystem         fs;
        FakeProbe                  probe;
        RunningCassoRegistry       registry (fs, probe, kFolder);
        std::vector<RunningCasso>  running;

        probe.running[1] = 100;
        probe.running[2] = 200;
        probe.running[3] = 300;

        AssertSucceeded (registry.Publish (1, 2, "Apple //c", 6502));
        AssertSucceeded (registry.Publish (2, 2, "Apple //e", 6503));
        AssertSucceeded (registry.Publish (3, 2, "Apple //c", 6504));

        probe.running.erase (1);        // exited
        probe.running[2] = 201;         // its id now another process's

        AssertSucceeded (registry.Enumerate (running));

        Assert::AreEqual ((size_t) 1, running.size());
        Assert::AreEqual ((DWORD) 3, running[0].pid);
        Assert::IsFalse (fs.Exists (std::wstring (kFolder) + L"\\1-2.json"), L"the exited Casso's file is deleted");
        Assert::IsFalse (fs.Exists (std::wstring (kFolder) + L"\\2-2.json"));
    }




    TEST_METHOD (NoFolder_IsNoneRunning)
    {
        InMemoryFileSystem         fs;
        FakeProbe                  probe;
        RunningCassoRegistry       registry (fs, probe, kFolder);
        std::vector<RunningCasso>  running;

        AssertSucceeded (registry.Enumerate (running));
        Assert::IsTrue (running.empty());
    }
};
