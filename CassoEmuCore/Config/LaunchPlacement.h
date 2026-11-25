#pragma once

#include "Pch.h"

#include "Config/StockMachines.h"
#include "Core/MachineConfig.h"

class IDiskFileIo;
struct ImageClass;





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchImageKind
//
//  Which drive a temporary machine boots an image from.
//
//  Floppy525       A Disk II image: .dsk, .do, .po, .woz, .nib, .nb2.
//  ThirteenSector  A .d13, also in drive 1, with the 13-sector ROM.
//  FloppyBlocks    A 140K, 400K or 800K block image, for a UniDisk 3.5 or an
//                  Apple 3.5 Drive. The SCSI card's drive detaches an image
//                  of these sizes.
//  HardDisk        Any other block count, for the SCSI card.
//  Unsupported     Anything else.
//
////////////////////////////////////////////////////////////////////////////////

enum class LaunchImageKind
{
    Unsupported,
    Floppy525,
    ThirteenSector,
    FloppyBlocks,
    HardDisk,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFacts
//
//  An image's kind and block count, from ImageClassifier's reading of its
//  extension, length and header.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskImageFacts
{
    LaunchImageKind  kind   = LaunchImageKind::Unsupported;
    uint32_t         blocks = 0;

    //  `extension` includes the dot. `head` is the file's first bytes, up to
    //  ImageClassifier::kHeadBytes.
    static DiskImageFacts  FromHeader (std::wstring_view extension, uint64_t fileBytes, std::span<const Byte> head);
    static DiskImageFacts  Read       (IDiskFileIo & fileIo, const std::string & path);

private:
    static DiskImageFacts  FromClass  (const ImageClass & imageClass);
};





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlan
//
//  config      The temporary copy, with `isTemporary` set and a block image on
//              its drive.
//  disk1Path   Drive 1's image, for a 5.25-inch or 13-sector image.
//  addedSlots  The slots of cards added to the copy. ResolveFirmware looks
//              up their firmware.
//  refusal     Why the model can't open the image; empty when it can.
//
////////////////////////////////////////////////////////////////////////////////

struct LaunchPlan
{
    MachineConfig     config;
    std::string       disk1Path;
    std::vector<int>  addedSlots;
    std::wstring      refusal;

    bool  IsRefused () const { return !refusal.empty(); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  LaunchPlacement
//
//  Where a temporary copy of a stock machine mounts an image, and what the
//  copy gains to mount it.
//
////////////////////////////////////////////////////////////////////////////////

class LaunchPlacement
{
public:
    static constexpr int  kBootSlot      = 7;
    static constexpr int  kDiskIiSlot    = 6;
    static constexpr int  kMinimumRamK   = 64;

    static LaunchPlan  Place (const DiskImageFacts  & facts,
                              const std::string     & imagePath,
                              const StockMachine    & machine,
                              const MachineConfig   & stockConfig);

    //  Looks up the firmware of the cards the plan added and of the drives
    //  on the disk port, which the stock config's load didn't include.
    static HRESULT     ResolveFirmware (LaunchPlan                                & plan,
                                        const std::string                         & machineId,
                                        const std::vector<fs::path>               & searchPaths,
                                        const MachineConfigLoader::FileResolver   & resolver,
                                        std::string                               & outError);

private:
    static void          PlaceOnPort       (LaunchPlan & plan, const DiskImageFacts & facts, const std::string & imagePath, std::wstring & outReason);
    static void          PlaceOnCard       (LaunchPlan & plan, const DiskImageFacts & facts, const std::string & imagePath, const StockMachine & machine, std::wstring & outReason);
    static SlotConfig &  PutCardInBootSlot (LaunchPlan & plan, const char * device, bool is6502);
    static void          AddLanguageCard   (MachineConfig & config);
};
