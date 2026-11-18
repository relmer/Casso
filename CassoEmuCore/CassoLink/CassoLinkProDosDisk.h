#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkProDosDisk
//
//  The CassoLink ProDOS disk (FR-199): a 140K ProDOS disk with the boot
//  blocks, PRODOS and BASIC.SYSTEM from the user's ProDOS disk, CASSOLINK,
//  the virtual drives' installer and driver (LinkProgram::BuildVirtualDrive),
//  and a STARTUP that runs it. Nothing of ProDOS ships with Casso.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkProDosDisk
{
public:
    static constexpr const char *  kVolumeName    = "CASSOLINK";
    static constexpr const char *  kInstallerName = "CASSOLINK";
    static constexpr Byte          kBinaryType    = 0x06;
    static constexpr Byte          kBasicType     = 0xFC;

    //  usersDisk is a 140K ProDOS disk image's file, in any 140K format
    //  VolumeImage loads; outImage is the new disk in DOS 3.3 sector order,
    //  as a .dsk has it. ERROR_FILE_NOT_FOUND when the user's disk lacks
    //  PRODOS or BASIC.SYSTEM.
    static HRESULT  Make (const std::vector<Byte> & usersDisk, const std::string & usersPath, DWORD baud, std::vector<Byte> & outImage);
};
