#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/PreservedCopy.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/DiskBay.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BayFileAccessTests
//
//  Every read, stat and write goes through the installed hooks. Each test
//  installs fakes, so no test opens a real file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BayFileAccessTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\disks\\Work.dsk";
    static constexpr time_t        kWhen      = 1700000000;



    struct Rig
    {
        DiskStoreHooks                        hooks;
        BayFileAccess                         files { hooks };
        FakeDiskFileIo                        fileIo;
        std::map<string, vector<Byte>>        written;

        Rig()
        {
            hooks.fileIo    = &fileIo;
            hooks.timestamp = [] () { return kWhen; };
            hooks.flushSink = [this] (const string & path, const vector<Byte> & bytes) -> HRESULT
                              {
                                  written[path] = bytes;
                                  return S_OK;
                              };
        }
    };


    static void MountSectorImage (DiskBay & bay)
    {
        vector<Byte>  sectors (NibbleFormat::kImageByteSize, 0x3C);



        bay.image   = make_unique<DiskImage>();
        bay.path    = kImagePath;
        bay.mounted = true;

        AssertSucceeded (Nibblizer::NibblizeDsk (sectors, *bay.image));
    }


    TEST_METHOD (ReadImageFile_GoesThroughTheReader)
    {
        Rig           rig;
        vector<Byte>  bytes;
        string        asked;



        rig.hooks.imageReader = [&asked] (const string & path, vector<Byte> & out) -> HRESULT
                                {
                                    asked = path;
                                    out.assign (3, 0x7E);
                                    return S_OK;
                                };

        AssertSucceeded (rig.files.ReadImageFile (kImagePath, bytes));
        Assert::AreEqual (string (kImagePath), asked);
        Assert::AreEqual ((size_t) 3, bytes.size());
    }


    TEST_METHOD (ReadImageFile_ReportsTheReadersFailure)
    {
        Rig           rig;
        vector<Byte>  bytes;



        rig.hooks.imageReader = [] (const string &, vector<Byte> &) -> HRESULT
                                {
                                    return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
                                };

        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                          (int) rig.files.ReadImageFile (kImagePath, bytes));
    }


    TEST_METHOD (ReadIdentity_GoesThroughTheIdentityReader)
    {
        Rig            rig;
        ImageIdentity  identity;



        rig.hooks.identityReader = [] (const string &)
                                   {
                                       ImageIdentity  id;

                                       id.recorded = true;
                                       return id;
                                   };

        identity = rig.files.ReadIdentity (kImagePath);

        Assert::IsTrue (identity.recorded);
    }


    TEST_METHOD (DoesPathExist_AsksTheFileSeam)
    {
        Rig  rig;



        rig.fileIo.files["C:\\disks\\Here.dsk"] = vector<Byte> (1, 0);

        Assert::IsTrue  (rig.files.DoesPathExist ("C:\\disks\\Here.dsk"));
        Assert::IsFalse (rig.files.DoesPathExist ("C:\\disks\\Gone.dsk"));
    }


    TEST_METHOD (WritePreserved_GoesThroughTheFlushSink)
    {
        Rig  rig;



        AssertSucceeded (rig.files.WritePreserved ("C:\\disks\\Copy.dsk", vector<Byte> (4, 0x11)));

        Assert::AreEqual ((size_t) 1, rig.written.size());
        Assert::AreEqual ((size_t) 4, rig.written["C:\\disks\\Copy.dsk"].size());
    }


    TEST_METHOD (FindFreePreservedPath_StepsPastATakenName)
    {
        Rig     rig;
        string  stamp = PreservedCopy::MakeStamp (kWhen);
        string  first = PreservedCopy::MakePath (kImagePath, stamp, 0);
        string  found;



        rig.fileIo.files[first] = vector<Byte> (1, 0);

        AssertSucceeded (rig.files.FindFreePreservedPath (kImagePath, found));

        Assert::AreEqual (PreservedCopy::MakePath (kImagePath, stamp, 1), found);
    }


    TEST_METHOD (SaveLoadedImage_UsesAReservedPathAsItIs)
    {
        Rig      rig;
        DiskBay  bay;
        string   path = "C:\\disks\\Reserved.dsk";



        MountSectorImage (bay);

        AssertSucceeded (rig.files.SaveLoadedImage (bay, path));

        Assert::AreEqual (string ("C:\\disks\\Reserved.dsk"), path);
        Assert::AreEqual ((size_t) NibbleFormat::kImageByteSize, rig.written[path].size());
    }


    TEST_METHOD (SaveLoadedImage_PicksAFreePathWhenNoneIsReserved)
    {
        Rig      rig;
        DiskBay  bay;
        string   path;



        MountSectorImage (bay);

        AssertSucceeded (rig.files.SaveLoadedImage (bay, path));

        Assert::AreEqual (PreservedCopy::MakePath (kImagePath, PreservedCopy::MakeStamp (kWhen), 0), path);
        Assert::AreEqual ((size_t) 1, rig.written.count (path));
    }


    TEST_METHOD (TryWriteRecoveryImage_WritesAWozBesideTheImage)
    {
        Rig      rig;
        DiskBay  bay;
        string   path;



        MountSectorImage (bay);

        AssertSucceeded (rig.files.TryWriteRecoveryImage (bay, path));

        Assert::AreEqual (BayFileAccess::MakeRecoveryPath (kImagePath, 0), path);
        Assert::AreEqual ((size_t) 1, rig.written.count (path));
    }


    TEST_METHOD (TryWriteRecoveryImage_EmptyBayWritesNothing)
    {
        Rig      rig;
        DiskBay  bay;
        string   path;



        AssertFailed (rig.files.TryWriteRecoveryImage (bay, path));

        Assert::IsTrue (path.empty());
        Assert::AreEqual ((size_t) 0, rig.written.size());
    }


    TEST_METHOD (MakeRecoveryPath_NeverCollidesWithAnEarlierRescue)
    {
        // Overwriting a previous recovery would repeat the mistake being fixed:
        // one rescue must never cost another.
        string  first  = BayFileAccess::MakeRecoveryPath ("C:\\disks\\Work.dsk", 0);
        string  second = BayFileAccess::MakeRecoveryPath ("C:\\disks\\Work.dsk", 1);



        Assert::AreEqual (string ("C:\\disks\\Work.recovered.woz"), first);
        Assert::AreNotEqual (first, second, L"a second rescue must take a different name");
    }
};
