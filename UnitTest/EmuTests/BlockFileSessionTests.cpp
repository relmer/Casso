#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeBlockDialogs.h"
#include "FakeBlockFrame.h"
#include "FakeBlockJobs.h"
#include "FakeBlockView.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Ui/DiskInspector/Blocks/BlockFileSession.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFileSessionTests
//
//  Opening, reading again and comparing block mode's file, and the
//  window's title, with fakes for the window and the panel and a fake file
//  system.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockFileSessionTests)
{
public:

    static constexpr size_t   kBlockBytes = 512;
    static constexpr size_t   kBlocks     = 1600;
    static constexpr int64_t  kTime       = 1'700'000'000;
    static constexpr size_t   kJunkBytes  = 10;

    //  A session with the fakes it calls, over vol.po in a fake file system.
    struct Rig
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        FakeBlockDialogs     dialogs;
        FakeBlockFrame       frame;
        FakeBlockJobs        jobs;
        FakeBlockView        view;
        BlockFileSession     session { model, dialogs, frame };

        Rig()
        {
            AddImage ("vol.po");
            session.SetFile (io, L"vol.po");
            session.Attach (jobs, view);
        }

        void AddImage (const std::string & path)
        {
            vector<Byte>  buffer (kBlocks * kBlockBytes, 0);

            AssertSucceeded (ProDosSkeleton::Write (buffer, "SESSION"));
            io.files[path]  = buffer;
            io.stamps[path] = { buffer.size(), kTime };
        }
    };


    TEST_METHOD (Open_ReadsTheImage)
    {
        Rig  rig;



        rig.session.Open();

        Assert::IsTrue (rig.model.IsOpen());
        Assert::AreEqual (1, rig.view.refreshes);
        Assert::IsTrue (rig.dialogs.warnings.empty());
        Assert::AreEqual (std::wstring (L"Opening vol.po"), rig.jobs.labels[0]);
    }


    TEST_METHOD (Open_FileThatCantBeReadWarnsAndCloses)
    {
        Rig  rig;



        rig.session.SetFile (rig.io, L"missing.po");
        rig.session.Open();

        Assert::IsFalse (rig.model.IsOpen());
        Assert::AreEqual ((size_t) 1, rig.dialogs.warnings.size());
        Assert::AreEqual (1, rig.frame.closes);
    }


    TEST_METHOD (Title_IsSetOnlyWhenItChanges)
    {
        Rig  rig;



        rig.session.Open();
        rig.session.UpdateTitle();
        rig.session.UpdateTitle();

        Assert::AreEqual (1, rig.frame.captions);
        Assert::AreEqual (std::wstring (L"Block inspector - vol.po"), rig.frame.caption);
    }


    TEST_METHOD (Title_DisplayNameReplacesTheFileName)
    {
        Rig  rig;



        rig.session.SetDisplayName (L"Partition 2: GAMES");
        rig.session.UpdateTitle();

        Assert::AreEqual (std::wstring (L"Block inspector - Partition 2: GAMES"), rig.frame.caption);
    }


    TEST_METHOD (NoteFileGone_AddsTheNote)
    {
        Rig  rig;



        rig.session.Open();
        rig.session.NoteFileGone();

        Assert::AreEqual (std::wstring (L"Block inspector - vol.po (the file can't be read)"), rig.frame.caption);
    }


    TEST_METHOD (Compare_FileThatIsntBlocksWarns)
    {
        Rig  rig;



        rig.io.files["notes.txt"]  = vector<Byte> (kJunkBytes, 'a');
        rig.io.stamps["notes.txt"] = { kJunkBytes, kTime };

        rig.session.Open();
        rig.session.CompareWithFile (L"notes.txt");

        Assert::AreEqual ((size_t) 1, rig.dialogs.warnings.size());
        Assert::IsFalse (rig.model.IsComparing());
    }


    TEST_METHOD (Compare_AddsTheNoteAndEndRemovesIt)
    {
        Rig  rig;



        rig.AddImage ("b.po");
        rig.session.Open();
        rig.session.CompareWithFile (L"b.po");

        Assert::IsTrue (rig.model.IsComparing());
        Assert::AreEqual (std::wstring (L"Block inspector - vol.po compared with b.po"), rig.frame.caption);

        rig.session.EndComparison();

        Assert::IsFalse (rig.model.IsComparing());
        Assert::AreEqual (std::wstring (L"Block inspector - vol.po"), rig.frame.caption);
    }


    TEST_METHOD (CompareWithPicked_ComparesTheChosenImage)
    {
        Rig  rig;



        rig.AddImage ("b.po");
        rig.dialogs.host.path = L"b.po";
        rig.session.Open();
        rig.session.CompareWithPicked();

        Assert::AreEqual (1, rig.dialogs.host.opens);
        Assert::IsTrue (rig.model.IsComparing());
    }


    TEST_METHOD (Reload_WhileBusyWaitsForTheJob)
    {
        Rig  rig;



        rig.session.Open();
        rig.jobs.isBusy = true;
        rig.session.Reload();

        Assert::AreEqual ((size_t) 1, rig.jobs.labels.size(), L"no job is queued while one runs");

        rig.jobs.isBusy = false;
        rig.session.RunPendingReload();

        Assert::AreEqual (std::wstring (L"Reading the file again"), rig.jobs.labels.back());
    }


    TEST_METHOD (Reload_WithEditsPendingNotesTheChange)
    {
        Rig  rig;
        int  overflow = 0;



        rig.session.Open();
        rig.model.SelectBlock (2);
        Assert::AreEqual (1, rig.model.Paste (0, L"A5", false, overflow));

        rig.io.stamps["vol.po"] = { kBlocks * kBlockBytes, kTime + 1 };
        rig.session.Reload();

        Assert::IsTrue (rig.frame.caption.find (L"(the file changed; save or discard to read it again)") != std::wstring::npos);
        Assert::IsTrue (rig.frame.markAt != std::wstring::npos, L"the unsaved edit is marked");
    }


    TEST_METHOD (IsOnCard_ForACardPartition)
    {
        Rig  rig;



        Assert::IsFalse (rig.session.IsOnCard());

        rig.session.SetFile (rig.io, L"\\\\.\\PhysicalDrive3|#1.po");

        Assert::IsTrue (rig.session.IsOnCard());
    }
};
