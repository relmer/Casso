#include "Pch.h"
#include "CassoExplorer/Model/RemovableMediaWatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RemovableMediaWatchTests
//
//  The watch over a fake probe and a fake drive mask. Drive A is bit 0.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr DWORD  s_kDrivesCDE = 0b11100;
static constexpr DWORD  s_kDriveD    = 0b01000;
static constexpr DWORD  s_kDriveE    = 0b10000;
static constexpr DWORD  s_kLongMs    = 60000;     // an interval no test waits out
static constexpr int    s_kTries     = 200;
static constexpr int    s_kTryMs     = 5;





////////////////////////////////////////////////////////////////////////////////
//
//  WaitForLook
//
//  Polls until the watch has looked, for at most a second.
//
////////////////////////////////////////////////////////////////////////////////

static bool WaitForLook (const RemovableMediaWatch & watch)
{
    for (int i = 0; i < s_kTries && !watch.HasLooked(); i++)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (s_kTryMs));
    }

    return watch.HasLooked();
}





TEST_CLASS (RemovableMediaWatchTests)
{
public:

    TEST_METHOD (Probe_SetsTheBitOfEachDriveWithMedia)
    {
        DWORD  media = RemovableMediaWatch::Probe ([] (const std::wstring & root) { return root != L"D:\\"; }, s_kDrivesCDE);



        Assert::AreEqual ((DWORD) (s_kDrivesCDE & ~s_kDriveD), media);
    }

    TEST_METHOD (GetMask_BeforeTheFirstLook_CountsEveryDrive)
    {
        RemovableMediaWatch  watch ([] (const std::wstring &) { return false; }, [] { return s_kDrivesCDE; });



        Assert::AreEqual (s_kDrivesCDE, watch.GetMask (s_kDrivesCDE));
    }

    TEST_METHOD (GetMask_AfterALook_LeavesOutAnEmptyDrive)
    {
        RemovableMediaWatch  watch ([] (const std::wstring & root) { return root != L"D:\\"; }, [] { return s_kDrivesCDE; });



        watch.Start (s_kLongMs);

        Assert::IsTrue   (WaitForLook (watch));
        Assert::AreEqual ((DWORD) (s_kDrivesCDE & ~s_kDriveD), watch.GetMask (s_kDrivesCDE));
    }

    TEST_METHOD (Kick_LooksAgainBeforeTheInterval)
    {
        std::atomic<bool>    hasMedia = false;
        RemovableMediaWatch  watch ([&] (const std::wstring &) { return hasMedia.load(); }, [] { return s_kDriveD; });



        watch.Start (s_kLongMs);
        Assert::IsTrue (WaitForLook (watch));
        Assert::AreEqual ((DWORD) 0, watch.GetMask (s_kDriveD));

        hasMedia = true;
        watch.Kick();

        for (int i = 0; i < s_kTries && watch.GetMask (s_kDriveD) == 0; i++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (s_kTryMs));
        }

        watch.Stop();

        Assert::AreEqual (s_kDriveD, watch.GetMask (s_kDriveD));
    }

    TEST_METHOD (GetMask_ADriveAddedSinceTheLook_CountsAsHavingMedia)
    {
        RemovableMediaWatch  watch ([] (const std::wstring &) { return false; }, [] { return s_kDriveD; });



        watch.Start (s_kLongMs);

        Assert::IsTrue   (WaitForLook (watch));
        Assert::AreEqual (s_kDriveE, watch.GetMask (s_kDriveE));
    }
};
