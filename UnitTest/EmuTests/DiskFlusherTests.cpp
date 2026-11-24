#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFlusherTests
//
//  The flusher on a bay table filled directly. The hooks capture every write
//  and stat, and a fake collects each call to RepointBayToFile.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFlusherTests)
{
public:

    static constexpr int           kSlot      = 6;
    static constexpr int           kDrive     = 1;
    static constexpr const char *  kImagePath = "C:\\disks\\Work.dsk";



    struct FakeRepoint : public IBayRepoint
    {
        vector<string>  paths;

        HRESULT RepointBayToFile (int slot, int drive, const string & newPath) override
        {
            (void) slot;
            (void) drive;
            paths.push_back (newPath);
            return S_OK;
        }
    };


    struct Rig
    {
        DiskStoreHooks                    hooks;
        DiskBayTable                      bays;
        BayFileAccess                     files   { hooks };
        FakeRepoint                       repoint;
        DiskFlusher                       flusher { bays, hooks, files, repoint };
        FakeDiskFileIo                    fileIo;
        std::map<string, vector<Byte>>    written;
        vector<ChangePrompt>              reports;
        HRESULT                           sinkResult = S_OK;

        Rig()
        {
            hooks.fileIo     = &fileIo;
            hooks.timestamp  = [] () { return (time_t) 1700000000; };
            hooks.flushSink  = [this] (const string & path, const vector<Byte> & bytes) -> HRESULT
                               {
                                   written[path] = bytes;
                                   return sinkResult;
                               };
            hooks.reportSink = [this] (int, int, const ChangePrompt & prompt)
                               {
                                   reports.push_back (prompt);
                               };
        }

        DiskBay & MountDirty()
        {
            DiskBay       & bay     = bays.GetBay (kSlot, kDrive);
            vector<Byte>    sectors (NibbleFormat::kImageByteSize, 0x5A);



            bay.image   = make_unique<DiskImage>();
            bay.path    = kImagePath;
            bay.format  = DiskFormat::Dsk;
            bay.mounted = true;

            AssertSucceeded (Nibblizer::NibblizeDsk (sectors, *bay.image));
            bay.image->SetSourceFormat (DiskFormat::Dsk);
            bay.image->WriteBit (0, 0, static_cast<uint8_t> (bay.image->ReadBit (0, 0) ^ 1));

            return bay;
        }
    };


    static inline int  s_notifyCount = 0;

    static void CountNotify (const wchar_t *)
    {
        s_notifyCount++;
    }

    //  Routes EhmNotifyUser to the counter for one scope.
    struct ScopedNotifyCapture
    {
        ScopedNotifyCapture()  { s_notifyCount = 0; SetNotifyFunction (CountNotify); }
        ~ScopedNotifyCapture() { SetNotifyFunction (nullptr); }
    };


    static ImageIdentity MakeIdentity (uint64_t size)
    {
        ImageIdentity  identity;



        identity.recorded        = true;
        identity.stamp.sizeBytes = size;

        return identity;
    }


    TEST_METHOD (Flush_CleanImage_WritesNothing)
    {
        Rig        rig;
        DiskBay  & bay = rig.MountDirty();



        bay.image->ClearDirty();

        AssertSucceeded (rig.flusher.Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 0, rig.written.size());
    }


    TEST_METHOD (Flush_DirtyImage_WritesTheWholeImageToItsPath)
    {
        Rig        rig;
        DiskBay  & bay = rig.MountDirty();



        AssertSucceeded (rig.flusher.Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) NibbleFormat::kImageByteSize, rig.written[kImagePath].size());
        Assert::IsFalse (bay.image->IsDirty(), L"a written image is clean");
    }


    TEST_METHOD (Flush_WriteProtectedImage_DropsTheWritesWithoutWriting)
    {
        Rig        rig;
        DiskBay  & bay = rig.MountDirty();



        bay.image->SetUserWriteProtected (true);

        AssertSucceeded (rig.flusher.Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 0, rig.written.size());
        Assert::IsFalse (bay.image->IsDirty());
    }


    TEST_METHOD (Flush_SinkFailure_KeepsTheImageDirty)
    {
        Rig        rig;
        DiskBay  & bay = rig.MountDirty();
        HRESULT    hr  = S_OK;



        rig.sinkResult = E_ACCESSDENIED;

        {
            ScopedNotifyCapture  capture;

            hr = rig.flusher.Flush (kSlot, kDrive);
        }

        Assert::AreEqual ((int) E_ACCESSDENIED, (int) hr);
        Assert::AreEqual (1, s_notifyCount, L"the loss is reported to the user");
        Assert::IsTrue (bay.image->IsDirty(), L"a failed write leaves the writes for a retry");
    }


    TEST_METHOD (Flush_FileChangedSinceMount_MovesTheWritesToACopyAndRepoints)
    {
        Rig        rig;
        DiskBay  & bay = rig.MountDirty();



        bay.sharedState.Mount (MakeIdentity (100));
        rig.hooks.identityReader = [] (const string &) { return MakeIdentity (200); };

        AssertSucceeded (rig.flusher.Flush (kSlot, kDrive));

        Assert::AreEqual ((size_t) 0, rig.written.count (kImagePath),
            L"the other program's version of the file is not written over");
        Assert::AreEqual ((size_t) 1, rig.written.size(), L"the guest's version goes to a copy");
        Assert::AreEqual ((size_t) 1, rig.repoint.paths.size());
        Assert::AreEqual (rig.written.begin()->first, rig.repoint.paths[0],
            L"and the bay moves onto that copy");
        Assert::AreEqual ((size_t) 1, rig.reports.size());
    }


    TEST_METHOD (FlushAll_WritesEveryDirtyBay)
    {
        Rig        rig;
        DiskBay  & first  = rig.MountDirty();
        DiskBay  & second = rig.bays.GetBay (5, 0);



        second.image   = make_unique<DiskImage>();
        second.path    = "C:\\disks\\Other.dsk";
        second.mounted = true;
        AssertSucceeded (Nibblizer::NibblizeDsk (vector<Byte> (NibbleFormat::kImageByteSize, 0x11), *second.image));
        second.image->SetSourceFormat (DiskFormat::Dsk);
        second.image->WriteBit (0, 0, static_cast<uint8_t> (second.image->ReadBit (0, 0) ^ 1));

        AssertSucceeded (rig.flusher.FlushAll());

        Assert::AreEqual ((size_t) 2, rig.written.size());
        Assert::IsFalse (first.image->IsDirty());
        Assert::IsFalse (second.image->IsDirty());
    }
};
