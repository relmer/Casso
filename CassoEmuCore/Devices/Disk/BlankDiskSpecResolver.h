#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/IDiskCommandContext.h"

enum class DiskFormat;
enum class BlankDiskContents;
enum class ProDosBootSystem;
struct BlankDiskSpec;
struct BootPayload;





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver
//
//  What a new disk from create or init is: its container, what goes inside it,
//  its volume, the system it starts and the tracks stored as flux, from the
//  command line's flags.
//
////////////////////////////////////////////////////////////////////////////////

class BlankDiskSpecResolver
{
public:
    explicit BlankDiskSpecResolver (IDiskCommandContext & context) : m_context (context) {}

    //  One row of the container table: the word a reader types and the
    //  format it names.
    struct ContainerName
    {
        const char  *  name;
        DiskFormat     format;

        //  The track size a NEW image of this container gets, and zero for
        //  the containers where the idea does not apply. It rides in the same
        //  row as the word so the two cannot be set in different places: a
        //  container offered without a size, or given one that disagrees with
        //  its name, is not expressible.
        size_t         nibbleTrackSize;
    };

    //
    //  Every container `create` advertises, as one list.
    //
    //  READABLE FROM OUTSIDE so that nothing has to restate it. The refusals
    //  print this list rather than a sentence of their own, and a test sweeps
    //  it, so a row added here reaches both without either being edited --
    //  which is how `do` came to be offered by the command line and refused
    //  by the builder.
    //
    static const ContainerName *  GetAdvertisedContainers (size_t & outCount);

    //  What was just written, in the words the flags asked for it with.
    static std::string  DescribeNewDisk (const BlankDiskSpec & spec);

    static ProDosBootSystem    GetBootSystem          (const std::string & formatName);
    static bool                IsProDosFamilyFormat   (const std::string & formatName);

    //
    //  The container a new image should be written as, from --type or, failing
    //  that, from the name it is being given.
    //
    //  An unknown word is refused by NAME, with the ones that exist, rather
    //  than defaulting: a reader who typed --type 2mg means it, and silently
    //  handing them a .dsk is worse than saying no.
    //
    HRESULT  ResolveContainer (const CommandLineOptions & options,
                               size_t                   & outNibbleTrackSize,
                               DiskFormat               & outFormat,
                               DiskCommandResult        & result);

    //  The advertised container words as a list in a sentence, each carrying
    //  `prefix` (a dot when they are being described as extensions) and joined
    //  by `conjunction` before the last.
    static std::string  FormatContainerWordList (const char * prefix, const char * conjunction);

    //  Why a settled spec cannot be written, in words, or empty if it can.
    static std::string  DescribeSpecRefusal (const BlankDiskSpec & spec);

    //  What goes INSIDE it: a DOS 3.3 catalog, a ProDOS directory, or nothing.
    HRESULT  ResolveContents (const CommandLineOptions & options,
                              BlankDiskContents        & outContents,
                              DiskCommandResult        & result);

    //  --volume, which is a number under DOS 3.3 and a name under ProDOS.
    HRESULT  ResolveVolume (const CommandLineOptions & options,
                            BlankDiskSpec            & inOutSpec,
                            DiskCommandResult        & result);

    //  Reads the operating system a --bootable disk is copied FROM, or the
    //  binary a --boot disk starts instead of one.
    HRESULT  ResolveBoot (const CommandLineOptions & options,
                          BlankDiskSpec            & inOutSpec,
                          BootPayload              & outPayload,
                          DiskCommandResult        & result);

    //  The SOS disk a ProDOS volume that starts an Apple /// copies SOS from.
    HRESULT  ResolveSosBoot   (const std::string & source, BootPayload & outPayload, DiskCommandResult & result);

    //  The disk a bootable Apple Pascal or CP/M disk is copied from.
    HRESULT  LoadSystemSource (const std::string   & path,
                               BlankDiskContents     contents,
                               BootPayload         & outPayload,
                               DiskCommandResult   & result);

    //  --flux as a mask of whole tracks. Without it, init keeps the tracks
    //  the existing WOZ already stores as flux.
    HRESULT  ResolveFlux (const CommandLineOptions & options,
                          bool                       overExisting,
                          uint64_t                 & outMask,
                          DiskCommandResult        & result);

    static HRESULT  ParseTrackList (const std::string & text, uint64_t & outMask);

private:
    IDiskCommandContext  & m_context;
};
