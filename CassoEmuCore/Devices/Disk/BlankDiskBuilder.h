#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskBuilder
//
//  Pure blank-disk generation (spec 017, FR-013): a BlankDiskSpec describes
//  the disk to create — format, contents, bootable — and Build produces the
//  complete on-disk bytes for it. No filesystem, window, or registry access:
//  boot payloads arrive as caller-supplied bytes and the output is a byte
//  vector the SHELL writes to a file. All-or-nothing — on failure the output
//  is untouched (FR-011 starts here).
//
//  Formats:
//      Woz — skeleton sector buffer nibblized (NibblizationLayer) into a
//            DiskImage, serialized by WozLoader::Serialize (WOZ v2).
//      Dsk — the 143,360-byte sector buffer itself, DOS 3.3 order.
//      Po  — the 143,360-byte sector buffer itself, ProDOS order.
//
//  Contents pairing (FR-010): Dsk carries DOS 3.3 or unformatted; Po carries
//  ProDOS or unformatted; Woz carries anything (order-agnostic bit stream).
//
////////////////////////////////////////////////////////////////////////////////

enum class BlankDiskContents
{
    Unformatted,
    Dos33,
    ProDos,
};



//
//  The user's choices from the create dialog. Defaults match FR-004: WOZ,
//  DOS 3.3, pre-formatted data-only.
//
struct BlankDiskSpec
{
    DiskFormat         format       = DiskFormat::Woz;
    BlankDiskContents  contents     = BlankDiskContents::Dos33;
    bool               bootable     = false;
    Byte               volumeNumber = NibblizationLayer::kDefaultVolume;
    std::string        volumeName   = "NEWDISK";   // ProDOS only
};



//
//  Caller-loaded boot-payload bytes (whole 143,360-byte master sector
//  images). Empty vectors mean "not available"; Build fails a bootable spec
//  whose matching payload is empty — availability is the shell's problem.
//
struct BootPayload
{
    vector<Byte>  dosMasterSectors;
    vector<Byte>  proDosUsersDisk;
};



class BlankDiskBuilder
{
public:
    //  Validates the format/contents/bootable pairing (FR-010).
    static HRESULT  ValidateSpec (const BlankDiskSpec & spec);

    //  Produces the complete image bytes for the spec. All-or-nothing.
    static HRESULT  Build (const BlankDiskSpec & spec,
                           const BootPayload   & payload,
                           vector<Byte>        & outBytes);
};
