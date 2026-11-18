#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35AttachTests
//
//  An image dropped on a 3.5-inch drive on the desk or the drive bar, or
//  chosen from its file dialog, is inserted through
//  MachineStorage::InsertFloppy35, and the error it returns is posted as a
//  notice. Its checks are the ones an image attached in the Machine tab
//  takes when the machine is built.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Floppy35AttachTests)
{
public:

    //  AN IMAGE TOO LARGE FOR THE DRIVE isn't inserted, and the error is the
    //  one the Machine tab's image gets. The drive stays empty, and the file
    //  is as it was.
    TEST_METHOD (Insert_TakesTheMachineTabsChecks)
    {
        FakeDiskFileIo     attachedIo;
        FakeDiskFileIo     droppedIo;
        std::vector<Byte>  image (Gcr35Track::kBlocksTwoSides * 2 * Gcr35Track::kDataBytes, 0x5A);
        std::wstring       error;
        HRESULT            hr = S_OK;



        AddFile (attachedIo, kBigPath, image);
        AddFile (droppedIo,  kBigPath, image);

        TestMachine  attached ("Apple2eEnhanced", TestMachine::Slots::Empty, { UniDiskCard (1, kBigPath) }, attachedIo);
        TestMachine  dropped  ("Apple2eEnhanced", TestMachine::Slots::Empty, { UniDiskCard (1, "") },      droppedIo);

        Assert::AreEqual ((size_t) 1, attached.GetStorage().GetBlockMediaProblems().size(), L"the Machine tab's image isn't attached");

        hr = dropped.GetStorage().InsertFloppy35 (kSlot, 0, kBigPath, error);
        Assert::IsTrue (FAILED (hr), L"the dropped image isn't inserted");
        Assert::IsFalse (error.empty());
        Assert::IsTrue (attached.GetStorage().GetBlockMediaProblems()[0].find (error) != std::wstring::npos,
                        std::format (L"the same error: \"{}\" and \"{}\"", attached.GetStorage().GetBlockMediaProblems()[0], error).c_str());
        Assert::IsTrue (error.find (L"at most 1600") != std::wstring::npos, L"the error states the drive's limit");

        Assert::IsFalse (dropped.GetStorage().FindFloppy35 (kSlot, 0)->HasDisk(), L"the drive stays empty");
        Assert::IsTrue  (droppedIo.files[kBigPath] == image, L"the file is as it was");
    }


    //  WITH A MISSING FILE the drive stays empty, and the error is for the
    //  notice.
    TEST_METHOD (Insert_MissingFile_LeavesTheDriveEmpty)
    {
        FakeDiskFileIo  io;
        std::wstring    error;
        HRESULT         hr = S_OK;



        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { UniDiskCard (1, "") }, io);

        hr = machine.GetStorage().InsertFloppy35 (kSlot, 0, "gone.po", error);
        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty(), L"an error for the notice");
        Assert::IsFalse (machine.GetStorage().FindFloppy35 (kSlot, 0)->HasDisk());
    }


    //  AN IMAGE IN ONE DRIVE isn't inserted in a second.
    TEST_METHOD (Insert_ImageInAnotherDrive_Fails)
    {
        FakeDiskFileIo     io;
        std::vector<Byte>  image (Gcr35Track::kBlocksTwoSides * Gcr35Track::kDataBytes, 0);
        std::wstring       error;
        HRESULT            hr = S_OK;



        AddFile (io, kDiskPath, image);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { UniDiskCard (2, "") }, io);

        AssertSucceeded (machine.GetStorage().InsertFloppy35 (kSlot, 0, kDiskPath, error));
        hr = machine.GetStorage().InsertFloppy35 (kSlot, 1, kDiskPath, error);
        Assert::IsTrue  (FAILED (hr));
        Assert::IsTrue  (error.find (L"another drive") != std::wstring::npos, error.c_str());
        Assert::IsTrue  (machine.GetStorage().FindFloppy35 (kSlot, 0)->HasDisk());
        Assert::IsFalse (machine.GetStorage().FindFloppy35 (kSlot, 1)->HasDisk());
    }


private:

    static constexpr int           kSlot     = 5;
    static constexpr const char *  kBigPath  = "big.po";
    static constexpr const char *  kDiskPath = "d.po";


    static void AddFile (FakeDiskFileIo & io, const std::string & path, const std::vector<Byte> & image)
    {
        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 1 };
    }


    //  The UniDisk 3.5 interface card in slot 5 with `drives` UniDisks, the
    //  first with `image`.
    static TestMachine::BlockCard UniDiskCard (size_t drives, const std::string & image)
    {
        TestMachine::BlockCard  card;



        card.slot   = kSlot;
        card.device = kpszLironCardDevice;
        card.ports.assign (drives, PortConfig {});

        for (PortConfig & port : card.ports)
        {
            port.device = kpszUniDisk35Device;
        }

        card.ports[0].image = image;

        return card;
    }
};
