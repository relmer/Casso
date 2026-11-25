#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeActionsTests
//
//  Each action and each answer on a bay table filled directly. Fakes replace
//  the store's repoint, eject and reload, and the hooks capture
//  every write, report and question.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExternalChangeActionsTests)
{
public:

    static constexpr int           kSlot      = 6;
    static constexpr int           kDrive     = 0;
    static constexpr const char *  kImagePath = "C:\\disks\\Work.dsk";



    struct FakeStore : public IBayRepoint, public IBayLifecycle
    {
        vector<string>  repointed;
        int             ejects       = 0;
        int             lostEjects   = 0;
        int             reloads      = 0;
        HRESULT         reloadResult = S_OK;

        HRESULT RepointBayToFile (int, int, const string & newPath) override
        {
            repointed.push_back (newPath);
            return S_OK;
        }

        void Eject (int, int) override
        {
            ejects++;
        }

        void EjectLostImage (int, int) override
        {
            lostEjects++;
        }

        HRESULT MountExternallyModifiedDisk (int, int, const vector<Byte> &) override
        {
            reloads++;
            return reloadResult;
        }
    };


    struct Rig
    {
        DiskStoreHooks                    hooks;
        DiskBayTable                      bays;
        BayFileAccess                     files   { hooks };
        FakeStore                         store;
        ExternalChangeActions             actions { bays, hooks, files, store, store };
        FakeDiskFileIo                    fileIo;
        std::map<string, vector<Byte>>    written;
        vector<ChangePrompt>              reports;
        vector<ChangePrompt>              questions;
        int                               restarts = 0;

        Rig()
        {
            hooks.fileIo          = &fileIo;
            hooks.timestamp       = [] () { return (time_t) 1700000000; };
            hooks.restartCallback = [this] () { restarts++; };
            hooks.flushSink       = [this] (const string & path, const vector<Byte> & bytes) -> HRESULT
                                    {
                                        written[path] = bytes;
                                        return S_OK;
                                    };
            hooks.reportSink      = [this] (int, int, const ChangePrompt & prompt)
                                    {
                                        reports.push_back (prompt);
                                    };
            hooks.askSink         = [this] (int, int, const ChangePrompt & prompt)
                                    {
                                        questions.push_back (prompt);
                                        return true;
                                    };
        }

        DiskBay & Mount()
        {
            DiskBay       & bay     = bays.GetBay (kSlot, kDrive);
            vector<Byte>    sectors (NibbleFormat::kImageByteSize, 0x24);



            bay.image   = make_unique<DiskImage>();
            bay.path    = kImagePath;
            bay.format  = DiskFormat::Dsk;
            bay.mounted = true;

            AssertSucceeded (Nibblizer::NibblizeDsk (sectors, *bay.image));
            bay.image->SetSourceFormat (DiskFormat::Dsk);

            return bay;
        }
    };


    TEST_METHOD (ReloadInPlace_ReloadsAndReports)
    {
        Rig  rig;



        rig.Mount();
        rig.actions.CarryOutChangeAction (kSlot, kDrive, ChangeAction::ReloadInPlace,
                                          vector<Byte> (4, 0), ChangeAuthor::AnotherProgram);

        Assert::AreEqual (1, rig.store.reloads);
        Assert::AreEqual (0, rig.restarts);
        Assert::AreEqual ((size_t) 1, rig.reports.size());
    }


    TEST_METHOD (Restart_ReloadsAndRestartsTheMachine)
    {
        Rig  rig;



        rig.Mount();
        rig.actions.CarryOutChangeAction (kSlot, kDrive, ChangeAction::Restart,
                                          vector<Byte> (4, 0), ChangeAuthor::CassoCli);

        Assert::AreEqual (1, rig.store.reloads);
        Assert::AreEqual (1, rig.restarts);
    }


    TEST_METHOD (Restart_FailedReload_DoesNotRestart)
    {
        Rig  rig;



        rig.Mount();
        rig.store.reloadResult = E_FAIL;

        rig.actions.CarryOutChangeAction (kSlot, kDrive, ChangeAction::Restart,
                                          vector<Byte> (4, 0), ChangeAuthor::CassoCli);

        Assert::AreEqual (1, rig.store.reloads);
        Assert::AreEqual (0, rig.restarts, L"a disk that did not reload is not restarted on");
    }


    TEST_METHOD (KeepHeld_WritesACopyAndMovesTheBayOntoIt)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        rig.actions.CarryOutChangeAction (kSlot, kDrive, ChangeAction::KeepHeld,
                                          vector<Byte>(), ChangeAuthor::AnotherProgram);

        Assert::AreEqual ((size_t) 1, rig.written.size());
        Assert::AreEqual ((size_t) 1, rig.store.repointed.size());
        Assert::AreEqual (rig.written.begin()->first, rig.store.repointed[0]);
        Assert::AreEqual (0, rig.store.reloads, L"keeping the disk does not reload it");
        Assert::IsFalse (bay.image->IsDirty());
    }


    TEST_METHOD (Resolve_WithNoQuestionOutstanding_DoesNothing)
    {
        Rig  rig;



        rig.Mount();
        rig.actions.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        Assert::AreEqual (0, rig.store.ejects);
        Assert::AreEqual (0, rig.store.lostEjects);
        Assert::AreEqual (0, rig.store.reloads);
    }


    TEST_METHOD (Resolve_DiscardForALostFile_EjectsWithoutFlushing)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        bay.sharedState.SetAskOutstanding (true);
        bay.sharedState.SetAskedAction (ChangeAction::Deleted);

        rig.actions.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        Assert::AreEqual (1, rig.store.lostEjects);
        Assert::AreEqual (0, rig.store.ejects);
        Assert::IsFalse (bay.sharedState.IsAskOutstanding());
    }


    TEST_METHOD (Resolve_SaveAsForALostFile_KeepsTheDiskOnTheChosenFile)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        bay.sharedState.SetAskOutstanding (true);
        bay.sharedState.SetAskedAction (ChangeAction::Deleted);

        rig.actions.ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, "C:\\saved\\Work.dsk");

        Assert::AreEqual ((size_t) 1, rig.written.count ("C:\\saved\\Work.dsk"));
        Assert::AreEqual ((size_t) 1, rig.store.repointed.size());
        Assert::AreEqual (string ("C:\\saved\\Work.dsk"), rig.store.repointed[0]);
        Assert::AreEqual (0, rig.store.lostEjects, L"a disk with a file to live in stays in the drive");
    }


    TEST_METHOD (Resolve_DiscardDuringAnEject_FinishesTheEject)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        bay.image->WriteBit (0, 0, static_cast<uint8_t> (bay.image->ReadBit (0, 0) ^ 1));
        bay.sharedState.SetAskOutstanding (true);
        bay.sharedState.SetAskedAction (ChangeAction::Conflict);
        bay.sharedState.SetEjectWhenAnswered (true);

        rig.actions.ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard);

        Assert::AreEqual (1, rig.store.ejects);
        Assert::IsFalse (bay.image->IsDirty(), L"the discarded writes go with the disk");
        Assert::IsFalse (bay.sharedState.IsEjectWhenAnswered());
    }
};
