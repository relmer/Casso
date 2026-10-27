#include "Pch.h"
#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleNibWriteTests
//
//  DOS 3.3 saving to a nibble image, from the guest's side. A nibble file
//  has each byte the drive read and none of the timing between them, so its
//  gaps are plain $FF bytes. A guest write ends on a bit the file's bytes do
//  not start on, and the drive has to find the next field after it anyway.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleNibWriteTests)
{
public:

    static constexpr int           kSlot6   = 6;
    static constexpr int           kDrive2  = 1;
    static constexpr const char *  kNibPath = "gate.nib";



    //  Two programs saved to a new DOS 3.3 disk made as a .nib, in drive 2,
    //  the motor stopping between them: DOS reports no error and catalogs
    //  both, and the file saved from the drive has both.
    TEST_METHOD (DosSavesTwiceToANibAndTheSavedFileHasBoth)
    {
        TestMachine               machine ("Apple2e");
        DiskImageStore          & store   = machine.GetDiskStore();
        std::vector<Byte>         master  = GuestSession::RequireDos33Master();
        BlankDiskSpec             spec;
        BootPayload               boot;
        DiskImage                 reloaded;
        std::vector<Byte>         nib;
        std::vector<Byte>         saved;
        std::vector<Byte>         sectors;
        std::vector<std::string>  rows;
        SectorDecodeReport        report;
        SectorDecodeReport        live;
        FilePayload               payload;



        spec.format           = DiskFormat::Nib;
        spec.contents         = BlankDiskContents::Dos33;
        spec.bootable         = true;
        spec.nibbleTrackSize  = NibbleImageCodec::kNibTrackSize;
        boot.dosMasterSectors = master;

        AssertSucceeded (BlankDiskBuilder::Build (spec, boot, nib), L"a new DOS 3.3 .nib builds");

        //  Every save goes to memory, and only the .nib's bytes are stored.
        store.SetFlushSink ([&saved] (const std::string & path, const std::vector<Byte> & bytes)
        {
            if (path == kNibPath)
            {
                saved = bytes;
            }

            return S_OK;
        });

        GuestSession::BootToPrompt (machine, master);

        AssertSucceeded (store.MountFromBytes (kSlot6, kDrive2, kNibPath, DiskFormat::Nib, nib), L"the .nib mounts");

        rows = GuestSession::TypeAndCollect (machine, "10 PRINT 1");
        rows = GuestSession::TypeAndCollect (machine, "SAVE FIRST,D2");
        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"the first save reports no error");

        rows = GuestSession::TypeAndCollect (machine, "SAVE SECOND,D2");
        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"nor does the second, on tracks the first wrote");

        rows = GuestSession::TypeAndCollect (machine, "CATALOG,D2");
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "FIRST"),  L"the catalog lists the first");
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "SECOND"), L"and the second");

        //  DOS reads only the sectors a command uses, so every sector on the
        //  disk in the drive is decoded too: the field after each write on its
        //  track has to be readable.
        sectors = GuestSession::DecodeThroughTheDrive (*store.GetImage (kSlot6, kDrive2), live);
        Assert::AreEqual (0, live.GetUnrecoveredCount(), L"every sector on the disk in the drive reads");

        AssertSucceeded (store.Flush (kSlot6, kDrive2), L"the disk saves");
        Assert::IsFalse (saved.empty(), L"to the file");

        reloaded.LoadFromBytes (DiskFormat::Nib, saved, kNibPath);
        Assert::IsTrue (reloaded.IsLoaded(), L"which loads");

        sectors = GuestSession::DecodeThroughTheDrive (reloaded, report);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("FIRST"),  payload), L"the saved file has the first program");
            AssertSucceeded (volume.Read (FilePath::Parse ("SECOND"), payload), L"and the second");
        }
    }
};
