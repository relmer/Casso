#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../Casso/FakeHostClipboard.h"
#include "FakeBlockDialogs.h"
#include "FakeBlockFrame.h"
#include "FakeBlockJobs.h"
#include "FakeBlockView.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Ui/DiskInspector/Blocks/BlockEditController.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockEditControllerTests
//
//  Block mode's edits with fakes for the window and the panel: saving and
//  undoing a save, the keys while editing, copying a block, and the
//  question when the window closes with unsaved edits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockEditControllerTests)
{
public:

    static constexpr size_t    kBlockBytes = 512;
    static constexpr size_t    kBlocks     = 1600;
    static constexpr int64_t   kTime       = 1'700'000'000;
    static constexpr uint32_t  kEdited     = 2;
    static constexpr Byte      kValue      = 0xA5;

    //  The edits and the parts they call, over vol.po in a fake file system.
    struct Rig
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        FakeBlockDialogs     dialogs;
        FakeBlockFrame       frame;
        FakeBlockJobs        jobs;
        FakeBlockView        view;
        FakeHostClipboard    clipboard;
        BlockFileSession     session { model, dialogs, frame };
        BlockEditController  edits   { { model, jobs, view, dialogs, frame, session, session, clipboard } };

        Rig()
        {
            vector<Byte>  buffer (kBlocks * kBlockBytes, 0);

            AssertSucceeded (ProDosSkeleton::Write (buffer, "EDITS"));
            io.files["vol.po"]  = buffer;
            io.stamps["vol.po"] = { buffer.size(), kTime };

            session.SetFile (io, L"vol.po");
            session.Attach (jobs, view);
            session.Open();
        }

        void EditOneByte()
        {
            int  overflow = 0;

            model.SelectBlock (kEdited);
            Assert::AreEqual (1, model.Paste (0, L"A5", false, overflow));
        }

        Byte GetFileByte() { return io.files["vol.po"][kEdited * kBlockBytes]; }
    };


    static DxuiKeyEvent MakeKey (WPARAM vk, bool isCtrl)
    {
        DxuiKeyEvent  ev;



        ev.vk   = vk;
        ev.ctrl = isCtrl;

        return ev;
    }


    TEST_METHOD (Save_WritesTheEditToTheFile)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.edits.Save();
        rig.edits.UpdateEditControls();

        Assert::AreEqual (kValue, rig.GetFileByte());
        Assert::IsTrue (rig.model.GetEdits().IsEmpty());
        Assert::AreEqual (std::wstring (L"Saved to vol.po"), rig.view.lastState.note);
    }


    TEST_METHOD (Save_WhileBusyWritesNothing)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.jobs.isBusy = true;
        rig.edits.Save();

        Assert::AreEqual ((Byte) 0, rig.GetFileByte());
        Assert::IsFalse (rig.model.GetEdits().IsEmpty());
    }


    TEST_METHOD (UndoApplied_RestoresTheFile)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.edits.Save();
        rig.edits.UndoApplied();
        rig.edits.UpdateEditControls();

        Assert::AreEqual ((Byte) 0, rig.GetFileByte());
        Assert::AreEqual (std::wstring (L"Undid the save to vol.po"), rig.view.lastState.note);
    }


    TEST_METHOD (Discard_DropsTheEdits)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.edits.Discard();

        Assert::IsTrue (rig.model.GetEdits().IsEmpty());
    }


    TEST_METHOD (HandleKey_CtrlZUndoesAndEscapeStopsEditing)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.view.isEditing = true;

        Assert::IsTrue (rig.edits.HandleKey (MakeKey ('Z', true)));
        Assert::IsTrue (rig.model.GetEdits().IsEmpty());

        Assert::IsTrue (rig.edits.HandleKey (MakeKey (VK_ESCAPE, false)));
        Assert::IsFalse (rig.view.isEditing);

        Assert::IsFalse (rig.edits.HandleKey (MakeKey ('Q', false)));
    }


    TEST_METHOD (CopyBlock_PutsTheBlockTextOnTheClipboard)
    {
        Rig  rig;



        rig.view.blockText = L"0000: A5";
        rig.edits.CopyBlock();

        Assert::AreEqual (std::wstring (L"0000: A5"), rig.clipboard.placedText);
    }


    TEST_METHOD (ConfirmUnsaved_WithNoEditsCloses)
    {
        Rig  rig;



        Assert::IsTrue (rig.edits.ConfirmUnsaved());
        Assert::IsTrue (rig.dialogs.questions.empty());
    }


    TEST_METHOD (ConfirmUnsaved_CancelKeepsTheEdits)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.dialogs.unsavedAnswer = BlockUnsavedChoice::Cancel;

        Assert::IsFalse (rig.edits.ConfirmUnsaved());
        Assert::AreEqual ((size_t) 1, rig.dialogs.questions.size());
        Assert::IsFalse (rig.model.GetEdits().IsEmpty());
    }


    TEST_METHOD (ConfirmUnsaved_DiscardClosesWithoutSaving)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.dialogs.unsavedAnswer = BlockUnsavedChoice::Discard;

        Assert::IsTrue (rig.edits.ConfirmUnsaved());
        Assert::AreEqual ((Byte) 0, rig.GetFileByte());
    }


    TEST_METHOD (ConfirmUnsaved_SaveClosesOnceTheSaveSucceeds)
    {
        Rig  rig;



        rig.EditOneByte();
        rig.dialogs.unsavedAnswer = BlockUnsavedChoice::Save;

        Assert::IsFalse (rig.edits.ConfirmUnsaved(), L"the window stays open until the save finishes");
        Assert::AreEqual (kValue, rig.GetFileByte());
        Assert::AreEqual (1, rig.frame.closes);
    }
};
