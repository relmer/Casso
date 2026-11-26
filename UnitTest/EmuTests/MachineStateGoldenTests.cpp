#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateWriter.h"
#include "Core/TextEncoding.h"
#include "Debugger/Reverse/KeyframeCodec.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "FakeDiskFileIo.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Shell/MachineStateStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateGoldenTests
//
//  The saved-state bytes of every shipped machine, and of an enhanced //e
//  with every storage card, at fixed points of a fixed run: a power-on with a
//  pattern disk spinning, a reset and a power cycle. Each state's checksum is
//  compared with a constant, so a change to the layout of any part's state
//  fails here. A change that is meant to alter the layout updates the
//  constants and the state version in the same commit.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineStateGoldenTests)
{
public:

    static constexpr uint64_t  kFrameCycles  = 17030;
    static constexpr uint64_t  kRunFrames    = 40;
    static constexpr int       kDiskSlot     = 6;
    static constexpr int       kDiskDrive    = 0;
    static constexpr size_t    kPatternStep  = 7;
    static constexpr int       kPoints       = 3;

    struct Golden
    {
        const char  * machineId     = nullptr;
        uint64_t      sums[kPoints] = {};
    };

    static constexpr Golden  kShipped[] =
    {
        { "Apple2",          { 0xB354162F3A72E6BA, 0xF002EFD774877666, 0xF80029FD2E707605 } },
        { "Apple2Plus",      { 0xCE98AB57647E671A, 0x3682B8DC5D6AEA19, 0xE9F7D4325469BBE0 } },
        { "Apple2e",         { 0xCD95DAFE77A459AC, 0xA88BF5450FD339D9, 0x74D8EA8940357593 } },
        { "Apple2eEnhanced", { 0x434870331E020C65, 0xE02767AF824ACC3B, 0x5B25621F6EB80CF2 } },
        { "Apple2c",         { 0x18CB5D28EC3C729A, 0xFB782172F97B1198, 0xDA7E37B1CD30C4E8 } },
        { "Apple2cPlus",     { 0x73183A8DF3D9FE10, 0xB0715C130377B6A8, 0xFAE4E40E96374147 } },
    };

    static constexpr uint64_t  kEveryCard[kPoints] = { 0xE2923322BC22D7AD, 0xFA05D6D4F29B9DB5, 0x9B68BC3E2693B746 };


    TEST_METHOD (EachShippedMachineSavesTheSameBytes)
    {
        std::wstring  mismatches;



        for (const Golden & golden : kShipped)
        {
            TestMachine  machine (golden.machineId);



            MountPatternDisk (machine);
            mismatches += CheckRun (machine, golden.sums, TextEncoding::NarrowToWide (golden.machineId));
        }

        Assert::AreEqual (std::wstring(), mismatches);
    }


    TEST_METHOD (AMachineWithEveryStorageCardSavesTheSameBytes)
    {
        FakeDiskFileIo                       io;
        std::vector<TestMachine::BlockCard>  cards;
        std::vector<Byte>                    floppy (Gcr35Track::kBlocksTwoSides * Gcr35Track::kDataBytes, 0);
        std::wstring                         error;
        size_t                               i = 0;



        for (i = 0; i < floppy.size(); i++)
        {
            floppy[i] = static_cast<Byte> (i * kPatternStep);
        }

        io.files["unidisk.po"]  = floppy;
        io.stamps["unidisk.po"] = FileStamp { floppy.size(), 1 };
        io.files["apple35.po"]  = floppy;
        io.stamps["apple35.po"] = FileStamp { floppy.size(), 1 };

        cards.push_back ({ 7, kpszAppleScsiCardDevice, std::vector<PortConfig> (AppleScsiCard::kTargets) });
        cards.push_back ({ 5, kpszLironCardDevice,     std::vector<PortConfig> (1) });
        cards.push_back ({ 4, kpszApple35CardDevice,   std::vector<PortConfig> (1) });
        cards.push_back ({ 2, kpszProFileCardDevice,   std::vector<PortConfig> (1) });

        cards[1].ports[0].device = kpszUniDisk35Device;
        cards[2].ports[0].device = kpszApple35DriveDevice;
        cards[3].ports[0].device = kpszProFile5Device;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, cards, io);

        machine.PowerCycle();

        AssertSucceeded (machine.GetStorage().InsertFloppy35 (5, 0, "unidisk.po", error), L"the UniDisk 3.5's disk");
        AssertSucceeded (machine.GetStorage().InsertFloppy35 (4, 0, "apple35.po", error), L"the 3.5 drive's disk");

        Assert::AreEqual (std::wstring(), CheckRun (machine, kEveryCard, L"every storage card"));
    }


    //  Saves the state after a run from power-on, after a reset and a run, and
    //  after a power cycle and a run, and returns a line for each checksum
    //  that differs from its constant. Every actual checksum is logged, so a
    //  deliberate layout change can copy them.
    static std::wstring CheckRun (TestMachine & machine, const uint64_t (&sums)[kPoints], const std::wstring & label)
    {
        uint64_t      actual[kPoints] = {};
        int           point           = 0;
        std::wstring  mismatches;



        machine.GetRunner().RunCycles (kRunFrames * kFrameCycles);
        actual[0] = SaveChecksum (machine);

        machine.SoftReset();
        machine.GetRunner().RunCycles (kRunFrames * kFrameCycles);
        actual[1] = SaveChecksum (machine);

        machine.PowerCycle();
        machine.GetRunner().RunCycles (kRunFrames * kFrameCycles);
        actual[2] = SaveChecksum (machine);

        Logger::WriteMessage (std::format (L"{}: 0x{:016X}, 0x{:016X}, 0x{:016X}\n", label, actual[0], actual[1], actual[2]).c_str());

        for (point = 0; point < kPoints; point++)
        {
            if (sums[point] != actual[point])
            {
                mismatches += std::format (L"{}, state {}: the checksum is 0x{:016X} and the constant is 0x{:016X}\n", label, point, actual[point], sums[point]);
            }
        }

        return mismatches;
    }


    static uint64_t SaveChecksum (const MachineHost & machine)
    {
        StateWriter        writer;
        std::vector<Byte>  bytes;
        HRESULT            hr = S_OK;



        hr = machine.GetStateStream().SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        bytes = writer.GetBytes();
        ClearMediaIds (bytes);

        return KeyframeCodec::ComputeChecksum (bytes.data(), bytes.size());
    }


    //  The header's media IDs come from a counter shared by every disk image
    //  the process creates, so they depend on which tests ran first. They are
    //  zeroed before the checksum.
    static void ClearMediaIds (std::vector<Byte> & bytes)
    {
        MachineStateStream::MediaIds  ids  = {};
        std::vector<Byte>             block;
        std::vector<Byte>::iterator   at;
        size_t                        i    = 0;
        HRESULT                       hr   = S_OK;



        hr = MachineStateStream::ReadSavedMedia (bytes, ids);
        AssertSucceeded (hr, L"ReadSavedMedia");

        for (uint64_t id : ids)
        {
            for (i = 0; i < sizeof (id); i++)
            {
                block.push_back (static_cast<Byte> (id >> (i * CHAR_BIT)));
            }
        }

        at = std::search (bytes.begin(), bytes.end(), block.begin(), block.end());
        Assert::IsTrue (at != bytes.end(), L"the media IDs are in the header");

        std::fill (at, at + block.size(), Byte { 0 });
    }


    //  A DOS-order image of a byte pattern in slot 6, drive 1, after a power
    //  on, when the machine has a Disk II there.
    static void MountPatternDisk (TestMachine & machine)
    {
        std::vector<Byte>  raw (NibbleFormat::kImageByteSize, 0);
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        machine.PowerCycle();

        if (machine.GetRefs().diskController == nullptr)
        {
            return;
        }

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * kPatternStep);
        }

        hr = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDiskDrive, "pattern.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        machine.GetRefs().diskController->SetExternalDisk (kDiskDrive, machine.GetDiskStore().GetImage (kDiskSlot, kDiskDrive));
    }
};
