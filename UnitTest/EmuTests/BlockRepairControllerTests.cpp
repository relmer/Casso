#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeBlockDialogs.h"
#include "FakeBlockFrame.h"
#include "FakeBlockJobs.h"
#include "FakeBlockView.h"
#include "FakeDiskFileIo.h"
#include "FakeDriveSpace.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"
#include "Ui/DiskInspector/Blocks/BlockRepairController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairControllerTests
//
//  Which volume repair is offered for, and the message when it isn't
//  available, with a panel for the places and fakes for the window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockRepairControllerTests)
{
public:

    static constexpr size_t   kBlockBytes = 512;
    static constexpr size_t   kBlocks     = 1600;
    static constexpr int64_t  kTime       = 1'700'000'000;

    //  Repair and the parts it calls, over vol.po in a fake file system.
    struct Rig
    {
        FakeDiskFileIo         io;
        BlockInspectorModel    model;
        FakeBlockDialogs       dialogs;
        FakeBlockFrame         frame;
        FakeBlockJobs          jobs;
        FakeBlockView          view;
        FakeDriveSpace         drives;
        DxuiDarkTheme          theme;
        MockDxuiTextRenderer   text;
        BlockInspectorPanel    panel;
        BlockFileSession       session { model, dialogs, frame };
        NullBlockEditNote      note;
        BlockRepairController  repair  { { model, jobs, view, panel, dialogs, session, note, drives } };

        Rig()
        {
            vector<Byte>  buffer (kBlocks * kBlockBytes, 0);

            AssertSucceeded (ProDosSkeleton::Write (buffer, "REPAIR"));
            io.files["vol.po"]  = buffer;
            io.stamps["vol.po"] = { buffer.size(), kTime };

            session.SetFile (io, L"vol.po");
            session.Attach (jobs, view);
            session.Open();
            panel.Init (&model, nullptr, &theme, nullptr, &text);
        }
    };


    TEST_METHOD (CanRepair_TheImagesOnlyVolume)
    {
        Rig  rig;



        Assert::IsTrue (rig.repair.CanRepair());
    }


    TEST_METHOD (Repair_WhileBusyWarns)
    {
        Rig  rig;



        rig.jobs.isBusy = true;
        rig.repair.Repair();

        Assert::AreEqual ((size_t) 1, rig.dialogs.warnings.size());
        Assert::AreEqual (std::wstring (L"Repair isn't available. Wait for the current job to finish."), rig.dialogs.warnings[0]);
    }


    TEST_METHOD (Repair_HealthyVolumeWarnsWithTheReason)
    {
        Rig  rig;



        rig.repair.Repair();

        Assert::AreEqual ((size_t) 1, rig.dialogs.warnings.size());
        Assert::IsTrue (rig.dialogs.warnings[0].starts_with (L"Repair isn't available. "));
        Assert::IsTrue (rig.dialogs.warnings[0].size() > std::wstring (L"Repair isn't available. ").size(), L"the plan's reason follows");
    }


    TEST_METHOD (GetNotice_HealthyVolumeHasNone)
    {
        Rig           rig;
        bool          isBad     = true;
        bool          canRepair = true;
        std::wstring  text;



        text = rig.repair.GetNotice (isBad, canRepair);

        Assert::IsTrue  (text.empty());
        Assert::IsFalse (isBad);
        Assert::IsFalse (canRepair);
    }


    TEST_METHOD (FormatNotice_HasTheFindingAndTheReason)
    {
        RepairPlan  plan;



        plan.canRepair = true;
        Assert::AreEqual (std::wstring (L"7 blocks in use are marked free."),
                          BlockRepairController::FormatNotice (FileSystemVerdict::Bad, L"7 blocks in use are marked free", plan));

        plan.canRepair = false;
        plan.reason    = L"The card is read-only.";
        Assert::AreEqual (std::wstring (L"7 blocks in use are marked free. Repair isn't available. The card is read-only."),
                          BlockRepairController::FormatNotice (FileSystemVerdict::Bad, L"7 blocks in use are marked free", plan));
    }
};
