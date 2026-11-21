#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Casso/FakeHostClipboard.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "FakeBlockDialogs.h"
#include "FakeBlockFrame.h"
#include "FakeBlockJobs.h"
#include "FakeBlockRepair.h"
#include "FakeBlockView.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Ui/DiskInspector/Blocks/BlockCommandSet.h"
#include "Ui/DiskInspector/Blocks/BlockEditController.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorPanel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCommandSetTests
//
//  Block mode's commands and the keys and mouse buttons that run them, with
//  a panel for the places and fakes for the window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockCommandSetTests)
{
public:

    static constexpr size_t    kBlockBytes = 512;
    static constexpr size_t    kBlocks     = 1600;
    static constexpr int64_t   kTime       = 1'700'000'000;
    static constexpr uint32_t  kEdited     = 2;
    static constexpr Byte      kValue      = 0xA5;

    //  The commands and the parts they call, over vol.po in a fake file system.
    struct Rig
    {
        FakeDiskFileIo         io;
        BlockInspectorModel    model;
        FakeBlockDialogs       dialogs;
        FakeBlockFrame         frame;
        FakeBlockJobs          jobs;
        FakeBlockView          view;
        FakeHostClipboard      clipboard;
        FakeBlockRepair        repair;
        DxuiDarkTheme          theme;
        MockDxuiTextRenderer   text;
        BlockInspectorPanel    panel;
        InspectorCommands      commands;
        BlockFileSession       session  { model, dialogs, frame };
        BlockEditController    edits    { { model, jobs, view, dialogs, frame, session, session, clipboard } };
        BlockCommandSet        set      { { model, jobs, view, panel, panel, dialogs, frame, session, session, edits, repair }, commands };

        Rig()
        {
            vector<Byte>  buffer (kBlocks * kBlockBytes, 0);

            AssertSucceeded (ProDosSkeleton::Write (buffer, "COMMANDS"));
            io.files["vol.po"]  = buffer;
            io.stamps["vol.po"] = { buffer.size(), kTime };

            session.SetFile (io, L"vol.po");
            session.Attach (jobs, view);
            session.Open();
            panel.Init (&model, nullptr, &theme, nullptr, &text);
            set.Build();
        }

        void EditOneByte()
        {
            int  overflow = 0;

            model.SelectBlock (kEdited);
            Assert::AreEqual (1, model.Paste (0, L"A5", false, overflow));
        }
    };


    static DxuiKeyEvent MakeKey (WPARAM vk, bool isCtrl, bool isAlt = false, bool isShift = false)
    {
        DxuiKeyEvent  ev;



        ev.vk    = vk;
        ev.ctrl  = isCtrl;
        ev.alt   = isAlt;
        ev.shift = isShift;

        return ev;
    }


    TEST_METHOD (Save_IsEnabledOnlyWithEdits)
    {
        Rig  rig;



        Assert::IsFalse (rig.commands.save->IsEnabled());

        rig.EditOneByte();

        Assert::IsTrue (rig.commands.save->IsEnabled());
    }


    TEST_METHOD (CtrlS_SavesTheEdits)
    {
        Rig   rig;
        bool  isResult = false;



        rig.EditOneByte();

        Assert::IsTrue (rig.set.TryRunKey (MakeKey ('S', true), isResult));
        Assert::IsTrue (isResult);
        Assert::AreEqual (kValue, rig.io.files["vol.po"][kEdited * kBlockBytes]);
    }


    TEST_METHOD (CtrlC_WithoutShiftGoesToTheFocusedControl)
    {
        Rig   rig;
        bool  isResult = false;



        Assert::IsFalse (rig.set.TryRunKey (MakeKey ('C', true), isResult));
        Assert::IsTrue (rig.set.TryRunKey (MakeKey ('C', true, false, true), isResult));
    }


    TEST_METHOD (Escape_CancelsTheJobAndGoesOn)
    {
        Rig   rig;
        bool  isResult = false;



        Assert::IsFalse (rig.set.TryRunKey (MakeKey (VK_ESCAPE, false), isResult));
        Assert::AreEqual (1, rig.jobs.cancels);
    }


    TEST_METHOD (F2_TogglesEditing)
    {
        Rig   rig;
        bool  isResult = false;



        Assert::IsTrue (rig.set.TryRunKey (MakeKey (VK_F2, false), isResult));
        Assert::IsTrue (rig.view.isEditing);
        Assert::IsTrue (rig.commands.editSector->isChecked());
    }


    TEST_METHOD (CtrlPlusAndCtrlZero_ZoomTheWindow)
    {
        Rig   rig;
        bool  isResult = false;



        Assert::IsTrue (rig.set.TryRunWindowKey (MakeKey (VK_OEM_PLUS, true), isResult));
        Assert::IsTrue (rig.frame.zoom > 1.0);

        Assert::IsTrue (rig.set.TryRunWindowKey (MakeKey ('0', true), isResult));
        Assert::AreEqual (1.0, rig.frame.zoom);
    }


    TEST_METHOD (AltLeft_IsTakenWhenBackIsDisabled)
    {
        Rig   rig;
        bool  isResult = true;



        Assert::IsTrue (rig.set.TryRunWindowKey (MakeKey (VK_LEFT, false, true), isResult));
        Assert::IsFalse (isResult, L"there is no place to go back to");
    }


    TEST_METHOD (MouseBackButton_IsTakenOnItsRelease)
    {
        Rig             rig;
        DxuiMouseEvent  ev;



        ev.kind   = DxuiMouseEventKind::Down;
        ev.button = DxuiMouseButton::Back;

        Assert::IsFalse (rig.set.TryRunMouseButton (ev));

        ev.kind = DxuiMouseEventKind::Up;

        Assert::IsTrue (rig.set.TryRunMouseButton (ev));
    }


    TEST_METHOD (Repair_FollowsCanRepair)
    {
        Rig  rig;



        Assert::IsTrue (BlockCommandSet::Run (rig.commands.repair));
        Assert::AreEqual (1, rig.repair.repairs);

        rig.repair.canRepair = false;

        Assert::IsFalse (BlockCommandSet::Run (rig.commands.repair));
        Assert::AreEqual (1, rig.repair.repairs);
    }


    TEST_METHOD (WindowZoomCommands_StepAndReset)
    {
        Rig  rig;



        Assert::IsTrue (BlockCommandSet::Run (rig.commands.windowZoomOut));
        Assert::IsTrue (rig.frame.zoom < 1.0);
        Assert::IsTrue (BlockCommandSet::Run (rig.commands.actualSize));
        Assert::AreEqual (1.0, rig.frame.zoom);
    }
};
