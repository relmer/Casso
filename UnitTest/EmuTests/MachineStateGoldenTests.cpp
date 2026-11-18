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
#include "Machines/Apple2/Common/NibblizationLayer.h"
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
        { "Apple2",          { 0x677F9A147CBD7C61, 0xA1608BE850420816, 0x161D24ACBCA2C45D } },
        { "Apple2Plus",      { 0xD157218F89A4A970, 0x83F8A8894DC6C79F, 0xEF14DF19AE36726F } },
        { "Apple2e",         { 0x3A221ECD58EBDD0F, 0xC79488DCE4E4548B, 0x57051D7C4EDE49A1 } },
        { "Apple2eEnhanced", { 0xEFDDF56AB0AF2ABC, 0x850B684D42ADE4EB, 0x271298BDE374DD65 } },
        { "Apple2c",         { 0x7AD37EC7509D7A10, 0xD27EE87BA2F51963, 0x4EB7F7EAF6D7FC0C } },
        { "Apple2cPlus",     { 0x2A76FF7A754AB8F4, 0xBEAD975C5FA83378, 0xB4078C28C5EDF422 } },
    };

    static constexpr uint64_t  kEveryCard[kPoints] = { 0x69ABB331C6BAA55B, 0xA01827CE7B8E38B3, 0xBBF6233D5CB4A893 };


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

        AssertSucceeded (machine.InsertFloppy35 (5, 0, "unidisk.po", error), L"the UniDisk 3.5's disk");
        AssertSucceeded (machine.InsertFloppy35 (4, 0, "apple35.po", error), L"the 3.5 drive's disk");

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



        machine.RunCycles (kRunFrames * kFrameCycles);
        actual[0] = SaveChecksum (machine);

        machine.SoftReset();
        machine.RunCycles (kRunFrames * kFrameCycles);
        actual[1] = SaveChecksum (machine);

        machine.PowerCycle();
        machine.RunCycles (kRunFrames * kFrameCycles);
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



        hr = machine.SaveState (writer);
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
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
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
