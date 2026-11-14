#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Device ids of the SmartPort and hard disk cards and drives, as the
//  machine JSON lists them.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char *  kpszAppleScsiCardDevice   = "apple-scsi-card";
static constexpr const char *  kpszHd20scDevice          = "hd20sc";
static constexpr const char *  kpszProFileCardDevice     = "profile-card";
static constexpr const char *  kpszProFile5Device        = "profile-5mb";
static constexpr const char *  kpszProFile10Device       = "profile-10mb";
static constexpr const char *  kpszLironCardDevice       = "liron-card";
static constexpr const char *  kpszUniDisk35Device       = "unidisk35";
static constexpr const char *  kpszApple35CardDevice     = "apple35-card";
static constexpr const char *  kpszApple35DriveDevice    = "apple35-drive";
static constexpr const char *  kpszDiskIicDevice         = "disk-iic";
static constexpr const char *  kpszStockImage            = "stock";





////////////////////////////////////////////////////////////////////////////////
//
//  BlockCardSpec
//
//  What the configuration and the Machine tab need to know about a card: its
//  firmware's folder and size, how many ports it has and what attaches to
//  them, and the machines that can host it.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockCardSpec
{
    const char                       * device          = nullptr;
    const char                       * label           = nullptr;
    const char                       * firmwareDir     = nullptr;
    size_t                             firmwareSize    = 0;
    const char                       * defaultFirmware = nullptr;
    int                                portCount       = 0;
    bool                               isChain         = false;
    std::span<const char * const>      drives;
    std::span<const char * const>      hosts;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDriveSpec
//
////////////////////////////////////////////////////////////////////////////////

struct BlockDriveSpec
{
    const char  * device        = nullptr;
    const char  * label         = nullptr;
    bool          hasRealTiming = false;
    uint32_t      maxBlocks     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceCatalog
//
//  The SmartPort and hard disk cards and drives Casso emulates.
//
////////////////////////////////////////////////////////////////////////////////

class BlockDeviceCatalog
{
public:
    static constexpr uint32_t  kProDosMaxBlocks = 65535;

    static std::span<const BlockCardSpec>   GetCards  ();
    static std::span<const BlockDriveSpec>  GetDrives ();

    static const BlockCardSpec   * FindCard  (const std::string & device);
    static const BlockDriveSpec  * FindDrive (const std::string & device);

    //  True when the card can be hosted by the machine, or when the machine
    //  is not one Casso ships, whose hardware Casso cannot judge.
    static bool  CanHost       (const BlockCardSpec & card, const std::string & machineId);
    static bool  AcceptsDrive  (const BlockCardSpec & card, const std::string & drive);

    //  The machines that can host the card, as display names separated by
    //  commas, for an error or a tip.
    static std::string  GetHostList (const BlockCardSpec & card);

    static const char *  GetMachineLabel (const std::string & machineId);
};
