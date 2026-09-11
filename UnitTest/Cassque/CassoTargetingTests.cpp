#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Cassque/Model/CassoTargeting.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoTargetingTests
//
//  The ranking over made-up window handles: nothing here is a real window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoTargetingTests)
{
public:

    static const HWND  kOwner;
    static const HWND  kRunningA;
    static const HWND  kRunningB;



    static MachineConfig MakeMachine (int driveCount)
    {
        MachineConfig  config;
        SlotConfig     slot;
        int            i = 0;

        config.name = "Apple //e";
        slot.slot   = 6;
        slot.device = kpszDiskIiDevice;

        for (i = 0; i < driveCount; i++)
        {
            slot.ports.push_back (PortConfig { "", kpszDiskIiDrive });
        }

        //  A card with one drive still has two connectors; the second is
        //  empty, and an empty list would mean "not described".
        if (driveCount == 1)
        {
            slot.ports.push_back (PortConfig { "", "" });
        }

        config.slots.push_back (slot);

        return config;
    }



    TEST_METHOD (OwnerAlive_WinsOverEverything)
    {
        const HWND   running[] = { kRunningA, kRunningB };
        CassoTarget  target    = CassoTargeting::Choose (kOwner, true, running, MakeMachine (2));

        Assert::IsTrue   (target.kind == CassoTarget::Kind::Owner);
        Assert::IsTrue   (target.hwnd == kOwner);
        Assert::AreEqual (2, target.driveCount);
        Assert::AreEqual (std::string ("Apple //e"), target.machineName);
    }



    TEST_METHOD (OwnerDead_TheMostRecentlyActiveRunningOneIsChosen)
    {
        const HWND   running[] = { kRunningB, kRunningA };
        CassoTarget  target    = CassoTargeting::Choose (kOwner, false, running, MakeMachine (2));

        Assert::IsTrue (target.kind == CassoTarget::Kind::Running);
        Assert::IsTrue (target.hwnd == kRunningB, L"the caller lists the most recently active first");
    }



    TEST_METHOD (NoOwner_ARunningOneIsChosen)
    {
        const HWND   running[] = { kRunningA };
        CassoTarget  target    = CassoTargeting::Choose (nullptr, false, running, MakeMachine (2));

        Assert::IsTrue (target.kind == CassoTarget::Kind::Running);
        Assert::IsTrue (target.hwnd == kRunningA);
    }



    TEST_METHOD (NoneRunning_LaunchWithTheDefaultMachinesDriveCount)
    {
        CassoTarget  oneDrive = CassoTargeting::Choose (nullptr, false, std::span<const HWND>(), MakeMachine (1));
        CassoTarget  twoDrive = CassoTargeting::Choose (kOwner, false, std::span<const HWND>(), MakeMachine (2));

        Assert::IsTrue   (oneDrive.kind == CassoTarget::Kind::Launch);
        Assert::IsNull   (oneDrive.hwnd);
        Assert::AreEqual (1, oneDrive.driveCount);
        Assert::AreEqual (2, twoDrive.driveCount);
        Assert::IsTrue   (twoDrive.kind == CassoTarget::Kind::Launch, L"a dead owner and nothing running means launch");
    }
};



const HWND  CassoTargetingTests::kOwner    = reinterpret_cast<HWND> (0x1001);
const HWND  CassoTargetingTests::kRunningA = reinterpret_cast<HWND> (0x2002);
const HWND  CassoTargetingTests::kRunningB = reinterpret_cast<HWND> (0x3003);
