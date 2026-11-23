#include "Pch.h"

#include "DiskCreateCommand.h"
#include "DiskImageSession.h"
#include "DiskImageStore.h"
#include "IDiskFileIo.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Containers/ImageContainer.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosStartup.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::BuildAndWrite
//
//  Everything create and init share: settle the spec, build the bytes, put
//  them where they go.
//
//  ALL OR NOTHING, through the same commit path every other write uses. A
//  build that fails leaves the target exactly as it was, which for `init`
//  means the disk somebody was reformatting is still the disk they had.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::BuildAndWrite (const CommandLineOptions & options,
                                       DiskFormat                 format,
                                       size_t                     nibbleTrackSize,
                                       bool                       overExisting,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    BlankDiskSpec                  spec;
    BootPayload                    payload;
    vector<Byte>                   imageBytes;
    DiskImageSession::OpenedImage  target;
    std::string                    refusal;
    bool                           pairingHolds = false;
    SYSTEMTIME                     now          = {};



    spec.format          = format;
    spec.nibbleTrackSize = nibbleTrackSize;

    hr = m_resolver.ResolveContents (options, spec.contents, result);
    CHR (hr);

    spec.bootSystem = BlankDiskSpecResolver::GetBootSystem (options.disk.formatName);

    hr = m_resolver.ResolveVolume (options, spec, result);
    CHR (hr);

    now             = m_context.GetToday();
    spec.pascalDate = PascalVolume::MakeDate (now.wYear, now.wMonth, now.wDay);

    hr = m_resolver.ResolveBoot (options, spec, payload, result);
    CHR (hr);

    hr = m_resolver.ResolveFlux (options, overExisting, spec.fluxTrackMask, result);
    CHR (hr);

    //  THE PAIRING RULES ARE THE BUILDER'S, AND ITS VERDICT IS ASKED FOR
    //  RATHER THAN ITS HRESULT. ValidateSpec answers in E_INVALIDARG, which
    //  asserts, and that was right while the create dialog was the only
    //  caller: its dropdowns cannot express an illegal combination. This
    //  command line can, in one word, so every rule the builder holds is
    //  now reachable by typing and none of them is a caller's bug.
    refusal      = BlankDiskSpecResolver::DescribeSpecRefusal (spec);
    pairingHolds = refusal.empty();

    CBRF (pairingHolds,
          (result.diagnostics    += refusal,
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = BlankDiskBuilder::Build (spec, payload, imageBytes);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    //  An image being made for the first time has nothing to be stale against,
    //  so the freshness guard the commit path applies to an edit is the wrong
    //  question here. Reformatting one that already exists keeps it.
    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = !overExisting;

    if (overExisting)
    {
        hr = m_context.GetFileIo().Stat (options.disk.imagePath, target.stamp);
        target.stampRecorded = SUCCEEDED (hr);
    }

    hr = m_context.GetSession().CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += options.disk.imagePath + ": " + BlankDiskSpecResolver::DescribeNewDisk (spec) + "\n";
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::AppendTypeExtension
//
//  Gives a bare image name the extension --type asked for.
//
//  Only a name with no extension gains one. A name that already has an
//  extension keeps it, and a trailing dot is how a caller says they want no
//  extension: std::filesystem reports that dot as the extension. The path is
//  built from the narrow string the same way Win32DiskFileIo builds it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::AppendTypeExtension (std::string & imagePath, const std::string & containerType)
{
    std::filesystem::path  path      = imagePath;
    std::string            extension = containerType;
    bool                   bare      = !containerType.empty() && path.has_filename() && !path.has_extension();



    if (bare)
    {
        for (char & letter : extension)
        {
            letter = (char) tolower ((unsigned char) letter);
        }

        path.replace_extension (extension);
        imagePath = path.string();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::RunCreate
//
//  A new image file, of a container this tool decides here.
//
//  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
//  keystroke from a disk they no longer have, and `create` is the command they
//  reach for when they are not thinking about what is already there. The
//  refusal names `init`, which is the command for meaning it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::RunCreate (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT             hr              = S_OK;
    DiskFormat          format          = DiskFormat::Dsk;
    size_t              nibbleTrackSize = 0;
    bool                named           = !options.disk.imagePath.empty();
    bool                alreadyThere    = false;
    bool                isLarge         = false;
    CommandLineOptions  resolved        = options;



    CBRF (named, m_context.ReportMissingParameter ("<image>", result));

    AppendTypeExtension (resolved.disk.imagePath, resolved.disk.containerType);

    alreadyThere = m_context.GetFileIo().Exists (resolved.disk.imagePath);
    CBRF (!alreadyThere, result.Fail (resolved.disk.imagePath, "",
                                      "is already there, and create will not write over it. "
                                      "Use init to reformat it, or choose another name"));

    isLarge = IsLargeCreate (resolved);

    if (isLarge)
    {
        BuildLargeProDos (resolved, result);

        BAIL_OUT_IF (true, S_OK);
    }

    if (IsThirteenSectorImage (resolved))
    {
        BuildThirteenSector (resolved, false, result);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_resolver.ResolveContainer (resolved, nibbleTrackSize, format, result);
    CHR (hr);

    if (!resolved.disk.directBootFile.empty())
    {
        BuildDirectBoot (resolved, format, nibbleTrackSize, result);
    }
    else
    {
        BuildAndWrite (resolved, format, nibbleTrackSize, false, result);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::IsThirteenSectorImage
//
//  A `.d13` image: %Ltype d13, or a `.d13` extension with no %Ltype.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::IsThirteenSectorImage (const CommandLineOptions & options)
{
    std::string  type = options.disk.containerType;



    if (type.empty())
    {
        type = std::filesystem::path (options.disk.imagePath).extension().string();
        type = type.empty() ? type : type.substr (1);
    }

    std::transform (type.begin(), type.end(), type.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return type == "d13";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::BuildThirteenSector
//
//  A blank DOS 3.2 volume written as a `.d13` image: 35 tracks of 13 sectors,
//  in physical sector order. The volume has no DOS image on tracks 0 to 2, so
//  it isn't bootable.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::BuildThirteenSector (const CommandLineOptions & options, bool overExisting, DiskCommandResult & result)
{
    HRESULT                        hr          = S_OK;
    Dos33Geometry                  geometry    = Dos33Geometry::Make (ThirteenSectorCodec::kTrackCount, ThirteenSectorCodec::kSectorsPerTrack);
    BlankDiskSpec                  spec;
    vector<Byte>                   imageBytes;
    DiskImageSession::OpenedImage  target;
    std::string                    format      = options.disk.formatName;
    bool                           isDos32     = false;
    bool                           plain       = !options.disk.bootable && options.disk.directBootFile.empty() && !options.disk.flux;



    std::transform (format.begin(), format.end(), format.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    isDos32 = format.empty() || format == "dos" || format == "dos32" || format == "dos3.2";

    CBRFEx (isDos32, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix ("Error: a .d13 image is a DOS 3.2 disk\n"
                                                   "       Leave out %Lformat, or use %Lformat dos32.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    CBRFEx (plain, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix ("Error: a .d13 image does not accept %Lbootable, %Lboot or %Lflux\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    spec.contents = BlankDiskContents::Dos33;

    hr = m_resolver.ResolveVolume (options, spec, result);
    CHR (hr);

    imageBytes.assign (geometry.GetImageByteSize(), 0);

    hr = Dos33Skeleton::Write (imageBytes, spec.volumeNumber, geometry);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = !overExisting;

    if (overExisting)
    {
        hr = m_context.GetFileIo().Stat (options.disk.imagePath, target.stamp);
        target.stampRecorded = SUCCEEDED (hr);
    }

    hr = m_context.GetSession().CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += options.disk.imagePath + ": DOS 3.2, volume " + std::to_string ((int) spec.volumeNumber) + ", not bootable\n";
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::IsLargeCreate
//
//  A create of a ProDOS volume larger than a floppy, a size past 140K or a
//  container only a block volume has, or of an HFS volume.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::IsLargeCreate (const CommandLineOptions & options)
{
    std::string  size    = options.disk.sizeName;
    bool         isSized = !size.empty() && size != "140k" && size != "140K";



    return isSized || IsBlockOnlyExtension (options.disk.imagePath) || IsHfsFormat (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::IsHfsFormat
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::IsHfsFormat (const CommandLineOptions & options)
{
    return _stricmp (options.disk.formatName.c_str(), "hfs") == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::IsBlockOnlyExtension
//
//  An extension only a block volume is written with: hard disk images, 2IMG
//  and Disk Copy 4.2.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::IsBlockOnlyExtension (const std::string & imagePath)
{
    ImageContainerKind  kind = ImageContainer::GetKind (imagePath, 0);



    return kind == ImageContainerKind::Blocks || kind == ImageContainerKind::TwoImg || kind == ImageContainerKind::DiskCopy42;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::IsProDosOrderExtension
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::IsProDosOrderExtension (const std::string & imagePath)
{
    std::string  extension = std::filesystem::path (imagePath).extension().string();



    std::transform (extension.begin(), extension.end(), extension.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return extension == ".po";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::TryGetLargeBlockCount
//
//  The block count of a size word. 32M is the 65,535 blocks a ProDOS volume
//  addresses.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCreateCommand::TryGetLargeBlockCount (const std::string & sizeName, uint32_t & outBlocks)
{
    static constexpr struct { const char * pszName; uint32_t blocks; }  kSizes[] =
    {
        { "140k", 280   },
        { "800k", 1600  },
        { "5m",   10240 },
        { "10m",  20480 },
        { "20m",  40960 },
        { "32m",  65535 },
    };



    std::string  lowered = sizeName.empty() ? std::string ("800k") : sizeName;



    std::transform (lowered.begin(), lowered.end(), lowered.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    for (const auto & size : kSizes)
    {
        if (lowered == size.pszName)
        {
            outBlocks = size.blocks;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::BuildLargeProDos
//
//  A formatted ProDOS or HFS volume of the size asked for, in the container
//  the image's extension selects: raw blocks for `.po` and `.hdv`, 2IMG in
//  ProDOS order, or Disk Copy 4.2, which is for 800K disks only. An HFS
//  volume is 800K or larger.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::BuildLargeProDos (const CommandLineOptions & options, DiskCommandResult & result)
{
    constexpr uint32_t  k800KBlocks = 1600;



    HRESULT                        hr         = S_OK;
    ImageContainerKind             kind       = ImageContainer::GetKind (options.disk.imagePath, 0);
    uint32_t                       blocks     = 0;
    bool                           sizeKnown  = TryGetLargeBlockCount (options.disk.sizeName, blocks);
    bool                           isHfs      = IsHfsFormat (options);
    bool                           isProDos   = isHfs || options.disk.formatName.empty() || BlankDiskSpecResolver::IsProDosFamilyFormat (options.disk.formatName);
    std::string                    name       = options.disk.volumeName.empty() ? std::string (isHfs ? "Untitled" : "NEWDISK") : options.disk.volumeName;
    bool                           nameOk     = isHfs ? HfsWriter::IsValidVolumeName (name) : ProDosVolume::IsValidName (name);
    bool                           hfsFits    = false;
    bool                           container  = false;
    bool                           fitsDc42   = false;
    vector<Byte>                   volume;
    vector<Byte>                   imageBytes;
    std::string                    bootWords;
    TwoImgHeader                   twoImg;
    DiskCopy42Header               diskCopy;
    DiskImageSession::OpenedImage  target;



    if (kind == ImageContainerKind::None)
    {
        kind = ImageContainerKind::Blocks;
    }

    container = IsBlockOnlyExtension (options.disk.imagePath) || IsProDosOrderExtension (options.disk.imagePath);
    fitsDc42  = kind != ImageContainerKind::DiskCopy42 || blocks == k800KBlocks;
    hfsFits   = !isHfs || blocks >= k800KBlocks;

    CBRF (sizeKnown, (result.diagnostics   += m_context.ApplyPrefix ("Error: illegal %Lsize value\n"
                                                           "       The sizes are 140K, 800K, 5M, 10M, 20M and 32M.\n"),
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (isProDos,  (result.diagnostics   += m_context.ApplyPrefix ("Error: illegal %Lformat for a large disk\n"
                                                           "       Disks larger than 140K are ProDOS or HFS volumes.\n"),
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (hfsFits,   (result.diagnostics   += "Error: illegal size for an HFS volume\n"
                                              "       HFS volumes are 800K or larger.\n",
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (container, (result.diagnostics   += "Error: illegal container for a large disk\n"
                                              "       Disks larger than 140K are written as .po, .hdv, .2mg or .dc42 images.\n",
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (fitsDc42,  (result.diagnostics   += "Error: illegal size for a Disk Copy 4.2 image\n"
                                              "       A Disk Copy 4.2 image is an 800K disk.\n",
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (nameOk,    (result.diagnostics   += GetVolumeNameRule (isHfs),
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));

    volume.assign ((size_t) blocks * (size_t) ProDosSkeleton::kBlockByteSize, 0);

    hr = isHfs ? HfsWriter::Format (volume, blocks, name) : ProDosSkeleton::Write (volume, name);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    hr = InstallLargeBoot (options, isHfs, volume, bootWords, result);
    CHR (hr);

    if (!isHfs)
    {
        ProDosSkeleton::InstallNoSystemLoader (volume);
    }

    //  A 140K volume is built in DOS logical order, and every container here
    //  has its blocks in order.
    if (!ProDosSkeleton::IsBlockOrder (volume.size()))
    {
        VolumeImage::DosLogicalToProDosFile (vector<Byte> (volume), volume);
    }

    if (kind == ImageContainerKind::TwoImg)
    {
        TwoImgContainer::Build (twoImg, volume, imageBytes);
    }
    else if (kind == ImageContainerKind::DiskCopy42)
    {
        diskCopy.name = name;
        diskCopy.tags.assign ((size_t) blocks * DiskCopy42Container::kTagBytesPerBlock, 0);
        DiskCopy42Container::Build (diskCopy, volume, imageBytes);
    }
    else
    {
        imageBytes = std::move (volume);
    }

    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = true;

    hr = m_context.GetSession().CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += std::format ("{}: {} volume {}, {} blocks{}\n", options.disk.imagePath, isHfs ? "HFS" : "ProDOS", name, blocks, bootWords);
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::GetVolumeNameRule
//
////////////////////////////////////////////////////////////////////////////////

const char * DiskCreateCommand::GetVolumeNameRule (bool isHfs)
{
    return isHfs ? "Error: illegal volume name\n"
                   "       HFS volume names are 1-27 characters, and can't include a colon.\n"
                 : "Error: illegal volume name\n"
                   "       ProDOS volume names are 1-15 characters, starting with a letter, and can include letters, digits, and periods.\n";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::ReformatLargeProDos
//
//  A new ProDOS volume over an existing block image, at the size the image
//  has. The container's header is kept, as any write through the volume
//  layer keeps it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::ReformatLargeProDos (const CommandLineOptions & options, const FileStamp & stamp, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           isHfs    = IsHfsFormat (options);
    bool                           isProDos = isHfs || options.disk.formatName.empty() || BlankDiskSpecResolver::IsProDosFamilyFormat (options.disk.formatName);
    std::string                    name     = options.disk.volumeName.empty() ? std::string (isHfs ? "Untitled" : "NEWDISK") : options.disk.volumeName;
    bool                           nameOk   = isHfs ? HfsWriter::IsValidVolumeName (name) : ProDosVolume::IsValidName (name);
    vector<Byte>                   original;
    vector<Byte>                   sectors;
    vector<Byte>                   imageBytes;
    std::string                    bootWords;
    SectorDecodeReport             report;
    std::string                    refusal;
    DiskImageSession::OpenedImage  target;



    CBRF (isProDos, (result.diagnostics   += m_context.ApplyPrefix ("Error: illegal %Lformat for a large disk\n"
                                                          "       Disks larger than 140K are ProDOS or HFS volumes.\n"),
                     result.exitStatus     = DiskCommandResult::kNoOutput,
                     result.badCommandLine = true));
    CBRF (nameOk,   (result.diagnostics   += GetVolumeNameRule (isHfs),
                     result.exitStatus     = DiskCommandResult::kNoOutput,
                     result.badCommandLine = true));

    hr = m_context.GetFileIo().ReadAllBytes (options.disk.imagePath, original);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be read"));

    hr = VolumeImage::Load (original, options.disk.imagePath, sectors, report);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "is not an image this tool can read"));

    hr = isHfs ? HfsWriter::Format (sectors, sectors.size() / ProDosSkeleton::kBlockByteSize, name) : ProDosSkeleton::Write (sectors, name);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    hr = InstallLargeBoot (options, isHfs, sectors, bootWords, result);
    CHR (hr);

    if (!isHfs)
    {
        ProDosSkeleton::InstallNoSystemLoader (sectors);
    }

    hr = VolumeImage::Save (original, options.disk.imagePath, sectors, imageBytes, refusal);
    CHRF (hr, result.Fail (options.disk.imagePath, "", refusal.empty() ? std::string ("could not be written") : refusal));

    target.imagePath     = options.disk.imagePath;
    target.stamp         = stamp;
    target.stampRecorded = true;
    target.isNew         = false;

    hr = m_context.GetSession().CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += std::format ("{}: {} volume {}, {} blocks{}\n", options.disk.imagePath, isHfs ? "HFS" : "ProDOS", name, ProDosSkeleton::GetBlockCount (sectors.size()), bootWords);
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::InstallLargeBoot
//
//  The volume is in block order, which ProDosSkeleton addresses by the
//  buffer's size.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCreateCommand::InstallLargeBoot (const CommandLineOptions & options, bool isHfs, vector<Byte> & ioVolume, std::string & outBootWords, DiskCommandResult & result)
{
    HRESULT        hr = S_OK;
    BlankDiskSpec  spec;
    BootPayload    payload;
    std::string    sosError;



    outBootWords.clear();

    BAIL_OUT_IF (!options.disk.bootable, S_OK);

    CBRFEx (!isHfs, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix ("Error: an HFS volume can't be made bootable\n"
                                                   "       %Lbootable copies ProDOS onto a ProDOS volume.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    spec.contents   = BlankDiskContents::ProDos;
    spec.bootSystem = BlankDiskSpecResolver::GetBootSystem (options.disk.formatName);

    hr = m_resolver.ResolveBoot (options, spec, payload, result);
    CHR (hr);

    if (spec.bootSystem != ProDosBootSystem::AppleIII)
    {
        hr = ProDosSkeleton::InstallBoot (ioVolume, payload.proDosUsersDisk);
        CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be made bootable: the master has no PRODOS and BASIC.SYSTEM to copy"));
    }

    if (spec.bootSystem != ProDosBootSystem::AppleII)
    {
        hr = ProDosStartup::InstallSos (ioVolume, payload.sosSystemDisk, sosError);
        CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be made bootable on an Apple ///: " + sosError));
    }

    outBootWords = spec.bootSystem == ProDosBootSystem::AppleIII ? ", bootable on an Apple ///"
                 : spec.bootSystem == ProDosBootSystem::Both     ? ", bootable on an Apple II and an Apple ///"
                 :                                                 ", bootable";

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::BuildDirectBoot
//
//  A disk that starts a binary with no operating system on it at all.
//
//  A SEPARATE PATH RATHER THAN A FLAG ON THE OTHER ONE, because there is no
//  filesystem here to put the binary into. DirectBootBuilder writes a loader
//  into the boot sector and lays the payload down in the sectors after it; the
//  disk has no VTOC, no catalog and no directory, and `list` will say so.
//
//  Both builders hand back the same 143,360-byte DOS-ordered buffer, so the
//  container is settled by the same function either way.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::BuildDirectBoot (const CommandLineOptions & options,
                                         DiskFormat                 format,
                                         size_t                     nibbleTrackSize,
                                         DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    DirectBootSpec                 spec;
    BlankDiskSpec                  fluxSpec;
    bool                           fluxFits     = false;
    vector<Byte>                   payload;
    vector<Byte>                   sectors;
    vector<Byte>                   imageBytes;
    DiskImageSession::OpenedImage  target;
    std::string                    refusal;
    bool                           formatAgrees = false;
    char                           summary[160] = {};



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    //
    //  Checked here as well as in ResolveBoot, because this path never reaches
    //  ResolveBoot: it builds no filesystem, so it skips the code that would
    //  have caught the pair. Measured before the check went in, the two
    //  together honored --boot and dropped --bootable without a word.
    CBRF (!options.disk.bootable,
          (result.diagnostics    += m_context.ApplyPrefix (
               "Error: %Lbootable and %Lboot are mutually exclusive\n"
               "       %Lbootable copies an operating system onto the disk.\n"
               "       %Lboot writes a binary that runs without one.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    //  A filesystem was asked for AND a disk with none. Refused rather than
    //  quietly dropping one of them.
    formatAgrees = options.disk.formatName.empty() || options.disk.formatName == "none";
    CBRF (formatAgrees,
          (result.diagnostics    += m_context.ApplyPrefix ("Error: %Lboot and %Lformat are mutually exclusive\n"
                                                 "       %Lboot writes no filesystem. A direct-boot "
                                                 "disk contains the\n"
                                                 "       binary and nothing else.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = m_context.GetFileIo().ReadAllBytes (options.disk.directBootFile, payload);
    CHRF (hr, result.Fail (options.disk.directBootFile, "", "cannot be read, so there is nothing to boot"));

    //  The load address is the one the binary was assembled for, and --load is
    //  how the caller says so. The entry follows it unless named separately.
    if (options.disk.hasLoadAddress)
    {
        spec.loadAddress = options.disk.loadAddress;
    }

    spec.entryAddress = options.disk.hasEntryAddress ? options.disk.entryAddress
                                                     : spec.loadAddress;

    //  The builder names exactly one reason, and it is the reason: an address
    //  outside the window has a capacity of zero, so blaming the payload's
    //  length for an address problem would be answering the wrong question.
    hr = DirectBootBuilder::Build (payload, spec, sectors, refusal);
    CHRF (hr, result.Fail (options.disk.imagePath, options.disk.directBootFile, refusal));

    hr = m_resolver.ResolveFlux (options, false, fluxSpec.fluxTrackMask, result);
    CHR (hr);

    //  A direct-boot disk has no filesystem, so of the builder's rules only
    //  the container's hold on flux applies.
    fluxSpec.format   = format;
    fluxSpec.contents = BlankDiskContents::Unformatted;
    refusal           = BlankDiskSpecResolver::DescribeSpecRefusal (fluxSpec);
    fluxFits          = refusal.empty();

    CBRF (fluxFits,
          (result.diagnostics    += refusal,
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = BlankDiskBuilder::WrapInContainer (format, nibbleTrackSize, false, fluxSpec.fluxTrackMask,
                                            sectors, imageBytes);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    target.imagePath     = options.disk.imagePath;
    target.stampRecorded = false;
    target.isNew         = true;

    hr = m_context.GetSession().CommitImage (target, imageBytes, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: direct boot, %zu bytes loading at $%04X, entered at $%04X\n",
              options.disk.imagePath.c_str(), payload.size(),
              (unsigned) spec.loadAddress, (unsigned) spec.entryAddress);

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand::RunInit
//
//  A disk that is already there, formatted again.
//
//  THE CONTAINER IS NOT A CHOICE HERE. It was decided when the file was made,
//  and this command rewrites what is INSIDE it -- so `init` takes no --type, and
//  a reader who wants a different container wants a different file, which is
//  what `create` makes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCreateCommand::RunInit (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT     hr              = S_OK;
    DiskFormat  format          = DiskFormat::Dsk;
    size_t      nibbleTrackSize = 0;
    FileStamp   stamp;
    bool        named           = !options.disk.imagePath.empty();
    bool        exists          = false;
    bool        untyped         = options.disk.containerType.empty();



    CBRF (named, m_context.ReportMissingParameter ("<image>", result));

    exists = m_context.GetFileIo().Exists (options.disk.imagePath);
    CBRF (exists, result.Fail (options.disk.imagePath, "", "is not there. Use create to make one"));

    CBRF (untyped,
          (result.diagnostics    += m_context.ApplyPrefix (
               "Error: init does not accept %Ltype\n"
               "       The image already has a container type. Use create to make an\n"
               "       image with a different type.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = m_context.GetFileIo().Stat (options.disk.imagePath, stamp);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be measured"));

    if (ImageContainer::GetKind (options.disk.imagePath, (size_t) stamp.sizeBytes) != ImageContainerKind::None)
    {
        ReformatLargeProDos (options, stamp, result);

        BAIL_OUT_IF (true, S_OK);
    }

    if (IsThirteenSectorImage (options))
    {
        BuildThirteenSector (options, true, result);

        BAIL_OUT_IF (true, S_OK);
    }

    //  From the file's own name, because the file is what is being reformatted.
    hr = DiskImageStore::GetSourceFormatByExtension (options.disk.imagePath, format);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "is not a kind of image this tool writes"));

    if (format == DiskFormat::Nib)
    {
        //  THE LENGTH DECIDES HERE, NOT THE NAME, which is the opposite of
        //  create and for a good reason: this file exists. Either nibble track
        //  size circulates under either extension, so reformatting by the name
        //  would resize a .nib that happens to hold the smaller tracks --
        //  turning a reformat into a rewrite of the whole container.
        hr = NibbleImageCodec::ResolveGeometry ((size_t) stamp.sizeBytes, nibbleTrackSize);
        CHRF (hr, result.Fail (options.disk.imagePath, "",
                               "is not a nibble image of a length this tool recognizes"));
    }

    BuildAndWrite (options, format, nibbleTrackSize, true, result);

Error:
    return;
}
