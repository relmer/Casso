#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSosDisk
//
//  The CassoLink SOS disk (FR-212): a 140K disk that starts an Apple ///,
//  with the SOS loader, SOS.KERNEL and SOS.DRIVER from the user's SOS disk
//  and the link's SOS program as SOS.INTERP (LinkProgram::
//  BuildSosInterpreter). Nothing of SOS ships with Casso. SOS.DRIVER must
//  have .RS232, which the user adds with Apple's System Configuration
//  Program when it's missing.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkSosDisk
{
public:
    static constexpr const char *  kVolumeName      = "CASSOLINK";
    static constexpr const char *  kInterpreterName = "SOS.INTERP";
    static constexpr Byte          kSosSystemType   = 0x0C;

    //  usersDisk is a 140K SOS disk image's file, in any 140K format
    //  VolumeImage loads; outImage is the new disk in DOS 3.3 sector order,
    //  as a .dsk has it. ERROR_NOT_SUPPORTED for a speed the Apple ///'s
    //  serial port doesn't have. When the user's disk doesn't start an
    //  Apple ///, outError has the reason.
    static HRESULT  Make (const std::vector<Byte> & usersDisk, const std::string & usersPath, DWORD baud, std::vector<Byte> & outImage, std::string & outError);
};
