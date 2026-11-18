#pragma once

#include "Pch.h"

struct CassoLinkBlockDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResumeRecord
//
//  A block device transfer cut short, as NAME.partial.json has it beside
//  NAME.partial (R42): the device by its slot, unit, blocks and volume, a
//  SHA-256 of its volume directory's blocks, the direction, the last block
//  verified and the image's path.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkResumeRecord
{
    static constexpr const char *  kRead  = "read";
    static constexpr const char *  kWrite = "write";

    int           slot              = 0;
    int           unit              = 0;
    uint32_t      blockCount        = 0;
    std::string   volumeName;
    std::string   directoryHash;
    std::string   direction         = kRead;
    int64_t       lastVerifiedBlock = -1;      // -1 before the first
    std::string   imagePath;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResume
//
//  The partial image and its record. A record is written after every
//  kEveryBlocks blocks verified, and a later transfer of the same device in
//  the same direction, whose slot, unit, block count, volume name and
//  directory hash match, continues at lastVerifiedBlock + 1. Pure: no I/O.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkResume
{
public:
    static constexpr uint32_t  kEveryBlocks    = 64;
    static constexpr uint32_t  kDirectoryFirst = 2;      // a ProDOS volume directory's blocks
    static constexpr uint32_t  kDirectoryCount = 4;

    //  NAME.partial and NAME.partial.json beside the image.
    static std::string  GetPartialPath (const std::string & imagePath) { return imagePath + ".partial"; }
    static std::string  GetRecordPath  (const std::string & imagePath) { return imagePath + ".partial.json"; }

    static std::string  Format  (const CassoLinkResumeRecord & record);

    //  ERROR_INVALID_DATA for text that isn't a whole record.
    static HRESULT      Parse   (const std::string & text, CassoLinkResumeRecord & outRecord);

    //  Whether the record is for this device, its directory and this
    //  direction, so the transfer continues from it.
    static bool         Matches (const CassoLinkResumeRecord & record, const CassoLinkBlockDevice & device, const std::string & directoryHash, const std::string & direction);

    //  The SHA-256, in hex, of the volume directory's blocks as read, which
    //  are kDirectoryCount blocks from kDirectoryFirst.
    static HRESULT      HashDirectory (std::span<const Byte> blocks, std::string & outHex);

    //  Whether a block verified, counted from 0, is one a record follows:
    //  every kEveryBlocks.
    static bool         IsCheckpoint (uint32_t verified) { return verified > 0 && verified % kEveryBlocks == 0; }
};
