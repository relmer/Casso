#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorCommandSet.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "FakeDrivelessHost.h"
#include "FakeInspectorFrame.h"
#include "FakeInspectorHost.h"
#include "FakeInspectorParts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSetTests
//
//  The disk inspector's window commands and the window's zoom keys, with
//  fakes for the window and its parts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorCommandSetTests)
{
public:

    struct Rig
    {
        FakeDrivelessHost    drives;
        InspectorSession     session;
        FakeInspectorHost    host;
        FakeInspectorFrame   frame;
        FakeInspectorParts   parts;
        InspectorCommands    commands;
        InspectorCommandSet  set      { session, host, frame, parts, commands };

        Rig()
        {
            session.host = &drives;
            set.Build();
        }
    };


    static DxuiKeyEvent MakeKey (WPARAM vk, bool isCtrl)
    {
        DxuiKeyEvent  ev;



        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = vk;
        ev.ctrl = isCtrl;

        return ev;
    }


    TEST_METHOD (HeadCommands_AreLeftEmptyWithoutDrives)
    {
        Rig  rig;



        Assert::IsTrue (rig.commands.goToHead == nullptr);
        Assert::IsTrue (rig.commands.followHead == nullptr);
        Assert::IsTrue (rig.commands.save != nullptr);
    }


    TEST_METHOD (CtrlZero_ReturnsTheWindowToActualSize)
    {
        Rig  rig;



        rig.host.zoom = 1.5;

        Assert::IsTrue   (rig.set.RunKey (MakeKey ('0', true)));
        Assert::AreEqual (1.0, rig.host.zoom);
    }


    TEST_METHOD (CtrlPlus_ZoomsTheWindowIn)
    {
        Rig  rig;



        Assert::IsTrue (rig.set.RunKey (MakeKey (VK_OEM_PLUS, true)));
        Assert::IsTrue (rig.host.zoom > rig.frame.zoom);
    }


    TEST_METHOD (WindowZoomOut_StepsFromTheWindowsZoom)
    {
        Rig  rig;



        Assert::IsTrue (InspectorCommandSet::Run (rig.commands.windowZoomOut));
        Assert::IsTrue (rig.host.zoom < rig.frame.zoom);
    }


    TEST_METHOD (CloseAndDefaultSize_CallTheWindow)
    {
        Rig  rig;



        Assert::IsTrue   (InspectorCommandSet::Run (rig.commands.close));
        Assert::IsTrue   (InspectorCommandSet::Run (rig.commands.defaultSize));
        Assert::AreEqual (1, rig.frame.closes);
        Assert::AreEqual (1, rig.frame.defaultSizes);
    }


    TEST_METHOD (Walkthrough_HidesTheTipFirst)
    {
        Rig  rig;



        Assert::IsTrue   (InspectorCommandSet::Run (rig.commands.walkthrough));
        Assert::AreEqual (1, rig.frame.tipHides);
        Assert::AreEqual (1, rig.host.walkthroughs);
    }


    TEST_METHOD (ArrowKeys_WithNoDisk_AreNotTaken)
    {
        Rig  rig;



        Assert::IsFalse (rig.set.RunKey (MakeKey (VK_UP, false)));
        Assert::AreEqual (0, rig.host.selections);
    }


    TEST_METHOD (Run_SkipsADisabledCommand)
    {
        Rig  rig;



        Assert::IsFalse (InspectorCommandSet::Run (rig.commands.exportFile), L"there is no disk to export");
        Assert::IsFalse (InspectorCommandSet::Run (nullptr));
    }


    //  A host that saves and overwrites raw sectors, as Casso Explorer does.
    class RawHost : public FakeDrivelessHost
    {
    public:
        bool  CanTransferRaw() const override                                 { return true; }
        void  TransferRaw    (const InspectorRawTransfer & transfer) override { transfers.push_back (transfer); }

        vector<InspectorRawTransfer>  transfers;
    };


    TEST_METHOD (RawCommands_AreLeftEmptyWithoutAHostForThem)
    {
        Rig  rig;



        Assert::IsTrue (rig.commands.saveRaw == nullptr);
        Assert::IsTrue (rig.commands.overwriteRaw == nullptr);
    }


    TEST_METHOD (SaveSectors_StartsAtTheSelectedTrack)
    {
        static constexpr int  kTrack = 17;



        RawHost              raw;
        InspectorSession     session;
        FakeInspectorHost    host;
        FakeInspectorFrame   frame;
        FakeInspectorParts   parts;
        InspectorCommands    commands;
        InspectorCommandSet  set      { session, host, frame, parts, commands };



        session.host             = &raw;
        session.context.hasDisk  = true;
        set.Build();
        session.model.SelectQuarterTrack (kTrack * DiskImage::kQuarterTracksPerWholeTrack);

        Assert::IsTrue   (InspectorCommandSet::Run (commands.saveRaw));
        Assert::IsTrue   (InspectorCommandSet::Run (commands.overwriteRaw));
        Assert::AreEqual (size_t (2), raw.transfers.size());
        Assert::IsTrue   (raw.transfers[0].isSave && raw.transfers[0].isSectors);
        Assert::IsFalse  (raw.transfers[1].isSave);
        Assert::AreEqual (kTrack, raw.transfers[0].start);
        Assert::AreEqual (0, raw.transfers[0].sector);
        Assert::IsFalse  (raw.transfers[0].isPhysical, L"no sector is selected");
    }
};
