#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskBuilder
//
//  Pure blank-disk generation: a BlankDiskSpec describes the disk to create
//  — format, contents, bootable — and Build produces the complete on-disk
//  bytes for it. No filesystem, window, or registry access: boot payloads
//  arrive as caller-supplied bytes and the output is a byte vector the SHELL
//  writes to a file. All-or-nothing — on failure the output is untouched, so
//  a failed create can never leave a partial image behind.
//
//  Formats:
//      Woz — skeleton sector buffer nibblized (NibblizationLayer) into a
//            DiskImage, serialized by WozLoader::Serialize (WOZ v2).
//      Dsk — the 143,360-byte sector buffer itself, DOS 3.3 order.
//      Do  — the same bytes as Dsk, under the other extension a
//            DOS-ordered image is found by.
//      Po  — the 143,360-byte sector buffer itself, ProDOS order.
//
//  Contents pairing: Dsk and Do carry DOS 3.3 or unformatted; Po carries
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
//  Why a spec cannot be written, in precedence order: the first failing rule
//  is the one reported.
//
//  A VERDICT RATHER THAN AN HRESULT, because `disk create` reaches these rules
//  with words somebody typed. E_INVALIDARG marks a caller's bug and asserts,
//  which is the wrong answer for a combination the command line openly offers;
//  and a single failure code cannot say WHICH rule was broken, so the sentence
//  the reader gets has to name all of them at once.
//
enum class BlankDiskVerdict
{
    Ok,
    ContentsNotInContainer,
    BootableNeedsFilesystem,
    ProDosNameUnusable,
};



//
//  The user's choices from the create dialog. Defaults are the immediately-
//  usable configuration: WOZ, DOS 3.3, pre-formatted data-only.
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
    //
    //  Every container Build can write.
    //
    //  THE ONE AUTHORITY ON WHAT CAN BE WRITTEN. Each surface presents these
    //  its own way -- the command line has a word for each, the create dialog
    //  a caption and a file extension -- but a LIST of them, restated
    //  anywhere, is a list that goes stale. `.do` was offered by the command
    //  line, refused by this validator, and missing from the dialog, all at
    //  the same time.
    //
    static const DiskFormat *  WritableContainers (size_t & outCount);

    //  Those of them that can hold `contents`, in the same order. A chooser
    //  offers exactly these, so an illegal pairing is never listed.
    static std::vector<DiskFormat>  ContainersFor (BlankDiskContents contents);

    //  Why the format / contents / bootable / volume-name combination
    //  cannot be written, or Ok. Does not assert: user input reaches it.
    static BlankDiskVerdict  CheckSpec (const BlankDiskSpec & spec);

    //  The same rules as Build's own precondition, which a caller that
    //  settled the combination first can never trip.
    static HRESULT  ValidateSpec (const BlankDiskSpec & spec);

    //  Produces the complete image bytes for the spec. All-or-nothing.
    static HRESULT  Build (const BlankDiskSpec & spec,
                           const BootPayload   & payload,
                           vector<Byte>        & outBytes);

    //
    //  A DOS-ordered sector buffer written as one of the containers.
    //
    //  Public because DirectBootBuilder produces the same buffer and needs the
    //  same three answers. A second copy of the .po reordering or the WOZ
    //  nibblization would be a second place for the sector skew to be wrong.
    //
    static HRESULT  WrapInContainer (DiskFormat            format,
                                     bool                  unformatted,
                                     const vector<Byte> &  sectors,
                                     vector<Byte>       &  outBytes);

private:
    static void  ReorderDosToPo (const vector<Byte> & dosOrdered, vector<Byte> & outPo);
};
