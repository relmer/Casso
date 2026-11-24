#include "Pch.h"

#include "BlankDiskSpecResolver.h"
#include "DiskImageSession.h"
#include "DiskImageStore.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DamagedMountReport.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/StockBootDisks.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





//
//  Every container this tool can WRITE, and the word that names it.
//
//  Separate from the reader in DiskImageStore, which recognizes what a file
//  already is. This is the shorter list of what a new one can be made as.
//
static constexpr BlankDiskSpecResolver::ContainerName  s_kContainers[] =
{
    { "dsk", DiskFormat::Dsk, 0 },
    { "do",  DiskFormat::Do,  0 },
    { "po",  DiskFormat::Po,  0 },
    { "woz", DiskFormat::Woz, 0 },
    { "nib", DiskFormat::Nib, NibbleImageCodec::kNibTrackSize },
    { "nb2", DiskFormat::Nib, NibbleImageCodec::kNb2TrackSize },
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::GetAdvertisedContainers
//
//  The container table, for anyone who has to agree with it.
//
////////////////////////////////////////////////////////////////////////////////

const BlankDiskSpecResolver::ContainerName * BlankDiskSpecResolver::GetAdvertisedContainers (size_t & outCount)
{
    outCount = _countof (s_kContainers);

    return s_kContainers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::FormatContainerWordList
//
//  The advertised words as a list in a sentence: `dsk, do, po and woz`.
//
//  READ OFF THE TABLE RATHER THAN TYPED OUT, because a sentence that names
//  the containers is a promise about what the tool accepts. Written by hand it
//  is a promise nothing keeps: `do` sat in the table, in both of these
//  sentences and in the extension reader, and the builder refused it.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlankDiskSpecResolver::FormatContainerWordList (const char * prefix, const char * conjunction)
{
    std::string  list;
    size_t       count = _countof (s_kContainers);
    size_t       i     = 0;



    for (i = 0; i < count; i++)
    {
        if (i > 0)
        {
            //  Serial comma before the last of three or more.
            if (i + 1 < count)
            {
                list += ", ";
            }
            else
            {
                list += (count > 2) ? std::string (", ") + conjunction + " "
                                    : std::string (" ") + conjunction + " ";
            }
        }

        list += prefix;
        list += s_kContainers[i].name;
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::DescribeSpecRefusal
//
//  Why a settled spec cannot be written, in words -- or empty when it can be.
//
//  THE BUILDER'S RULES ARE ASKED, NOT RESTATED. CheckSpec holds the pairing
//  matrix and answers in verdicts precisely so this layer can say which rule
//  was broken; a second copy of the matrix here would be a second thing to get
//  wrong, and the two would disagree the first time one of them changed.
//
//  Reporting only the broken rule matters more than it looks. The message
//  this replaced recited all three at once, so somebody who mistyped a
//  ProDOS volume name was handed a paragraph about sector order.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlankDiskSpecResolver::DescribeSpecRefusal (const BlankDiskSpec & spec)
{
    std::string       text;
    BlankDiskVerdict  verdict = BlankDiskBuilder::CheckSpec (spec);



    switch (verdict)
    {
        case BlankDiskVerdict::ContentsNotInContainer:
            text = "Error: illegal container and filesystem combination\n"
                   "       .dsk and .do hold DOS 3.3, .po holds ProDOS, and .woz and\n"
                   "       .nib hold either. Apple Pascal and CP/M go in any of them.\n";
            break;

        case BlankDiskVerdict::FluxNeedsWoz:
            text = "Error: flux tracks need a WOZ image\n"
                   "       Only a .woz image can store flux tracks.\n";
            break;

        case BlankDiskVerdict::BootableNeedsFilesystem:            text = "Error: cannot make an unformatted disk bootable\n"
                   "       There is no filesystem to copy an operating system into.\n"
                   "       Format the disk as dos33, prodos, pascal, or cpm.\n";
            break;

        case BlankDiskVerdict::ProDosNameUnusable:
            text = "Error: illegal volume name\n"
                   "       ProDOS volume names are 1-15 characters, starting with a\n"
                   "       letter, and can include letters, digits, and periods.\n";
            break;

        case BlankDiskVerdict::PascalNameUnusable:
            text = "Error: illegal volume name\n"
                   "       Apple Pascal volume names are 1-7 characters, with no spaces\n"
                   "       or any of $ = ? , : [ ].\n";
            break;

        default:
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveContainer
//
//  Which container a new image is written as.
//
//  --type when it is given, the file's own extension when it is not. An
//  unknown word is refused BY NAME with the ones that exist: somebody who
//  typed `--type 2mg` meant it, and handing them a .dsk instead is a disk they
//  did not ask for under a name they did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveContainer (const CommandLineOptions & options,
                                             size_t                   & outNibbleTrackSize,
                                             DiskFormat               & outFormat,
                                             DiskCommandResult        & result)
{
    HRESULT      hr         = S_OK;
    bool         found      = false;
    bool         asWord     = !options.disk.containerType.empty();
    std::string  asked      = options.disk.containerType;
    std::string  words      = FormatContainerWordList ("", "and");
    std::string  extensions = FormatContainerWordList (".", "and");
    size_t       dot        = 0;



    if (!asWord)
    {
        //  No --type, so the name decides. THE WRITE LIST ANSWERS IT, not the
        //  extension reader: the reader recognizes what a file already is, and
        //  is the wrong question for what a new one may be made as. The word
        //  and the extension are the same string for every container here, so
        //  one table serves both branches.
        dot   = options.disk.imagePath.find_last_of ('.');
        asked = (dot == std::string::npos) ? std::string()
                                           : options.disk.imagePath.substr (dot + 1);
    }

    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    for (const ContainerName & entry : s_kContainers)
    {
        if (!asked.empty() && asked == entry.name)
        {
            outFormat          = entry.format;
            outNibbleTrackSize = entry.nibbleTrackSize;
            found              = true;
            break;
        }
    }

    if (!asWord)
    {
        CBRF (found, (result.diagnostics    += "Error: missing image type: "
                                             + options.disk.imagePath + "\n"
                                             + "       Valid extensions are " + extensions + ".\n"
                                             + m_context.ApplyPrefix ("       Use %Ltype to specify the type.\n"),
                      result.exitStatus      = DiskCommandResult::kNoOutput,
                      result.badCommandLine  = true));
    }
    else
    {
        CBRFEx (found, E_INVALIDARG,
                (result.diagnostics    += "Error: unknown image type: "
                                        + options.disk.containerType + "\n"
                                          "       Valid types are " + words + ".\n",
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveContents
//
//  What goes inside the container: a DOS 3.3 catalog, a ProDOS directory, or
//  nothing at all.
//
//  DOS 3.3 IS THE DEFAULT because it is what a disk written by this tool is
//  most likely to be for: `put` and `boot` both work on one without anything
//  further. `none` is a genuinely useful answer -- a raw sector image for a
//  guest that formats it itself -- so it is offered by name rather than being
//  where an unrecognized word quietly lands.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveContents (const CommandLineOptions & options,
                                            BlankDiskContents        & outContents,
                                            DiskCommandResult        & result)
{
    HRESULT      hr         = S_OK;   // vestigial, for the bail
    bool         recognized = true;
    bool         isDos32    = false;
    std::string  asked      = options.disk.formatName;



    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    isDos32 = asked == "dos32" || asked == "dos3.2";

    CBRFEx (!isDos32, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix ("Error: a DOS 3.2 disk is written as a .d13 image\n"
                                                   "       Use %Ltype d13, or a file name ending in .d13.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    if (asked.empty() || asked == "dos33" || asked == "dos" || asked == "dos3.3")
    {
        outContents = BlankDiskContents::Dos33;
    }
    else if (asked == "prodos" || asked == "sos" || asked == "prodos+sos")
    {
        outContents = BlankDiskContents::ProDos;
    }
    else if (asked == "pascal")
    {
        outContents = BlankDiskContents::Pascal;
    }
    else if (asked == "cpm" || asked == "cp/m")
    {
        outContents = BlankDiskContents::Cpm;
    }
    else if (asked == "none" || asked == "raw" || asked == "unformatted")
    {
        outContents = BlankDiskContents::Unformatted;
    }
    else
    {
        recognized = false;
    }

    CBRFEx (recognized, E_INVALIDARG,
            (result.diagnostics    += "Error: unknown format: " + options.disk.formatName + "\n"
                                      "       Valid formats are dos33, prodos, pascal, cpm, and none.\n",
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveVolume
//
//  --volume, which is a NUMBER under DOS 3.3 and a NAME under ProDOS.
//
//  The two filesystems label a disk differently and one flag serves both, so
//  which one the word is read as follows from the format already chosen rather
//  than from how the word looks. A ProDOS volume legitimately called `254`
//  would otherwise become a DOS volume number, silently.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveVolume (const CommandLineOptions & options,
                                          BlankDiskSpec            & inOutSpec,
                                          DiskCommandResult        & result)
{
    HRESULT              hr             = S_OK;   // vestigial, for the bail
    const std::string &  asked          = options.disk.volumeName;
    bool                 named          = !asked.empty();
    int                  number         = 0;
    bool                 isVolumeNumber = false;



    //  No %Lvolume at all keeps the builder's default, which is not a
    //  failure of anything.
    BAIL_OUT_IF (!named, S_OK);

    CBRFEx (inOutSpec.contents != BlankDiskContents::Cpm, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix ("Error: CP/M has no volume name or number\n"
                                                   "       Leave out %Lvolume for a CP/M disk.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    //  UPPERCASED HERE, WHERE IT IS ACCEPTED, and not only where it is
    //  written. ProDOS holds a volume name in upper case and compares without
    //  regard to case, so `--volume mydisk` is a perfectly good way to ask for
    //  /MYDISK. The skeleton has always stored it correctly. What it did not do
    //  was tell the spec, so the line confirming the disk read back the name
    //  that was typed while `disk list` read back the name that is there, and
    //  the two disagreed over a disk that was right all along.
    if (inOutSpec.contents == BlankDiskContents::ProDos || inOutSpec.contents == BlankDiskContents::Pascal)
    {
        inOutSpec.volumeName = asked;

        for (char & letter : inOutSpec.volumeName)
        {
            letter = (char) toupper ((unsigned char) letter);
        }
    }
    else
    {
        //  A DOS 3.3 volume number, and only a number. Read by hand so a word
        //  that is not one at all is refused rather than quietly reading as
        //  zero.
        for (char letter : asked)
        {
            if (letter < '0' || letter > '9')
            {
                number = -1;
                break;
            }

            number = (number * 10) + (letter - '0');
        }

        isVolumeNumber = number >= 1 && number <= 254;
        CBRFEx (isVolumeNumber, E_INVALIDARG,
                (result.diagnostics    += "Error: illegal volume number\n"
                                          "       DOS 3.3 volume numbers are 1-254. Format the "
                                          "disk as prodos for a\n"
                                          "       volume with a name instead of a number.\n",
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        inOutSpec.volumeNumber = (Byte) number;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveBoot
//
//  Whether the disk boots, and from which operating system.
//
//  THE MASTER IS NAMED RATHER THAN FOUND. Making a disk bootable means copying
//  an operating system onto it, so there has to be one to copy from -- and
//  where the emulator keeps its downloaded copy is the executable's knowledge
//  rather than this library's. So the path arrives on the command line, where
//  a script points at whichever master it has.
//
//  --boot, which starts a binary with no operating system at all, is not here
//  yet: DirectBootBuilder produces a whole image rather than a boot sector to
//  lay over a formatted one, so it is its own path and not a flag on this one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveBoot (const CommandLineOptions & options,
                                        BlankDiskSpec            & inOutSpec,
                                        BootPayload              & outPayload,
                                        DiskCommandResult        & result)
{
    HRESULT                hr       = S_OK;
    bool                   isProDos = inOutSpec.contents == BlankDiskContents::ProDos;
    bool                   isFlat   = inOutSpec.contents == BlankDiskContents::Pascal || inOutSpec.contents == BlankDiskContents::Cpm;
    StockBootDisks::Which  which    = isProDos ? StockBootDisks::Which::ProDosUsersDisk
                                               : StockBootDisks::Which::Dos33Master;
    std::string   master        = options.disk.bootableFrom;
    bool          stock         = false;
    bool          bothWaysAsked = false;
    bool          cached        = false;
    vector<Byte>  osBytes;



    //  THE TWO WAYS TO BOOT ARE NOT VARIANTS OF ONE THING, so asking for both
    //  asks for a disk that boots twice.
    bothWaysAsked = !options.disk.directBootFile.empty() && options.disk.bootable;
    CBRFEx (!bothWaysAsked, E_INVALIDARG,
            (result.diagnostics    += m_context.ApplyPrefix (
                 "Error: %Lbootable and %Lboot are mutually exclusive\n"
                 "       %Lbootable copies an operating system onto the disk.\n"
                 "       %Lboot writes a binary that runs without one.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    //  Not asked for at all is the ordinary case, not a failure.
    BAIL_OUT_IF (!options.disk.bootable, S_OK);

    //  SOS for an Apple ///. A disk that starts only an Apple /// takes it from
    //  the image given; one that starts both takes ProDOS from the image given
    //  and SOS from the stock disk.
    if (isProDos && inOutSpec.bootSystem != ProDosBootSystem::AppleII)
    {
        hr = ResolveSosBoot (inOutSpec.bootSystem == ProDosBootSystem::AppleIII ? master : std::string(), outPayload, result);
        CHR (hr);

        inOutSpec.bootable = true;
        BAIL_OUT_IF (inOutSpec.bootSystem == ProDosBootSystem::AppleIII, S_OK);
    }

    //  A stock Apple Pascal or CP/M disk comes from the same cache, and neither
    //  goes in the repository. The choice between CP/M's two is a word in place
    //  of the image's path.
    if (isFlat)
    {
        stock = StockBootDisks::TryGetStockDisk (options.disk.formatName, master, which);

        if (stock)
        {
            cached = StockBootDisks::IsCached (which);
            CBRFEx (cached, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                    (result.diagnostics    += "Error: the " + TextEncoding::WideToNarrow (StockBootDisks::GetLabel (which))
                                            + " boot disk has not been downloaded\n"
                                            + m_context.ApplyPrefix ("       Select Bootable in Casso Explorer's New disk image to download it,\n"
                                                           "       or supply a boot disk image with %Lbootable <image>.\n"),
                     result.exitStatus      = DiskCommandResult::kNoOutput,
                     result.badCommandLine  = true));

            master = StockBootDisks::GetPath (which);
        }

        hr = LoadSystemSource (master, inOutSpec.contents, outPayload, result);
        CHR (hr);

        inOutSpec.bootable = true;
        BAIL_OUT_IF (true, S_OK);
    }

    //  A BARE --bootable MEANS THE ONE THE EMULATOR DOWNLOADED. Which of the
    //  two that is follows the format being written, because a ProDOS disk
    //  cannot be made bootable out of a DOS master.
    if (master.empty())
    {
        cached = StockBootDisks::IsCached (which);
        CBRFEx (cached, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                (result.diagnostics    += std::string ("Error: the ")
                                        + (isProDos ? "ProDOS" : "DOS 3.3")
                                        + " master has not been downloaded\n"
                                        + m_context.ApplyPrefix ("       Open Casso and use the disk picker to download "
                                                       "it, or supply a\n"
                                                       "       master image with %Lbootable <image>.\n"),
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        master = StockBootDisks::GetPath (which);
    }

    hr = m_context.GetFileIo().ReadAllBytes (master, osBytes);
    CHRF (hr, result.Fail (master, "", "cannot be read, so there is no operating system to copy"));

    //  Which slot it fills follows the format being written, because that is
    //  the one the builder will reach for.
    if (isProDos)
    {
        outPayload.proDosUsersDisk = osBytes;
    }
    else
    {
        outPayload.dosMasterSectors = osBytes;
    }

    inOutSpec.bootable = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::IsProDosFamilyFormat
//
//  The `--format` words for a ProDOS volume: one file system, which differ in
//  the computers a bootable disk starts.
//
////////////////////////////////////////////////////////////////////////////////

bool BlankDiskSpecResolver::IsProDosFamilyFormat (const std::string & formatName)
{
    return _stricmp (formatName.c_str(), "prodos") == 0 || _stricmp (formatName.c_str(), "sos") == 0 || _stricmp (formatName.c_str(), "prodos+sos") == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::GetBootSystem
//
//  The computers a bootable ProDOS volume of a `--format` word starts.
//
////////////////////////////////////////////////////////////////////////////////

ProDosBootSystem BlankDiskSpecResolver::GetBootSystem (const std::string & formatName)
{
    if (_stricmp (formatName.c_str(), "sos") == 0)
    {
        return ProDosBootSystem::AppleIII;
    }

    return _stricmp (formatName.c_str(), "prodos+sos") == 0 ? ProDosBootSystem::Both : ProDosBootSystem::AppleII;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveSosBoot
//
//  The SOS disk to copy SOS from: the image given, or with none the stock
//  System Utilities disk, which must have been downloaded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveSosBoot (const std::string & source, BootPayload & outPayload, DiskCommandResult & result)
{
    HRESULT             hr      = S_OK;
    std::string         path    = source;
    bool                cached  = false;
    vector<Byte>        bytes;
    vector<Byte>        sectors;
    SectorDecodeReport  report;



    if (path.empty())
    {
        cached = StockBootDisks::IsCached (StockBootDisks::Which::SosSystemDisk);
        CBRFEx (cached, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
                (result.diagnostics    += "Error: the SOS system disk has not been downloaded\n"
                                        + m_context.ApplyPrefix ("       Select Bootable for SOS in Casso Explorer's New disk image to download it,\n"
                                                       "       or supply an SOS disk image with %Lbootable <image>.\n"),
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        path = StockBootDisks::GetPath (StockBootDisks::Which::SosSystemDisk);
    }

    hr = m_context.GetFileIo().ReadAllBytes (path, bytes);
    CHRF (hr, result.Fail (path, "", "cannot be read, so there is no SOS to copy"));

    hr = VolumeImage::Load (bytes, path, sectors, report);
    CHRF (hr, result.Fail (path, "", "is not a disk image SOS can be copied from"));

    outPayload.sosSystemDisk = std::move (sectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::LoadSystemSource
//
//  The image a bootable Apple Pascal or CP/M disk is copied from, in any
//  container, checked for what the copy needs before anything is built.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::LoadSystemSource (const std::string   & path,
                                             BlankDiskContents     contents,
                                             BootPayload         & outPayload,
                                             DiskCommandResult   & result)
{
    HRESULT             hr       = S_OK;
    bool                isPascal = contents == BlankDiskContents::Pascal;
    VolumeKind          wanted   = isPascal ? VolumeKind::Pascal : VolumeKind::Cpm;
    bool                matches  = false;
    bool                complete = false;
    vector<Byte>        fileBytes;
    vector<Byte>        sectors;
    SectorDecodeReport  report;
    std::string         missing;



    hr = m_context.GetFileIo().ReadAllBytes (path, fileBytes);
    CHRF (hr, result.Fail (path, "", "cannot be read, so there is no operating system to copy"));

    hr = VolumeImage::Load (fileBytes, path, sectors, report);
    CHRF (hr, result.Fail (path, "", "is not a disk image this tool can read"));

    matches = VolumeImage::DetectFilesystem (sectors) == wanted;
    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT),
            result.Fail (path, "", std::string ("is not ") + (isPascal ? "an Apple Pascal disk" : "a CP/M disk") + ", so it can't supply the system"));

    missing  = isPascal ? PascalVolume::DescribeMissingSystem (sectors)
             : CpmVolume::HasSystemTracks (sectors) ? std::string() : std::string ("it has no system on tracks 0 to 2");
    complete = missing.empty();

    CBRFEx (complete, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT),
            result.Fail (path, "", "can't make a disk bootable: " + missing));

    if (isPascal)
    {
        outPayload.pascalSystem = std::move (sectors);
    }
    else
    {
        outPayload.cpmSystem = std::move (sectors);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::DescribeNewDisk
//
//  What was just written, in the words the flags used to ask for it.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlankDiskSpecResolver::DescribeNewDisk (const BlankDiskSpec & spec)
{
    std::string  text;



    switch (spec.contents)
    {
        case BlankDiskContents::Dos33:       text = "DOS 3.3";      break;
        case BlankDiskContents::ProDos:      text = "ProDOS";       break;
        case BlankDiskContents::Pascal:      text = "Apple Pascal"; break;
        case BlankDiskContents::Cpm:         text = "CP/M";         break;
        default:                             text = "unformatted";  break;
    }

    if (spec.contents == BlankDiskContents::Dos33)
    {
        text += ", volume " + std::to_string ((int) spec.volumeNumber);
    }
    else if (spec.contents == BlankDiskContents::ProDos || spec.contents == BlankDiskContents::Pascal)
    {
        text += ", volume " + spec.volumeName;
    }

    text += !spec.bootable                                       ? ", not bootable"
          : spec.contents != BlankDiskContents::ProDos           ? ", bootable"
          : spec.bootSystem == ProDosBootSystem::AppleIII     ? ", bootable on an Apple ///"
          : spec.bootSystem == ProDosBootSystem::Both         ? ", bootable on an Apple II and an Apple ///"
          :                                                       ", bootable";

    if (spec.fluxTrackMask != 0)
    {
        vector<int>  quarterTracks;
        wstring      list;
        int          track = 0;

        for (track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            if ((spec.fluxTrackMask & (1ull << track)) != 0)
            {
                quarterTracks.push_back (track * DiskImage::kQuarterTracksPerWholeTrack);
            }
        }

        list  = DamagedMountReport::FormatTrackList (quarterTracks);
        text += ", flux ";

        for (wchar_t ch : list)
        {
            text += static_cast<char> (ch);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ParseTrackList
//
//  "0-2,17" as a mask of whole tracks: numbers from 0 to 34 separated by
//  commas, a range being two of them joined by a hyphen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ParseTrackList (const std::string & text, uint64_t & outMask)
{
    HRESULT   hr      = S_OK;
    uint64_t  mask    = 0;
    size_t    pos     = 0;
    int       first   = 0;
    int       last    = 0;
    int       track   = 0;
    bool      inRange = false;
    bool      ok      = !text.empty();



    CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    while (pos <= text.size())
    {
        ok = pos < text.size() && isdigit ((unsigned char) text[pos]) != 0;
        CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        first = 0;

        while (pos < text.size() && isdigit ((unsigned char) text[pos]) != 0 && first < NibbleFormat::kTrackCount)
        {
            first = first * 10 + (text[pos] - '0');
            pos++;
        }

        last    = first;
        inRange = pos < text.size() && text[pos] == '-';

        if (inRange)
        {
            pos++;
            ok = pos < text.size() && isdigit ((unsigned char) text[pos]) != 0;
            CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            last = 0;

            while (pos < text.size() && isdigit ((unsigned char) text[pos]) != 0 && last < NibbleFormat::kTrackCount)
            {
                last = last * 10 + (text[pos] - '0');
                pos++;
            }
        }

        ok = first <= last && last < NibbleFormat::kTrackCount;
        CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        for (track = first; track <= last; track++)
        {
            mask |= 1ull << track;
        }

        BAIL_OUT_IF (pos == text.size(), S_OK);

        ok = text[pos] == ',';
        CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        pos++;
    }

Error:
    outMask = SUCCEEDED (hr) ? mask : 0;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlankDiskSpecResolver::ResolveFlux
//
//  Which whole tracks the new disk stores as flux. A bare --flux is every
//  track. Reformatting without --flux keeps the tracks the WOZ already
//  stores as flux, so init does not quietly turn a flux disk into a bit one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlankDiskSpecResolver::ResolveFlux (const CommandLineOptions & options,
                                        bool                       overExisting,
                                        uint64_t                 & outMask,
                                        DiskCommandResult        & result)
{
    HRESULT       hr       = S_OK;
    DiskFormat    format   = DiskFormat::Dsk;
    vector<Byte>  existing;
    DiskImage     image;
    int           track    = 0;
    int           slot     = 0;



    outMask = 0;

    if (options.disk.flux && options.disk.fluxTracks.empty())
    {
        outMask = BlankDiskBuilder::kAllFluxTracks;
    }
    else if (options.disk.flux)
    {
        hr = ParseTrackList (options.disk.fluxTracks, outMask);
        CHRF (hr, result.FailBadValue (
                  "Error: illegal track list\n"
                  "       Tracks are whole numbers from 0 to 34, separated by commas. A\n"
                  "       range is two of them joined by a hyphen, such as 0-2,17.\n"));
    }
    else if (overExisting)
    {
        hr = DiskSourceFormat::GetSourceFormatByExtension (options.disk.imagePath, format);
        BAIL_OUT_IF (FAILED (hr) || !BlankDiskBuilder::CanHoldFlux (format), S_OK);

        hr = m_context.GetFileIo().ReadAllBytes (options.disk.imagePath, existing);
        BAIL_OUT_IF (FAILED (hr), S_OK);

        hr = WozLoader::Load (existing, image);
        BAIL_OUT_IF (FAILED (hr), S_OK);

        for (track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            slot = image.ResolveQuarterTrack (track * DiskImage::kQuarterTracksPerWholeTrack);

            if (slot >= 0 && image.GetTrackKind (slot) == TrackKind::Flux)
            {
                outMask |= 1ull << track;
            }
        }
    }

Error:
    return hr;
}
