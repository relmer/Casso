#include "Pch.h"

#include "Config/LaunchPlacement.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceAttachments.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/BootDriveRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFacts::FromHeader
//
////////////////////////////////////////////////////////////////////////////////

DiskImageFacts DiskImageFacts::FromHeader (std::wstring_view extension, uint64_t fileBytes, std::span<const Byte> head)
{
    DiskImageFacts  facts;



    if (_wcsicmp (std::wstring (extension).c_str(), L".d13") == 0)
    {
        facts.kind = LaunchImageKind::ThirteenSector;
        return facts;
    }

    return FromClass (ImageClassifier::ClassifyHeader (extension, fileBytes, head));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFacts::Read
//
////////////////////////////////////////////////////////////////////////////////

DiskImageFacts DiskImageFacts::Read (IDiskFileIo & fileIo, const std::string & path)
{
    DiskImageFacts  facts;



    if (_wcsicmp (fs::path (path).extension().wstring().c_str(), L".d13") == 0)
    {
        facts.kind = LaunchImageKind::ThirteenSector;
        return facts;
    }

    return FromClass (ImageClassifier::Classify (fileIo, path));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFacts::FromClass
//
//  A Disk II mounts sector, nibble and WOZ images. A 5.25-inch image in a
//  2IMG or Disk Copy container is blocks, so it goes on a 3.5-inch drive,
//  which takes a 140K block image. A 3.5-inch WOZ image isn't one the block
//  drives read.
//
////////////////////////////////////////////////////////////////////////////////

DiskImageFacts DiskImageFacts::FromClass (const ImageClass & imageClass)
{
    DiskImageFacts  facts;
    bool            isDiskIiLayout = imageClass.layout == ImageLayout::Sector
                                  || imageClass.layout == ImageLayout::Nibble
                                  || imageClass.layout == ImageLayout::Woz;



    if (!imageClass.IsClassified())
    {
        return facts;
    }

    facts.blocks = imageClass.blockCount;

    if (imageClass.formFactor == ImageFormFactor::Floppy525 && isDiskIiLayout)
    {
        facts.kind = LaunchImageKind::Floppy525;
    }
    else if (imageClass.blockCount == 0 || imageClass.layout == ImageLayout::Woz)
    {
        facts.kind = LaunchImageKind::Unsupported;
    }
    else if (imageClass.formFactor == ImageFormFactor::HardDisk)
    {
        facts.kind = LaunchImageKind::HardDisk;
    }
    else if (imageClass.formFactor == ImageFormFactor::Floppy525 || imageClass.formFactor == ImageFormFactor::Floppy35)
    {
        facts.kind = LaunchImageKind::FloppyBlocks;
    }

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::Place
//
//  A plan with a refusal has no config. A copy with less than 64K gets a language
//  card, since ProDOS requires 64K.
//
////////////////////////////////////////////////////////////////////////////////

LaunchPlan LaunchPlacement::Place (
    const DiskImageFacts  & facts,
    const std::string     & imagePath,
    const StockMachine    & machine,
    const MachineConfig   & stockConfig)
{
    LaunchPlan    plan;
    std::wstring  reason;
    bool          isBlocks = facts.kind == LaunchImageKind::FloppyBlocks || facts.kind == LaunchImageKind::HardDisk;



    plan.config             = stockConfig;
    plan.config.isTemporary = true;

    if (facts.kind == LaunchImageKind::Unsupported)
    {
        reason = L"it isn't a disk image Casso can boot";
    }
    else if (!isBlocks)
    {
        bool  hasDrive = !machine.hasSlots || plan.config.AttachedDiskIiDriveCount() > 0;



        if (hasDrive)
        {
            plan.disk1Path = imagePath;
        }
        else
        {
            reason = L"it has no 5.25-inch drive";
        }
    }
    else if (machine.hasSlots)
    {
        PlaceOnCard (plan, facts, imagePath, machine, reason);
    }
    else
    {
        PlaceOnPort (plan, facts, imagePath, reason);
    }

    if (!reason.empty())
    {
        plan            = LaunchPlan();
        plan.refusal    = std::format (L"The {} can't open {}: {}.",
                                       machine.label,
                                       fs::path (imagePath).filename().wstring(),
                                       reason);
        return plan;
    }

    if (plan.config.GetTotalRamK() < (uint32_t) kMinimumRamK)
    {
        AddLanguageCard (plan.config);
    }

    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::ResolveFirmware
//
//  A card the placement added has no firmware path yet, and neither does a
//  drive it put on the disk port. A card the catalog doesn't list for the
//  model runs Casso's firmware, so only its drives are looked up.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LaunchPlacement::ResolveFirmware (
    LaunchPlan                                & plan,
    const std::string                         & machineId,
    const std::vector<fs::path>               & searchPaths,
    const MachineConfigLoader::FileResolver   & resolver,
    std::string                               & outError)
{
    HRESULT                    hr         = S_OK;
    const MachineDefinition  * definition = MachineDefinitions::Find (machineId);



    for (SlotConfig & slot : plan.config.slots)
    {
        const BlockCardSpec  * card    = BlockDeviceCatalog::FindCard (slot.device);
        bool                   isAdded = std::ranges::find (plan.addedSlots, slot.slot) != plan.addedSlots.end();



        if (!isAdded || card == nullptr)
        {
            continue;
        }

        if (BlockDeviceCatalog::CanHost (*card, machineId))
        {
            MachineConfigLoader::ResolveBlockCard (machineId, searchPaths, resolver, slot);
            continue;
        }

        for (PortConfig & port : slot.ports)
        {
            MachineConfigLoader::ResolveDriveFirmware (searchPaths, resolver, port, slot.deviceProblem);
        }
    }

    hr = MachineConfigLoader::ResolveDiskPortChains (searchPaths, resolver, plan.config.ports, outError,
                                                     definition != nullptr ? (size_t) definition->diskPortFloppyCount : 1,
                                                     definition != nullptr ? (size_t) definition->diskPortApple35Count : 0);
    CHR (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::PlaceOnPort
//
//  A model without slots: a 3.5-inch-size image goes in the //c Plus's
//  internal drive, else on the first drive of the disk port. The disk port
//  doesn't take a hard disk.
//
////////////////////////////////////////////////////////////////////////////////

void LaunchPlacement::PlaceOnPort (LaunchPlan & plan, const DiskImageFacts & facts, const std::string & imagePath, std::wstring & outReason)
{
    PortConfig                                   * internal  = nullptr;
    std::vector<BlockDeviceAttachments::Attachment>  smartPort (1);
    std::string                                    error;
    HRESULT                                        hr        = S_OK;
    bool                                           hasDiskPort = plan.config.FindPort ("disk") != nullptr;



    if (!hasDiskPort)
    {
        outReason = L"it has no slot or disk port for the image";
        return;
    }

    if (facts.kind == LaunchImageKind::HardDisk)
    {
        outReason = L"the disk port doesn't take a hard disk image";
        return;
    }

    for (PortConfig & port : plan.config.ports)
    {
        if (port.name == BootDriveRule::kpszInternalPort && port.device == kpszApple35DriveDevice)
        {
            internal = &port;
        }
    }

    if (internal != nullptr)
    {
        internal->image = imagePath;
        return;
    }

    smartPort[0].chain = 1;
    smartPort[0].image = imagePath;

    hr = BlockDeviceAttachments::Apply ({}, smartPort, plan.config, error);

    if (FAILED (hr))
    {
        outReason = TextEncoding::NarrowToWide (error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::PlaceOnCard
//
//  A hard disk image goes on the copy's first SCSI card, or on one added in
//  slot 7. A 3.5-inch-size image goes on a UniDisk 3.5 interface card in
//  slot 7, since the ROM boots from slot 7 first and slot 6's Disk II, with
//  no disk, stops the search before slot 5.
//
////////////////////////////////////////////////////////////////////////////////

void LaunchPlacement::PlaceOnCard (LaunchPlan & plan, const DiskImageFacts & facts, const std::string & imagePath, const StockMachine & machine, std::wstring & outReason)
{
    const BlockCardSpec                            * liron  = BlockDeviceCatalog::FindCard (kpszLironCardDevice);
    std::vector<BlockDeviceAttachments::Attachment>  hd (1);
    std::string                                      error;
    HRESULT                                          hr     = S_OK;
    int                                              slot   = 0;



    if (facts.kind == LaunchImageKind::HardDisk)
    {
        for (const SlotConfig & entry : plan.config.slots)
        {
            if (slot == 0 && entry.enabled && entry.device == kpszAppleScsiCardDevice)
            {
                slot = entry.slot;
            }
        }

        if (slot == 0)
        {
            slot = PutCardInBootSlot (plan, kpszAppleScsiCardDevice, machine.is6502).slot;
        }
    }
    else
    {
        if (!BlockDeviceCatalog::CanHost (*liron, machine.id))
        {
            outReason = std::format (L"the {} works in the {}",
                                     TextEncoding::NarrowToWide (liron->label),
                                     TextEncoding::NarrowToWide (BlockDeviceCatalog::GetHostList (*liron)));
            return;
        }

        slot = PutCardInBootSlot (plan, kpszLironCardDevice, machine.is6502).slot;
    }

    hd[0].slot  = slot;
    hd[0].image = imagePath;

    hr = BlockDeviceAttachments::Apply (hd, {}, plan.config, error);

    if (FAILED (hr))
    {
        outReason = TextEncoding::NarrowToWide (error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::PutCardInBootSlot
//
//  Replaces whatever slot 7 has in the copy. A card added to a 6502 model
//  runs Casso's block firmware, which uses 6502 instructions only.
//
////////////////////////////////////////////////////////////////////////////////

SlotConfig & LaunchPlacement::PutCardInBootSlot (LaunchPlan & plan, const char * device, bool is6502)
{
    const BlockCardSpec  * card  = BlockDeviceCatalog::FindCard (device);
    SlotConfig           * entry = nullptr;



    for (SlotConfig & existing : plan.config.slots)
    {
        if (existing.slot == kBootSlot)
        {
            entry = &existing;
        }
    }

    if (entry == nullptr)
    {
        plan.config.slots.push_back (SlotConfig());
        entry = &plan.config.slots.back();
    }

    *entry               = SlotConfig();
    entry->slot          = kBootSlot;
    entry->device        = device;
    entry->firmware      = card->defaultFirmware;
    entry->cassoFirmware = is6502 && card->hasCassoFirmware;

    plan.addedSlots.push_back (kBootSlot);

    return *entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement::AddLanguageCard
//
////////////////////////////////////////////////////////////////////////////////

void LaunchPlacement::AddLanguageCard (MachineConfig & config)
{
    InternalDevice  card;



    card.type = kpszLanguageCardDevice;
    config.internalDevices.push_back (card);
}
