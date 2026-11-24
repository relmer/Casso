#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/MountedImageState.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangeMonitorTests
//
//  Recording a change and deciding what it leads to, on a bay table filled
//  directly. The clock, the file reads and the stat are hooks, and a fake
//  collects each action passed to CarryOutChangeAction.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExternalChangeMonitorTests)
{
public:

    static constexpr int           kSlot      = 6;
    static constexpr int           kDrive     = 0;
    static constexpr const char *  kImagePath = "C:\\disks\\Work.dsk";
    static constexpr int64_t       kSettledMs = 60 * 1000;



    struct FakeActions : public IChangeActions
    {
        vector<ChangeAction>  actions;
        vector<ChangeAuthor>  authors;

        void CarryOutChangeAction (int, int, ChangeAction action, const vector<Byte> &, ChangeAuthor author) override
        {
            actions.push_back (action);
            authors.push_back (author);
        }
    };


    struct Rig
    {
        DiskStoreHooks         hooks;
        DiskBayTable           bays;
        BayFileAccess          files     { hooks };
        FakeActions            actions;
        ExternalChangeMonitor  monitor   { bays, hooks, files, actions };
        FakeDiskFileIo         fileIo;
        int64_t                nowMs     = 1000;
        uint64_t               fileSize  = 100;
        int                    questions = 0;

        Rig()
        {
            hooks.fileIo         = &fileIo;
            hooks.clock          = [this] () { return nowMs; };
            hooks.identityReader = [this] (const string &) { return MakeIdentity (fileSize); };
            hooks.imageReader    = [] (const string &, vector<Byte> & out) -> HRESULT
                                   {
                                       out.assign (NibbleFormat::kImageByteSize, 0x42);
                                       return S_OK;
                                   };
            hooks.askSink        = [this] (int, int, const ChangePrompt &)
                                   {
                                       questions++;
                                       return true;
                                   };

            fileIo.files[kImagePath] = vector<Byte> (1, 0);
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
            bay.sharedState.Mount (MakeIdentity (fileSize));

            return bay;
        }
    };


    static ImageIdentity MakeIdentity (uint64_t size)
    {
        ImageIdentity  identity;



        identity.recorded        = true;
        identity.stamp.sizeBytes = size;

        return identity;
    }


    TEST_METHOD (NoteExternalChange_OnAPathNoBayHas_RecordsNothing)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        rig.monitor.NoteExternalChange ("C:\\disks\\Other.dsk", ExternalChangeIntent::ReloadInPlace);

        Assert::IsFalse (bay.sharedState.GetPending().seen);
    }


    TEST_METHOD (ApplyPendingReload_BeforeTheQuietPeriod_DoesNothing)
    {
        Rig  rig;



        rig.Mount();
        rig.fileSize = 200;
        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual ((size_t) 0, rig.actions.actions.size());
    }


    TEST_METHOD (ApplyPendingReload_StatedReload_CarriesItOutForTheCli)
    {
        Rig  rig;



        rig.Mount();
        rig.fileSize = 200;
        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);
        rig.nowMs += kSettledMs;

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, rig.actions.actions.size());
        Assert::IsTrue (rig.actions.actions[0] == ChangeAction::ReloadInPlace);
        Assert::IsTrue (rig.actions.authors[0] == ChangeAuthor::CassoCli);
    }


    TEST_METHOD (ApplyPendingReload_FromCassoExplorer_CreditsCassoExplorer)
    {
        Rig  rig;



        rig.Mount();
        rig.fileSize = 200;
        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace, true);
        rig.nowMs += kSettledMs;

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual ((size_t) 1, rig.actions.authors.size());
        Assert::IsTrue (rig.actions.authors[0] == ChangeAuthor::CassoExplorer);
    }


    TEST_METHOD (ApplyPendingReload_IdentityUnchanged_DropsTheChange)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);
        rig.nowMs += kSettledMs;

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual ((size_t) 0, rig.actions.actions.size());
        Assert::IsFalse (bay.sharedState.GetPending().seen, L"a change that changed nothing is cleared");
    }


    TEST_METHOD (ApplyPendingReload_WhileAQuestionStands_Waits)
    {
        Rig        rig;
        DiskBay  & bay = rig.Mount();



        bay.sharedState.SetAskOutstanding (true);
        rig.fileSize = 200;
        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::ReloadInPlace);
        rig.nowMs += kSettledMs;

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual ((size_t) 0, rig.actions.actions.size());
        Assert::IsTrue (bay.sharedState.GetPending().seen, L"the change waits behind the answer");
    }


    TEST_METHOD (ApplyPendingReload_UnstatedChange_PutsAQuestion)
    {
        Rig  rig;



        rig.Mount();
        rig.fileSize = 200;
        rig.monitor.NoteExternalChange (kImagePath, ExternalChangeIntent::Unstated);
        rig.nowMs += kSettledMs;

        rig.monitor.ApplyPendingReload();

        Assert::AreEqual (1, rig.questions);
        Assert::AreEqual ((size_t) 0, rig.actions.actions.size(), L"nothing is done until it is answered");
    }
};
