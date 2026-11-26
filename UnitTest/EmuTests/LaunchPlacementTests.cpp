#include "Pch.h"

#include "Config/LaunchPlacement.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  Fixtures
//
//  Configs laid out as the stock models' are, in memory: the //e's cards in
//  slots 1, 4, 5, 6 and 7 with the stock hard disk at SCSI ID 6, the ][ with
//  48K and a Disk II, the //c's back panel and the //c Plus's internal drive.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint64_t  k140K = 143360;
static constexpr uint64_t  k800K = 819200;
static constexpr uint64_t  k32MB = 33553920;





////////////////////////////////////////////////////////////////////////////////
//
//  MakeSlot
//
////////////////////////////////////////////////////////////////////////////////

static SlotConfig MakeSlot (int slot, const char * device)
{
    SlotConfig  entry;



    entry.slot   = slot;
    entry.device = device;
    return entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeIIe
//
////////////////////////////////////////////////////////////////////////////////

static MachineConfig MakeIIe()
{
    MachineConfig   config;
    SlotConfig      scsi  = MakeSlot (7, kpszAppleScsiCardDevice);
    RamRegion       main;
    InternalDevice  lc;



    main.size = 0xC000;
    lc.type   = kpszLanguageCardDevice;

    scsi.ports.resize (7);
    scsi.ports[6].device = kpszHd20scDevice;
    scsi.ports[6].image  = kpszStockImage;

    config.ram.push_back (main);
    config.internalDevices.push_back (lc);
    config.slots = { MakeSlot (1, "parallel-printer"), MakeSlot (4, "mockingboard-c"), MakeSlot (5, kpszLironCardDevice),
                     MakeSlot (6, kpszDiskIiDevice), scsi };
    return config;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeII
//
////////////////////////////////////////////////////////////////////////////////

static MachineConfig MakeII()
{
    MachineConfig  config;
    RamRegion      main;



    main.size = 0xC000;

    config.ram.push_back (main);
    config.slots = { MakeSlot (1, "parallel-printer"), MakeSlot (6, kpszDiskIiDevice) };
    return config;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeIIc
//
////////////////////////////////////////////////////////////////////////////////

static MachineConfig MakeIIc (bool hasInternal35)
{
    MachineConfig   config = MakeIIe();
    PortConfig      disk;
    PortConfig      internal;



    config.slots.clear();
    disk.name = "disk";

    if (hasInternal35)
    {
        internal.name   = "internal";
        internal.device = kpszApple35DriveDevice;
        config.ports.push_back (internal);
    }

    config.ports.push_back (disk);
    return config;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeTwoImgHeader
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<Byte> MakeTwoImgHeader (uint32_t format, uint32_t dataBytes)
{
    std::vector<Byte>  head (BlockImageFile::kTwoImgHeader);
    auto               put  = [&] (size_t offset, uint32_t value)
    {
        for (size_t i = 0; i < 4; i++)
        {
            head[offset + i] = (Byte) (value >> (8 * i));
        }
    };



    head[0] = '2';
    head[1] = 'I';
    head[2] = 'M';
    head[3] = 'G';
    put (BlockImageFile::kTwoImgFormat,     format);
    put (BlockImageFile::kTwoImgDataOffset, (uint32_t) BlockImageFile::kTwoImgHeader);
    put (BlockImageFile::kTwoImgDataLength, dataBytes);
    return head;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeFacts
//
////////////////////////////////////////////////////////////////////////////////

static DiskImageFacts MakeFacts (LaunchImageKind kind, uint32_t blocks = 0)
{
    DiskImageFacts  facts;



    facts.kind   = kind;
    facts.blocks = blocks;
    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlot
//
////////////////////////////////////////////////////////////////////////////////

static const SlotConfig * FindSlot (const MachineConfig & config, int slot)
{
    for (const SlotConfig & entry : config.slots)
    {
        if (entry.slot == slot)
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasLanguageCard
//
////////////////////////////////////////////////////////////////////////////////

static bool HasLanguageCard (const MachineConfig & config)
{
    return std::any_of (config.internalDevices.begin(), config.internalDevices.end(),
                        [] (const InternalDevice & device) { return device.type == kpszLanguageCardDevice; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFactsTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageFactsTests)
{
public:

    TEST_METHOD (FromHeader_140KSectorImages_Are525)
    {
        std::vector<Byte>  head (ImageClassifier::kHeadBytes);



        for (const wchar_t * extension : { L".dsk", L".do", L".po" })
        {
            Assert::IsTrue (DiskImageFacts::FromHeader (extension, k140K, head).kind == LaunchImageKind::Floppy525, extension);
        }
    }

    TEST_METHOD (FromHeader_NibbleImage_Is525)
    {
        std::vector<Byte>  head (ImageClassifier::kHeadBytes);



        Assert::IsTrue (DiskImageFacts::FromHeader (L".nib", ImageClassifier::kNibBytes, head).kind == LaunchImageKind::Floppy525);
    }

    TEST_METHOD (FromHeader_D13_IsThirteenSector)
    {
        Assert::IsTrue (DiskImageFacts::FromHeader (L".D13", 116480, {}).kind == LaunchImageKind::ThirteenSector);
    }

    TEST_METHOD (FromHeader_800KTwoImg_IsFloppyBlocks)
    {
        std::vector<Byte>  head  = MakeTwoImgHeader (BlockImageFile::kTwoImgProDos, (uint32_t) k800K);
        DiskImageFacts     facts = DiskImageFacts::FromHeader (L".2mg", k800K + BlockImageFile::kTwoImgHeader, head);



        Assert::IsTrue (facts.kind == LaunchImageKind::FloppyBlocks);
        Assert::AreEqual (1600u, facts.blocks);
    }

    TEST_METHOD (FromHeader_140KProDosTwoImg_IsFloppyBlocks)
    {
        std::vector<Byte>  head = MakeTwoImgHeader (BlockImageFile::kTwoImgProDos, (uint32_t) k140K);



        Assert::IsTrue (DiskImageFacts::FromHeader (L".2mg", k140K + BlockImageFile::kTwoImgHeader, head).kind == LaunchImageKind::FloppyBlocks);
    }

    TEST_METHOD (FromHeader_DosOrderTwoImg_IsUnsupported)
    {
        std::vector<Byte>  head = MakeTwoImgHeader (BlockImageFile::kTwoImgDos, (uint32_t) k140K);



        Assert::IsTrue (DiskImageFacts::FromHeader (L".2mg", k140K + BlockImageFile::kTwoImgHeader, head).kind == LaunchImageKind::Unsupported);
    }

    TEST_METHOD (FromHeader_32MBHdv_IsHardDisk)
    {
        std::vector<Byte>  head (ImageClassifier::kHeadBytes);
        DiskImageFacts     facts = DiskImageFacts::FromHeader (L".hdv", k32MB, head);



        Assert::IsTrue (facts.kind == LaunchImageKind::HardDisk);
        Assert::AreEqual (65535u, facts.blocks);
    }

    TEST_METHOD (FromHeader_NotAnImage_IsUnsupported)
    {
        Assert::IsTrue (DiskImageFacts::FromHeader (L".txt", k140K, {}).kind == LaunchImageKind::Unsupported);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacementTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LaunchPlacementTests)
{
public:

    TEST_METHOD (Place_525OnEveryModel_GoesInDrive1)
    {
        for (const StockMachine & machine : StockMachines::GetAll())
        {
            MachineConfig  stock = machine.hasSlots ? (machine.is6502 ? MakeII() : MakeIIe()) : MakeIIc (false);
            LaunchPlan     plan  = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Floppy525), "C:\\a.dsk", machine, stock);



            Assert::IsFalse (plan.IsRefused(), plan.refusal.c_str());
            Assert::AreEqual (std::string ("C:\\a.dsk"), plan.disk1Path);
            Assert::IsTrue (plan.config.isTemporary);
        }
    }

    TEST_METHOD (Place_ThirteenSector_GoesInDrive1)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::ThirteenSector), "C:\\dos32.d13",
                                                   *StockMachines::Find ("Apple2c"), MakeIIc (false));



        Assert::AreEqual (std::string ("C:\\dos32.d13"), plan.disk1Path);
    }

    TEST_METHOD (Place_HardDiskOnIIe_ReplacesTheStockDisk)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::HardDisk, 65535), "C:\\big.hdv",
                                                           StockMachines::GetDefault(), MakeIIe());
        const SlotConfig  * scsi = FindSlot (plan.config, 7);



        Assert::IsFalse (plan.IsRefused());
        Assert::IsTrue (plan.disk1Path.empty());
        Assert::IsTrue (plan.addedSlots.empty());
        Assert::AreEqual (std::string ("C:\\big.hdv"), scsi->ports[6].image);
        Assert::IsFalse (scsi->cassoFirmware);
        Assert::IsTrue (plan.config.isTemporary);
    }

    TEST_METHOD (Place_HardDiskOnII_AddsScsiCardAndLanguageCard)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::HardDisk, 65535), "C:\\big.hdv",
                                                           *StockMachines::Find ("Apple2"), MakeII());
        const SlotConfig  * scsi = FindSlot (plan.config, 7);



        Assert::IsFalse (plan.IsRefused(), plan.refusal.c_str());
        Assert::IsNotNull (scsi);
        Assert::AreEqual (std::string (kpszAppleScsiCardDevice), scsi->device);
        Assert::IsTrue (scsi->cassoFirmware, L"a card added to a 6502 model runs Casso's firmware");
        Assert::AreEqual (std::string ("C:\\big.hdv"), scsi->ports[6].image);
        Assert::IsTrue (plan.addedSlots == std::vector<int> { 7 });
        Assert::IsTrue (HasLanguageCard (plan.config));
    }

    TEST_METHOD (Place_525OnII_AddsLanguageCard)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Floppy525), "C:\\a.dsk",
                                                   *StockMachines::Find ("Apple2"), MakeII());



        Assert::IsTrue (HasLanguageCard (plan.config));
    }

    TEST_METHOD (Place_HardDiskOnIIe_AddsNoLanguageCard)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::HardDisk, 65535), "C:\\big.hdv",
                                                   StockMachines::GetDefault(), MakeIIe());



        Assert::AreEqual (size_t (1), plan.config.internalDevices.size());
    }

    TEST_METHOD (Place_800KOnIIe_PutsUniDiskCardInSlot7)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.2mg",
                                                           StockMachines::GetDefault(), MakeIIe());
        const SlotConfig  * card = FindSlot (plan.config, 7);



        Assert::IsFalse (plan.IsRefused(), plan.refusal.c_str());
        Assert::AreEqual (std::string (kpszLironCardDevice), card->device);
        Assert::AreEqual (std::string (kpszUniDisk35Device), card->ports[0].device);
        Assert::AreEqual (std::string ("C:\\800.2mg"), card->ports[0].image);
        Assert::IsTrue (plan.addedSlots == std::vector<int> { 7 });
    }

    TEST_METHOD (Place_800KOnIIPlus_AddsUniDiskCard)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.2mg",
                                                           *StockMachines::Find ("Apple2Plus"), MakeII());
        const SlotConfig  * card = FindSlot (plan.config, 7);



        Assert::IsFalse (plan.IsRefused(), plan.refusal.c_str());
        Assert::AreEqual (std::string (kpszLironCardDevice), card->device);
        Assert::IsTrue (HasLanguageCard (plan.config));
    }

    TEST_METHOD (Place_800KOnII_IsRefused)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\disks\\800.2mg",
                                                   *StockMachines::Find ("Apple2"), MakeII());



        Assert::IsTrue (plan.IsRefused());
        Assert::IsTrue (plan.refusal.starts_with (L"The Apple ][ can't open 800.2mg: the UniDisk 3.5 interface card works in the "), plan.refusal.c_str());
        Assert::IsTrue (plan.refusal.find (L" and Apple ") != std::wstring::npos, plan.refusal.c_str());
        Assert::IsTrue (plan.config.slots.empty(), L"a plan with a refusal has no config");
    }

    TEST_METHOD (Place_800KOnIIc_GoesOnTheDiskPort)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.2mg",
                                                           *StockMachines::Find ("Apple2c"), MakeIIc (false));
        const PortConfig  * disk = plan.config.FindPort ("disk");



        Assert::IsFalse (plan.IsRefused(), plan.refusal.c_str());
        Assert::AreEqual (size_t (1), disk->chain.size());
        Assert::AreEqual (std::string (kpszUniDisk35Device), disk->chain[0].device);
        Assert::AreEqual (std::string ("C:\\800.2mg"), disk->chain[0].image);
    }

    TEST_METHOD (Place_800KOnIIcPlus_GoesInTheInternalDrive)
    {
        LaunchPlan          plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.2mg",
                                                           *StockMachines::Find ("Apple2cPlus"), MakeIIc (true));
        const PortConfig  * internal = plan.config.FindPort ("internal");



        Assert::AreEqual (std::string ("C:\\800.2mg"), internal->image);
        Assert::IsTrue (plan.config.FindPort ("disk")->chain.empty());
    }

    TEST_METHOD (Place_HardDiskOnIIc_IsRefused)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::HardDisk, 65535), "C:\\big.hdv",
                                                   *StockMachines::Find ("Apple2c"), MakeIIc (false));



        Assert::AreEqual (std::wstring (L"The Apple //c can't open big.hdv: the disk port doesn't take a hard disk image."), plan.refusal);
    }

    TEST_METHOD (Place_NoSlotsAndNoDiskPort_IsRefused)
    {
        MachineConfig  bare;
        LaunchPlan     plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.2mg",
                                                      *StockMachines::Find ("Apple2c"), bare);



        Assert::AreEqual (std::wstring (L"The Apple //c can't open 800.2mg: it has no slot or disk port for the image."), plan.refusal);
    }

    TEST_METHOD (Place_525WithNoDiskII_IsRefused)
    {
        MachineConfig  stock = MakeII();
        LaunchPlan     plan;



        stock.slots.pop_back();
        plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Floppy525), "C:\\a.dsk", *StockMachines::Find ("Apple2Plus"), stock);

        Assert::AreEqual (std::wstring (L"The Apple ][+ can't open a.dsk: it has no 5.25-inch drive."), plan.refusal);
    }

    TEST_METHOD (Place_Unsupported_IsRefused)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Unsupported), "C:\\x.woz",
                                                   StockMachines::GetDefault(), MakeIIe());



        Assert::AreEqual (std::wstring (L"The Apple //e Enhanced can't open x.woz: it isn't a disk image Casso can boot."), plan.refusal);
    }

    //  Runs ResolveFirmware on the plan and returns each path passed to the resolver.
    static std::vector<std::string> ResolveAndRecord (LaunchPlan & plan, const char * machineId)
    {
        std::vector<std::string>             asked;
        std::string                          error;
        MachineConfigLoader::FileResolver    resolver = [&asked] (const std::vector<fs::path> &, const fs::path & relPath)
        {
            asked.push_back (relPath.generic_string());
            return fs::path();
        };



        Assert::AreEqual (S_OK, LaunchPlacement::ResolveFirmware (plan, machineId, {}, resolver, error), TextEncoding::NarrowToWide (error).c_str());
        return asked;
    }

    TEST_METHOD (Place_BlockImageOn6502Models_StartsSlot7)
    {
        LaunchPlan  plus = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.po",
                                                   *StockMachines::Find ("Apple2Plus"), MakeII());
        LaunchPlan  ii   = LaunchPlacement::Place (MakeFacts (LaunchImageKind::HardDisk, 65535), "C:\\big.hdv",
                                                   *StockMachines::Find ("Apple2"), MakeII());



        Assert::IsFalse (plus.IsRefused(), plus.refusal.c_str());
        Assert::IsFalse (ii.IsRefused(), ii.refusal.c_str());
        Assert::AreEqual (7, plus.config.bootSlot);
        Assert::AreEqual (7, ii.config.bootSlot);
    }

    TEST_METHOD (Place_BlockImageOnIIe_LeavesTheBootToTheRom)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.po",
                                                   StockMachines::GetDefault(), MakeIIe());



        Assert::AreEqual (0, plan.config.bootSlot);
    }

    //  The unenhanced //e's ROM passes over a card without the Disk II's
    //  signature, so the card's slot is started for it.
    TEST_METHOD (Place_BlockImageOnUnenhancedIIe_StartsSlot7)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.po",
                                                   *StockMachines::Find ("Apple2e"), MakeIIe());



        Assert::AreEqual (LaunchPlacement::kBootSlot, plan.config.bootSlot);
    }

    //  The ][ monitor doesn't boot a disk, so a floppy starts slot 6.
    TEST_METHOD (Place_FloppyOnII_StartsSlot6)
    {
        LaunchPlan  plan = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Floppy525, 280), "C:\a.dsk",
                                                   *StockMachines::Find ("Apple2"), MakeIIe());



        Assert::AreEqual (std::string ("C:\a.dsk"), plan.disk1Path);
        Assert::AreEqual (LaunchPlacement::kDiskIiSlot, plan.config.bootSlot);
    }

    //  The //c Plus has no built-in 5.25-inch drive, so one goes on its disk
    //  port for the image.
    TEST_METHOD (Place_FloppyOnIIcPlus_AddsADriveToTheDiskPort)
    {
        LaunchPlan          plan     = LaunchPlacement::Place (MakeFacts (LaunchImageKind::Floppy525, 280), "C:\a.dsk",
                                                               *StockMachines::Find ("Apple2cPlus"), MakeIIc (true));
        const PortConfig  * diskPort = plan.config.FindPort ("disk");



        Assert::IsTrue   (plan.refusal.empty());
        Assert::IsNotNull (diskPort);
        Assert::AreEqual (std::string (kpszDiskIicDevice), diskPort->chain.back().device);
        Assert::AreEqual (0, plan.config.bootSlot);
    }

    TEST_METHOD (ResolveFirmware_800KOnIIc_LooksUpTheUniDiskRom)
    {
        LaunchPlan                plan  = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.po",
                                                                  *StockMachines::Find ("Apple2c"), MakeIIc (false));
        std::vector<std::string>  asked = ResolveAndRecord (plan, "Apple2c");



        Assert::IsTrue (std::ranges::find (asked, "Devices/UniDisk35/UniDisk35.rom") != asked.end());
    }

    TEST_METHOD (ResolveFirmware_800KOnIIe_LooksUpTheAddedCardsDriveRom)
    {
        LaunchPlan                plan  = LaunchPlacement::Place (MakeFacts (LaunchImageKind::FloppyBlocks, 1600), "C:\\800.po",
                                                                  StockMachines::GetDefault(), MakeIIe());
        std::vector<std::string>  asked = ResolveAndRecord (plan, "Apple2eEnhanced");



        Assert::IsTrue (std::ranges::find (asked, "Devices/UniDisk35/UniDisk35.rom") != asked.end());
    }};
