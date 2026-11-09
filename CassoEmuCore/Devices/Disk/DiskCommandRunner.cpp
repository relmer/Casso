#include "Pch.h"

#include "DiskCommandRunner.h"
#include "Seams/IIntentChannel.h"
#include "AppleTextCodec.h"
#include "CommandLineParser.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "DiskImageStore.h"
#include "Core/AppleSingleCodec.h"
#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/Disk/Containers/ImageContainer.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Devices/Disk/DamagedMountReport.h"
#include "Machines/Apple2/Common/StockBootDisks.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/FilesystemChoices.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "FileTypes/FileFormatTable.h"
#include "IntegerBasicDetokenizer.h"
#include "Utils.h"




//
//  Every container this tool can WRITE, and the word that names it.
//
//  Separate from the reader in DiskImageStore, which recognizes what a file
//  already is. This is the shorter list of what a new one can be made as.
//
static constexpr DiskCommandRunner::ContainerName  s_kContainers[] =
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
//  DiskCommandRunner::DiskCommandRunner
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandRunner::DiskCommandRunner (IDiskFileIo & fileIo)
    : m_fileIo  (fileIo)
    , m_session (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetLocalDate
//
////////////////////////////////////////////////////////////////////////////////

SYSTEMTIME DiskCommandRunner::GetLocalDate()
{
    SYSTEMTIME  now = {};



    GetLocalTime (&now);

    return now;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::SetBanner
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::SetBanner (const std::string & banner)
{
    m_banner = banner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyPrefixes
//
//  Puts the reader's own prefixes into a line of help.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::ApplyPrefix (const std::string & text) const
{
    return DiskHelpPage::ApplyPrefixes (text, m_flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FindMissingParameters
//
//  Every required operand this command did not get, in grammar order.
//
//  THE LIST MATCHES THE GRAMMAR LINE the block prints, which is what makes the
//  refusal readable: a reader told that <image> and <name> are missing can see
//  both of them in the usage directly above, in that order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DiskCommandRunner::FindMissingParameters (const CommandLineOptions & options) const
{
    using Command = CommandLineOptions::DiskOptions::Command;

    std::vector<std::string>  missing;
    Command                   command   = options.disk.command;
    bool                      wantsFile = command == Command::Put || command == Command::SectorWrite
                                       || command == Command::BlockWrite;
    bool                      wantsName = command == Command::Get || command == Command::Delete
                                       || command == Command::Boot;
    bool                      wantsPath = command == Command::Mkdir || command == Command::Rmdir;



    //  Help asks for nothing, and a command the table does not know is
    //  answered as an unknown command rather than as a missing operand.
    HRESULT  hr    = S_OK;   // vestigial, for the bail
    bool     asked = command != Command::None && command != Command::Help;



    BAIL_OUT_IF (!asked, S_OK);

    if (options.disk.imagePath.empty())
    {
        missing.push_back ("<image>");
    }

    if (wantsName && options.disk.path.empty())
    {
        missing.push_back ("<name>");
    }

    if (wantsPath && options.disk.path.empty())
    {
        missing.push_back ("<path>");
    }

    if (wantsFile && options.disk.hostFile.empty())
    {
        missing.push_back ("<file>");
    }

Error:
    return missing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReportMissingParameters
//
//  That command's usage, and then every operand it did not get.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReportMissingParameters (const std::vector<std::string> & parameters,
                                                 DiskCommandResult & result) const
{
    std::string  list;
    size_t       i = 0;



    for (i = 0; i < parameters.size(); i++)
    {
        bool  last = (i + 1) == parameters.size();

        list += parameters[i];

        if (!last)
        {
            list += (parameters.size() == 2) ? " and " : (i + 2 == parameters.size() ? ", and " : ", ");
        }
    }

    result.output        += DiskHelpPage::BuildCommandHelp (m_command, m_flagPrefix);
    result.diagnostics   += std::string ("Error: required parameter")
                          + ((parameters.size() > 1) ? "s " : " ") + list + " missing\n";
    result.exitStatus     = DiskCommandResult::kNoOutput;
    result.badCommandLine = true;
    result.usageShown     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReportMissingParameter
//
//  A required operand that was not supplied.
//
//  THAT COMMAND'S USAGE, THEN THE PARAMETER, in the shape a Windows
//  command-line tool answers with. What went before was a bare sentence with
//  no usage at all for some commands and the WHOLE page for others, and the
//  bare sentence was written in a voice the tool uses nowhere else.
//
//  The usage comes first and the error last, for the reason every other
//  refusal here puts it last: a reader sees the bottom of the screen.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReportMissingParameter (const std::string & parameter,
                                                DiskCommandResult & result) const
{
    ReportMissingParameters (std::vector<std::string> { parameter }, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetDos33TypeLetter
//
//  The letters DOS itself prints. The lock bit is already masked off by the
//  reader, so only the type remains.
//
////////////////////////////////////////////////////////////////////////////////

char DiskCommandRunner::GetDos33TypeLetter (Byte type)
{
    switch (type)
    {
        case 0x00: return 'T';
        case 0x01: return 'I';
        case 0x02: return 'A';
        case 0x04: return 'B';
        case 0x08: return 'S';
        case 0x10: return 'R';
        case 0x20: return 'A';
        case 0x40: return 'B';
        default:   return '?';
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatDos33Entry
//
//  The shape a booted machine prints: lock flag, type letter, sector count in
//  three digits, then the name.
//
//  ENTRIES OCCUPYING NO SECTORS ARE RENDERED, NOT FILTERED, and that is a
//  decision rather than an oversight. Real disks use zero-sector catalog
//  entries to draw section headings -- Merlin Pro's own disk has twenty of
//  them among sixty-three, reading "MERLIN PRO - DOS 3.3" and a rule of
//  underscores beneath it. DOS renders them, the vendor's own printed catalog
//  shows them, and their author put them there to be seen.
//
//  Hiding them would make this listing disagree with the machine's, and the
//  disagreement would look like an enumeration bug rather than a display
//  choice. Anyone who wants them gone is asking for a filter, which is a
//  different request from enumerating differently.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatDos33Entry (const FileEntry & entry)
{
    char  sectors[8] = {};



    snprintf (sectors, sizeof (sectors), "%03u", (unsigned) (entry.sizeUnits & 0x3FF));

    return std::string (entry.isLocked ? "*" : " ")
         + std::string (1, GetDos33TypeLetter (entry.type))
         + " " + sectors + " " + entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatProDosEntry
//
//  EVERY COLUMN, ALWAYS. `eof=` and `aux=` were behind a `--long` flag, and the
//  flag was paying for nothing: ProDosVolume::Enumerate fills both fields
//  unconditionally, so withholding them cost a read of the help to discover and
//  a second run of the command to get. The widest row this produces still fits
//  inside 80 columns.
//
//  They are the two fields a build loop actually needs, which is what made the
//  flag worst: the exact length of a file and the address a binary loads at are
//  what you check after placing one, and both were the ones hidden.
//
//  A DOS 3.3 listing is unaffected -- it has its own formatter, and the
//  filesystem records neither field.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatProDosEntry (const FileEntry & entry)
{
    char         detail[96] = {};
    std::string  text;



    text = std::string (entry.isLocked ? "*" : " ") + entry.name;

    while (text.size() < 18)
    {
        text += " ";
    }

    snprintf (detail, sizeof (detail), "%s $%02X %5u  eof=%u aux=$%04X",
              entry.isDirectory ? "DIR" : "   ",
              (unsigned) entry.type,
              (unsigned) entry.sizeUnits,
              (unsigned) entry.eofBytes,
              (unsigned) entry.auxType);

    text += detail;

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeListingRefusal
//
//  WHICH LINE, AND THE LINE ITSELF. A number on its own is useless against a
//  file numbered by tens, and useless in the other direction too: a program
//  read off a disk has line numbers the file on the host does not have. So the number
//  is given when there is one, the file's own position when there is not, and
//  the offending text is quoted underneath whenever it is known.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeListingRefusal (
    const char                   *  leadIn,
    const ApplesoftListingError  &  error)
{
    char         note[96] = {};
    std::string  message  = leadIn;



    message += ": ";

    if (error.hasLineNumber)
    {
        snprintf (note, sizeof (note), "line %u ", (unsigned) error.lineNumber);
        message += note;
    }
    else if (error.sourceLineIndex > 0)
    {
        snprintf (note, sizeof (note), "the line at file position %u ",
                  (unsigned) error.sourceLineIndex);
        message += note;
    }

    message += error.reason;
    message += "\n";

    if (!error.sourceLine.empty())
    {
        message += "    ";
        message += error.sourceLine;
        message += "\n";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeDamage
//
//  Both volumes return ERROR_HANDLE_EOF for a file whose sectors cannot be
//  gathered: DOS 3.3 from its track/sector list, ProDOS from its index
//  block.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeDamage (HRESULT hr, VolumeKind kind)
{
    std::string  damage;



    if (hr == HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF))
    {
        damage = (kind == VolumeKind::ProDos) ? "the index block is damaged"
               : (kind == VolumeKind::Dos33)  ? "the track/sector list is damaged"
                                              : "the directory entry is damaged";
    }

    return damage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeVolumeRefusal
//
//  A verdict from the filesystem layer, in words.
//
//  That layer answers in Win32 codes on purpose -- they carry the meanings a
//  refusal needs without asserting the way E_INVALIDARG does, since everything
//  it refuses came from a user rather than from a caller's bug. What they are
//  not is an explanation. Printing the code would tell someone whose file is
//  locked that the tool returned a number, and the one thing they could have
//  done about it would be missing from the message.
//
//  Anything unrecognized still gets a sentence rather than a code, because a
//  case added below this layer must not be able to leak one upward.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeVolumeRefusal (HRESULT hr, VolumeKind kind)
{
    //  A lookup rather than a ladder: eleven ifs were a table wearing
    //  control flow, and every row added was another early return.
    static constexpr struct { HRESULT code; const char * sentence; }  kSentences[] =
    {
        { HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
          "it is locked. Unlock it on the disk first" },
        { HRESULT_FROM_WIN32 (ERROR_DISK_FULL),
          "the volume has no room for the file's contents or for another "
          "catalog entry" },
        { HRESULT_FROM_WIN32 (ERROR_DISK_TOO_FRAGMENTED),
          "the volume has enough free blocks, but in pieces, and an Apple Pascal "
          "file has to be contiguous. Run 'disk krunch' to combine the free space" },
        { HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE),
          "the volume directory is full. ProDOS has room for 51 entries "
          "there, so put it in a subdirectory" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
          "the name is not valid on this file system. It must be a single "
          "component starting with a letter, short enough for the catalog, "
          "and free of commas and control characters" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
          "it is not on this volume" },
        { HRESULT_FROM_WIN32 (ERROR_DUP_NAME),
          "the name is not unique. Run 'disk list' for the indexes, then "
          "specify both <name> and --index to select that file" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE),
          "it is larger than this file system can record for one file" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
          "it is a binary file and requires a load address. Use %Lload $XXXX" },
        { HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED),
          "it is a directory, and delete accepts files only. Use rmdir, which "
          "removes a directory and, with --recurse, everything below it" },
        { HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
          "the image has no operating system on the boot tracks" },
        { HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE),
          "ProDOS starts only a file of type SYS, and not the kernel itself" },
    };
    std::string  sentence = DescribeDamage (hr, kind);
    bool         noBoot   = hr == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED) && kind != VolumeKind::Dos33 && kind != VolumeKind::ProDos;



    if (noBoot)
    {
        sentence = (kind == VolumeKind::Pascal) ? "Apple Pascal has no setting for this. It runs a code file with the name SYSTEM.STARTUP"
                 : (kind == VolumeKind::Cpm)    ? "CP/M has no setting for this. Its command processor runs what is typed at it"
                                                : "this file system has no setting for a startup program";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME) && kind == VolumeKind::Pascal)
    {
        sentence = "the name is not valid on this file system. Apple Pascal names are 1 to 15 characters, "
                   "with no spaces or any of $ = ? , : [ ]";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME) && kind == VolumeKind::Cpm)
    {
        sentence = "the name is not valid on this file system. CP/M names are 1 to 8 characters, an optional "
                   "period and up to 3 more, with no spaces or any of < > , ; : = ? * [ ], in a user of 0 to 15";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND) && (kind == VolumeKind::Pascal || kind == VolumeKind::Cpm))
    {
        sentence = std::string (VolumeImage::GetSystemName (kind)) + " has no directories";
    }

    for (const auto & row : kSentences)
    {
        if (sentence.empty() && row.code == hr)
        {
            sentence = row.sentence;
        }
    }

    if (sentence.empty())
    {
        sentence = "the operation is not possible on this volume";
    }

    return sentence;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunList
//
//  Damage does not fail the listing. The developer this serves is recovering
//  old disks, and the entries that DID read are the ones they need; a refusal
//  would withhold exactly the information the situation calls for. It earns the
//  complaints status instead, so a script can tell a partial read from a clean
//  one without parsing anything.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunList (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr           = S_OK;
    DiskImageSession::OpenedImage  opened;
    VolumeListing                  listing;
    std::string                    directory;
    char                           summary[128] = {};
    bool                           isFlat       = false;



    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    isFlat = opened.kind == VolumeKind::Pascal || opened.kind == VolumeKind::Cpm;

    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);



        hr = volume->Enumerate (listing);

        CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

        result.output += std::string (VolumeImage::GetSystemName (opened.kind)) + " disk\n\n";

        //  The whole volume's listing, as the Filer or STAT prints it, since
        //  neither file system has anything below the volume directory.
        if (isFlat)
        {
            hr = AppendNativeListing (options, opened, *volume, listing, result);
            CHR (hr);

            ReportListingDamage (options, opened, listing, result);

            BAIL_OUT_IF (true, S_OK);
        }

        if (listing.hasVolumeName)
        {
            result.output += "/" + listing.volumeName + "\n\n";
        }
        else if (listing.hasVolumeNumber)
        {
            snprintf (summary, sizeof (summary), "DISK VOLUME %u\n\n",
                      (unsigned) listing.volumeNumber);
            result.output += summary;
        }

        //  The rows come from the directory the caller named, and the volume's
        //  name and free space above and below them describe the whole volume
        //  either way.
        hr = ResolveVolumePath (options, opened, options.disk.path, directory, result);
        CHR (hr);

        hr = AppendDirectoryRows (*volume, opened.kind, directory, options.disk.recurse, result);

        CHRF (hr, result.FailAction (options.disk.imagePath, "list " + options.disk.path,
                              ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

        snprintf (summary, sizeof (summary), "\n%u %s free of %u\n",
                  (unsigned) listing.freeUnits,
                  (opened.kind == VolumeKind::Dos33 || opened.kind == VolumeKind::Gutenberg) ? "sectors" : "blocks",
                  (unsigned) listing.totalUnits);

        result.output += summary;

        // A listing is where someone finds out what a boot would do, so a
        // disk with no operating system says so here, in the words `create`
        // uses for the same state. A DOS 3.3 disk boots from DOS on tracks 0
        // to 2; a ProDOS disk from the PRODOS file its block 0 loader loads.
        if (opened.kind == VolumeKind::Dos33)
        {
            if (!Dos33Volume (opened.sectors).HasDosImage())
            {
                result.output += "\nNot bootable: no DOS on tracks 0 to 2\n";
            }
        }
        else if (opened.kind == VolumeKind::ProDos && !HasProDosSystemFile (listing))
        {
            result.output += "\nNot bootable: no PRODOS file on the volume\n";
        }
    }

    ReportListingDamage (options, opened, listing, result);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::AppendNativeListing
//
//  A directory typed for a file system with none is turned down, as it is on
//  DOS 3.3.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::AppendNativeListing (
    const CommandLineOptions             & options,
    const DiskImageSession::OpenedImage  & opened,
    const IVolume                        & volume,
    const VolumeListing                  & listing,
    DiskCommandResult                    & result)
{
    HRESULT                   hr = S_OK;
    VolumeListing             below;
    std::vector<std::string>  lines;
    std::string               missing;



    hr = volume.EnumerateDirectory (FilePath::Parse (options.disk.path), below);

    CHRF (hr, result.FailAction (options.disk.imagePath, "list " + options.disk.path,
                                 ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    if (opened.kind == VolumeKind::Pascal)
    {
        NativeListing::FormatPascal (listing, lines);
    }
    else
    {
        NativeListing::FormatCpm (listing, lines);
    }

    for (const std::string & line : lines)
    {
        result.output += line + "\n";
    }

    //  As DOS 3.3 and ProDOS listings report it.
    missing = (opened.kind == VolumeKind::Pascal) ? PascalVolume::DescribeMissingSystem (opened.sectors)
            : CpmVolume::HasSystemTracks (opened.sectors) ? std::string() : std::string ("it has no system on tracks 0 to 2");

    if (!missing.empty())
    {
        result.output += "\nNot bootable: " + missing + "\n";
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReportListingDamage
//
//  Damage from the catalog walk, and from the track layer beneath it.
//
//  Each message must say the LISTING IS INCOMPLETE, not merely that the disk
//  is damaged. A pipeline that treats status 1 as a warning and continues
//  will otherwise consume a truncated listing that reads as whole, and its
//  log will describe a disk problem rather than a missing-entries problem.
//  The status carries the distinction for a script; the wording has to carry
//  it for whoever reads the log afterwards.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReportListingDamage (
    const CommandLineOptions             & options,
    const DiskImageSession::OpenedImage  & opened,
    const VolumeListing                  & listing,
    DiskCommandResult                    & result)
{
    char  summary[128] = {};



    for (const std::string & note : listing.damage)
    {
        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "",
            note + ". THIS LISTING IS INCOMPLETE, entries may be missing") + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded and read back as zeros "
                  "-- THIS LISTING IS INCOMPLETE, entries may be missing",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::AppendDirectoryRows
//
//  A RECURSIVE ROW GIVES ITS FULL PATH, because a listing that reaches
//  several directories has rows whose names repeat: two files called NOTES in
//  different directories are two rows reading NOTES, and nothing in the output
//  says which is which. The row is formatted from a copy whose name is the
//  path, so the columns stay exactly as a one-directory listing prints them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::AppendDirectoryRows (
    const IVolume      & volume,
    VolumeKind           kind,
    const std::string  & directory,
    bool                 recurse,
    DiskCommandResult  & result)
{
    HRESULT        hr        = S_OK;
    VolumeListing  listing;
    char           note[32]  = {};



    hr = volume.EnumerateDirectory (FilePath::Parse (directory), listing);
    CHR (hr);

    for (const FileEntry & entry : listing.entries)
    {
        FileEntry  shown   = entry;
        size_t     sharing = 0;

        for (const FileEntry & other : listing.entries)
        {
            sharing += (_stricmp (other.name.c_str(), entry.name.c_str()) == 0) ? 1 : 0;
        }

        if (recurse && !directory.empty())
        {
            shown.name = directory + "/" + entry.name;
        }

        result.output += (kind == VolumeKind::Dos33) ? FormatDos33Entry (shown) : FormatProDosEntry (shown);

        //  A name that several entries share cannot select one of them, so
        //  each such entry is listed with its index. A unique name has none.
        if (sharing > 1)
        {
            snprintf (note, sizeof (note), "  (index %u)", (unsigned) (entry.catalogIndex + 1));
            result.output += note;
        }

        result.output += "\n";
    }

    if (recurse)
    {
        for (const FileEntry & entry : listing.entries)
        {
            std::string  below = directory.empty() ? entry.name : directory + "/" + entry.name;

            if (entry.isDirectory)
            {
                hr = AppendDirectoryRows (volume, kind, below, true, result);
                CHR (hr);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunMkdir
//
//  Every directory along the path is made where it is not there yet, which is
//  what cmd's own mkdir does: `mkdir A/B/C` makes all three. A path that is
//  there in full already is an error rather than a quiet success, since the
//  caller asked for something to be made.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunMkdir (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           named    = !options.disk.path.empty();
    bool                           isProDos = false;
    bool                           madeAny  = false;
    size_t                         i        = 0;
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    typed;
    vector<Byte>                   staged;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<path>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    isProDos = opened.kind == VolumeKind::ProDos;

    CBRFEx (isProDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (opened.kind))));

    hr = ResolveVolumePath (options, opened, options.disk.path, typed, result);
    CHR (hr);

    path   = FilePath::Parse (typed);
    staged = opened.sectors;

    for (i = 1; i <= path.GetDepth(); i++)
    {
        ProDosVolume   step (staged);
        VolumeListing  listing;
        std::string    prefix;
        vector<Byte>   next;
        HRESULT        probe   = S_OK;
        size_t         j       = 0;
        bool           there   = false;

        for (j = 0; j < i; j++)
        {
            prefix += (prefix.empty() ? "" : "/") + path.GetComponents()[j];
        }

        probe = step.EnumerateDirectory (FilePath::Parse (prefix), listing);
        there = SUCCEEDED (probe);

        if (there)
        {
            continue;
        }

        hr = step.CreateDirectory (FilePath::Parse (prefix), next);

        CHRF (hr, result.FailAction (options.disk.imagePath, "create " + prefix,
                              ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

        staged  = next;
        madeAny = true;
    }

    CBRFEx (madeAny, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path, "it is already on this volume"));

    hr = m_session.SaveAndCommit (opened, staged, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunRmdir
//
//  PowerShell's meanings: a directory holding anything needs --recurse, and
//  --force takes the locked entries with it.
//
//  THE LIST COMES BEFORE THE QUESTION. A subtree removal is answered for by
//  whoever typed it, and nobody can answer for what they cannot see, so every
//  entry that would go is printed with the locked ones marked and the blocks
//  totaled, and only then is the question asked. --yes answers it in advance,
//  and with nobody at the keyboard the removal is turned down.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunRmdir (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr          = S_OK;
    bool                           named       = !options.disk.path.empty();
    bool                           isProDos    = false;
    bool                           holdsThings = false;
    bool                           permitted   = false;
    bool                           confirmed   = false;
    DiskImageSession::OpenedImage  opened;
    DirectoryRemovalPlan           plan;
    DeleteOutcome                  outcome;
    FilePath                       path;
    vector<Byte>                   edited;
    std::string                    typed;
    std::string                    declined;
    char                           note[128]   = {};



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<path>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    isProDos = opened.kind == VolumeKind::ProDos;

    CBRFEx (isProDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (opened.kind))));

    hr = ResolveVolumePath (options, opened, options.disk.path, typed, result);
    CHR (hr);

    path = FilePath::Parse (typed);

    {
        ProDosVolume  volume (opened.sectors);

        hr = volume.BuildRemovalPlan (path, plan);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    holdsThings = plan.entries.size() > 1;
    permitted   = !holdsThings || options.disk.recurse;

    CBRFEx (permitted, HRESULT_FROM_WIN32 (ERROR_DIR_NOT_EMPTY),
            result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                               ApplyPrefix ("it is not empty, and --recurse removes its entries with it")));

    if (holdsThings)
    {
        result.output += "Removing " + options.disk.path + ":\n";

        for (const DirectoryRemovalEntry & entry : plan.entries)
        {
            result.output += "  " + entry.path + (entry.isLocked ? "   (locked)" : "") + "\n";
        }

        snprintf (note, sizeof (note), "%u entries, %u blocks\n",
                  (unsigned) plan.entries.size(), (unsigned) plan.blocksFreed);

        result.output += note;
    }

    confirmed = options.disk.yes;
    declined  = "was not removed: nobody was there to answer, and --yes answers in advance";

    if (!confirmed && m_confirm)
    {
        confirmed = m_confirm ("Remove " + options.disk.path + " and everything below it?");
        declined  = "was not removed";
    }

    CBRFEx (confirmed, HRESULT_FROM_WIN32 (ERROR_CANCELLED),
            result.Fail (options.disk.imagePath, options.disk.path, ApplyPrefix (declined)));

    {
        ProDosVolume  volume (opened.sectors);

        hr = volume.RemoveDirectory (path, options.disk.force, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, options.disk.path, warning) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyEncoding
//
//  Turns the file's stored bytes into what the caller asked for. Verbatim is
//  the absence of a conversion and is the default, so a caller who says nothing
//  gets the bytes the disk holds.
//
//  AN ENCODING THIS BUILD CANNOT PERFORM IS REFUSED, NOT IGNORED. A flag that
//  is parsed and then silently dropped is worse than one that does not exist:
//  the caller reads "converted to a listing" in the help, gets tokenized bytes,
//  and has no way to tell the difference from a file that needed no conversion.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ApplyEncoding (
    const CommandLineOptions &  options,
    FilePayload              &  payload,
    DiskCommandResult        &  result)
{
    static constexpr Byte     s_kProDosIntegerType = 0xFA;
    HRESULT                   hr                   = S_OK;
    std::string               hostText;
    ApplesoftListingError     listingError;
    IntegerBasicListingError  integerError;
    char                      note[200]            = {};
    bool                      integer              = payload.type == Dos33Volume::kTypeInteger || payload.type == s_kProDosIntegerType;



    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            // Both conventions decode identically -- once the high bit is
            // ignored, Apple high-ASCII and plain-ASCII text differ in nothing -- so
            // this choice is inert on the read path. It becomes load-bearing
            // when writing, which is where the terminator gets chosen.
            //
            // Apple high-ASCII is the measured answer on both filesystems: the ProDOS
            // fixture volumes carry TXT files that are predominantly high-bit
            // with $8D terminators, not the plain seven-bit form usually
            // assumed for that filesystem.
            if (payload.textForm == PayloadTextForm::PascalPages)
            {
                PascalVolume::DecodeText (payload.bytes, hostText);
            }
            else if (payload.textForm == PayloadTextForm::CpmRecords)
            {
                CpmVolume::DecodeText (payload.bytes, hostText);
            }
            else
            {
                AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, hostText);
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            if (payload.textForm != PayloadTextForm::AppleHighAscii)
            {
                result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lbasic means nothing on this volume: it has no BASIC programs\n", options.flagPrefix);
                result.exitStatus   = DiskCommandResult::kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
                break;
            }

            //  An Integer BASIC program lists through its own tokens, as its
            //  preview does.
            if (integer)
            {
                hr = IntegerBasicDetokenizer::Detokenize (payload.bytes, hostText, integerError);

                if (FAILED (hr))
                {
                    snprintf (note, sizeof (note), "byte %u: %s\n", (unsigned) integerError.offset, integerError.reason.c_str());

                    result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lbasic cannot read this file as an Integer BASIC program: ", options.flagPrefix) + note;
                    result.exitStatus   = DiskCommandResult::kNoOutput;
                    break;
                }

                payload.bytes.assign (hostText.begin(), hostText.end());
                payload.encoding = PayloadEncoding::HostText;
                break;
            }

            hr = ApplesoftTokenizer::Detokenize (payload.bytes, hostText, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot read this file as an Applesoft "
                                   "BASIC program", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunGet
//
//  Extraction goes to a named file when one is given and to the process's own
//  output otherwise, which is what makes it pipeable. Both destinations go
//  through the seam so the platform's opinion about binary output is settled in
//  one place rather than at each call site.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunGet (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.path.empty();
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    std::string                    name;
    char                           note[128] = {};



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    hr = ResolveEntryPath (options, opened, path, name, result);
    CHR (hr);

    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->Read (path, payload);
    }

    result.failure = hr;

    CHRF (hr, result.FailAction (options.disk.imagePath, "read " + name,
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    hr = ApplyEncoding (options, payload, result);
    CHR (hr);

    result.resourceFork    = payload.resourceFork;
    result.finderInfo      = payload.finderInfo;
    result.hasResourceFork = payload.hasResourceFork;

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload.bytes);

        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        if (payload.hasResourceFork)
        {
            hr = WriteAppleDouble (options.disk.hostFile, payload);
            CHRF (hr, result.Fail (GetAppleDoublePath (options.disk.hostFile), "", "could not be written"));
        }
    }
    else
    {
        result.payload    = payload.bytes;
        result.hasPayload = true;
    }

    if (payload.hasLoadAddress)
    {
        snprintf (note, sizeof (note), "%s: loads at $%04X, %u bytes\n",
                  name.c_str(),
                  (unsigned) payload.loadAddress,
                  (unsigned) payload.bytes.size());

        result.diagnostics += note;
    }

    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (note, sizeof (note),
                  "%d %s could not be decoded. THIS FILE IS INCOMPLETE, "
                  "unreadable sectors were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, name, note) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunTest
//
//  Each problem the image's structure has, a line each: for a volume, the
//  units claimed twice, the units in use and marked free, the chains that
//  couldn't be followed and any catalog damage; for an archive, each record
//  whose header or data doesn't match its CRC. Units marked in use that no
//  file claims aren't listed: a DOS 3.3 disk's tracks for DOS are marked so
//  with no file on them. A clean image says so and exits clean.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunTest (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr     = S_OK;
    bool                           named  = !options.disk.imagePath.empty();
    DiskImageSession::OpenedImage  opened;
    VolumeIntegrityReport          report;
    VolumeListing                  listing;
    std::vector<std::string>       problems;



    CBRF (named, ReportMissingParameter ("<image>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    if (opened.kind == VolumeKind::Archive)
    {
        ArchiveVolume (opened.sectors).CollectDamage (problems);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->BuildIntegrityReport (report);
        CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be checked"));

        hr = volume->Enumerate (listing);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (uint32_t unit : report.GetCrossLinked())
        {
            problems.push_back (std::format ("unit {} is claimed by more than one file", unit));
        }

        for (uint32_t unit : report.GetClaimedButFree())
        {
            problems.push_back (std::format ("unit {} is in use but marked free", unit));
        }

        for (uint16_t owner : report.GetUnfollowableChains())
        {
            problems.push_back (std::format ("file {} has a block chain that couldn't be followed", owner + 1));
        }

        problems.insert (problems.end(), listing.damage.begin(), listing.damage.end());
    }

    for (const std::string & problem : problems)
    {
        result.output += options.disk.imagePath + ": " + problem + "\n";
    }

    if (problems.empty())
    {
        result.output += options.disk.imagePath + ": no problems found\n";
    }

    result.exitStatus = problems.empty() ? DiskCommandResult::kClean : DiskCommandResult::kWithComplaints;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetAppleDoublePath
//
//  `._NAME` beside `NAME`, the name AppleDouble's convention gives the file of
//  a data fork's other entries.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::GetAppleDoublePath (const std::string & hostFile)
{
    std::filesystem::path  path = hostFile;



    return (path.parent_path() / ("._" + path.filename().string())).string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::WriteAppleDouble
//
//  The resource fork, the Finder information and the ProDOS type of a file
//  whose data fork was just written to `hostFile`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::WriteAppleDouble (const std::string & hostFile, const FilePayload & payload)
{
    HRESULT          hr = S_OK;
    AppleSingleFile  single;
    vector<Byte>     bytes;



    single.resourceFork    = payload.resourceFork;
    single.finderInfo      = payload.finderInfo;
    single.hasResourceFork = true;
    single.hasProDosInfo   = true;
    single.access          = payload.access;
    single.fileType        = payload.type;
    single.auxType         = payload.auxType;

    AppleSingleCodec::EncodeDouble (single, bytes);

    hr = m_fileIo.WriteAllBytes (GetAppleDoublePath (hostFile), bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetOnDiskName
//
//  --as when the caller gave one, and otherwise the last component of the
//  component. Nothing is stripped or shortened on the way: the caller already
//  chose that name, and inventing a different one would leave them looking for
//  a file the catalog does not hold. A name the filesystem cannot store is
//  refused by the volume layer, which is where that rule belongs.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::GetOnDiskName (const CommandLineOptions & options)
{
    size_t       lastSeparator = options.disk.hostFile.find_last_of ("/\\");
    std::string  name          = options.disk.path;



    if (name.empty())
    {
        name = (lastSeparator == std::string::npos)
                   ? options.disk.hostFile
                   : options.disk.hostFile.substr (lastSeparator + 1);
    }

    return name;
}





//  Integer BASIC stops numbering lines at 32767, so the high bit of a line
//  number is clear and zero is not a line at all.
static constexpr Word    s_kMaxIntegerLineNumber = 32767;


//  How many links have to agree before a chain is a chain rather than a
//  coincidence. One proves nothing on a short file.
static constexpr size_t  s_kLinesThatProveAChain = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DetectFileType
//
//  What a file's own bytes say it is, or 0 when they say nothing.
//
//  APPLESOFT IS A LINKED LIST AND THAT IS WHAT IS WALKED. Every line is a
//  two-byte pointer to the next line, a two-byte line number, tokens, and a
//  zero terminator; a zero pointer ends the program. Walking that chain with
//  the pointers rising and the line numbers rising, and landing EXACTLY on the
//  last byte of the file, is a structure arbitrary data does not have. The
//  exact landing is what makes a false positive vanishingly unlikely: a binary
//  whose first four bytes happen to look like a header still has to have every
//  later line agree, and end where the file ends.
//
//  INTEGER BASIC IS THE SAME SHAPE INSIDE OUT: a leading LENGTH byte rather
//  than a trailing zero. Walked the same way, to the same exact landing.
//
//  A DOS 3.3 FOUR-BYTE HEADER IS DELIBERATELY NOT READ AS A TYPE. `put --load`
//  writes that header itself, so a file already carrying one is the doubled-
//  header mistake the worked example warns about, and quietly agreeing with it
//  would file bytes the guest runs as code.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskCommandRunner::DetectFileType (const vector<Byte> & bytes, VolumeKind kind)
{
    Byte    type     = 0;
    bool    isDos    = kind == VolumeKind::Dos33;
    size_t  at       = 0;
    Word    previous = 0;
    Word    lastLine = 0;
    size_t  seen     = 0;



    //  Applesoft: a chain of lines ending on a zero next-pointer.
    //  Two bytes, not four: the chain ENDS on a bare zero pointer, and a
    //  loop that demanded a whole line would walk past it and never see
    //  the end of the program.
    while (at + 2 <= bytes.size())
    {
        Word    next       = (Word) (bytes[at] | (bytes[at + 1] << 8));
        Word    line       = 0;
        size_t  terminator = at + 4;



        if (next == 0)
        {
            //  The chain ends here, and it has to end WITH the file.
            bool  landedOnTheEnd = (at + 2 == bytes.size());

            if (landedOnTheEnd && seen > 0)
            {
                type = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
            }

            break;
        }

        if (at + 4 > bytes.size())
        {
            break;
        }

        line = (Word) (bytes[at + 2] | (bytes[at + 3] << 8));

        //  Pointers rise, and so do line numbers.
        if (next <= previous || (seen > 0 && line <= lastLine))
        {
            break;
        }

        while (terminator < bytes.size() && bytes[terminator] != 0)
        {
            terminator++;
        }

        if (terminator >= bytes.size())
        {
            break;
        }

        previous = next;
        lastLine = line;
        seen++;
        at       = terminator + 1;
    }

    //  Integer BASIC: a chain of length-prefixed lines, and DOS 3.3's alone.
    //
    //  TIGHTER THAN THE APPLESOFT WALK BECAUSE IT HAS LESS TO GO ON. Applesoft
    //  carries a pointer per line that has to rise AND land on the last byte,
    //  and measured over 200,000 random blobs it never once fired. Integer has
    //  only a length byte, so a short file can chain to its own end by
    //  accident: the same measurement put it at 0.39% on 16-byte blobs.
    //
    //  Two free constraints close that. A line number stops at 32767 on this
    //  machine, so the high bit is clear and zero is not a line; and three
    //  links have to agree rather than one. Re-measured, 0.39% becomes 0.0025%,
    //  and what a rejected file falls back to is B, which is where it would
    //  have gone anyway.
    at       = 0;
    lastLine = 0;
    seen     = 0;

    while (type == 0 && isDos && at + 3 <= bytes.size())
    {
        Byte  length = bytes[at];
        Word  line   = (Word) (bytes[at + 1] | (bytes[at + 2] << 8));



        if (length < 4 || at + length > bytes.size())
        {
            break;
        }

        if (line == 0 || line > s_kMaxIntegerLineNumber)
        {
            break;
        }

        if (seen > 0 && line <= lastLine)
        {
            break;
        }

        lastLine = line;
        seen++;
        at      += length;

        if (at == bytes.size())
        {
            type = (seen >= s_kLinesThatProveAChain) ? Dos33Volume::kTypeInteger : (Byte) 0;
            break;
        }
    }

    return type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveFileType
//
//  The two filesystems number their types differently and name them
//  differently, so the forms accepted here are each one's own. A letter
//  that means Applesoft on DOS 3.3 and nothing on ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  A caller who named no type gets the one that matches the conversion they
//  asked for -- text when they asked for text, a binary otherwise, which is
//  what a build loop places.
//
//  AN UNRECOGNIZED FORM IS REFUSED, not defaulted. Defaulting would place
//  the file under a type the caller did not ask for and say nothing, and the
//  guest would report the mismatch much later as a file that will not load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveFileType (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    Byte                      & outType,
    DiskCommandResult         & result)
{
    HRESULT      hr         = S_OK;
    bool         isDos      = kind == VolumeKind::Dos33;
    bool         isText     = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text;
    bool         isBasic    = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Basic;
    bool         named      = !options.disk.typeName.empty();
    bool         recognized = true;
    size_t       i          = 0;
    std::string  form       = options.disk.typeName;
    bool         isReadOnly = false;
    bool         isSystem   = false;



    if (kind == VolumeKind::Pascal)
    {
        hr = ResolvePascalKind (options, outType, result);
        BAIL_OUT_IF (true, hr);
    }

    if (kind == VolumeKind::Cpm)
    {
        outType = 0;

        //  The attributes are read from the same word in BuildPutPayload.
        recognized = !named || ParseCpmAttributes (options.disk.typeName, isReadOnly, isSystem);

        CBRFEx (recognized, E_NOTIMPL,
                (result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype means nothing on this volume: CP/M has no file types, only the attributes RO and SYS\n", options.flagPrefix),
                 result.exitStatus   = DiskCommandResult::kNoOutput));

        BAIL_OUT_IF (true, S_OK);
    }

    if (isBasic)
    {
        // A tokenized program stored under any other type is a file the guest
        // will not RUN, so the conversion picks the type rather than leaving it
        // to a default meant for a build loop's binaries.
        outType = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
    }
    else if (isText)
    {
        outType = isDos ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
    }
    else
    {
        //  What the file says it is, and a binary when it says nothing.
        Byte  detected = DetectFileType (hostBytes, kind);

        outType = (detected != 0)
                      ? detected
                      : (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
    }

    BAIL_OUT_IF (!named, S_OK);

    for (i = 0; i < form.size(); i++)
    {
        if (form[i] >= 'a' && form[i] <= 'z')
        {
            form[i] = (char) (form[i] - 'a' + 'A');
        }
    }

    if (isDos)
    {
        if      (form == "T" || form == "TXT") { outType = Dos33Volume::kTypeText; }
        else if (form == "I" || form == "INT") { outType = Dos33Volume::kTypeInteger; }
        else if (form == "A" || form == "BAS") { outType = Dos33Volume::kTypeApplesoft; }
        else if (form == "B" || form == "BIN") { outType = Dos33Volume::kTypeBinary; }
        else if (form == "R" || form == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                           { recognized = false; }
    }
    else
    {
        if      (form == "T" || form == "TXT") { outType = ProDosVolume::kTypeText; }
        else if (form == "B" || form == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (form == "A" || form == "BAS") { outType = ProDosVolume::kTypeBasic; }
        else if (form == "S" || form == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                           { recognized = false; }
    }

    if (!recognized)
    {
        result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype ", options.flagPrefix) + options.disk.typeName
                            + " means nothing on this volume: "
                            + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                                     : "ProDOS takes TXT, BIN, BAS or SYS")
                            + "\n";

        result.exitStatus   = DiskCommandResult::kNoOutput;
        hr                  = E_NOTIMPL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ParseCpmAttributes
//
//  RO, R/O or SYS, or two of them with a comma, from what STAT accepts.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::ParseCpmAttributes (const std::string & text, bool & outReadOnly, bool & outSystem)
{
    size_t  start = 0;



    outReadOnly = false;
    outSystem   = false;

    while (start <= text.size())
    {
        size_t       comma = text.find (',', start);
        std::string  word  = text.substr (start, (comma == std::string::npos) ? std::string::npos : comma - start);

        if (_stricmp (word.c_str(), "RO") == 0 || _stricmp (word.c_str(), "R/O") == 0)
        {
            outReadOnly = true;
        }
        else if (_stricmp (word.c_str(), "SYS") == 0)
        {
            outSystem = true;
        }
        else
        {
            return false;
        }

        if (comma == std::string::npos)
        {
            break;
        }

        start = comma + 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolvePascalKind
//
//  The kind --type selects. With no --type, text is TEXT, and anything else
//  takes its kind from the name in the volume, as Pascal's Filer does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolvePascalKind (const CommandLineOptions & options, Byte & outType, DiskCommandResult & result)
{
    static constexpr struct { const char * form; Byte kind; }  kForms[] =
    {
        { "BAD",  PascalVolume::kKindBad  },
        { "CODE", PascalVolume::kKindCode },
        { "TEXT", PascalVolume::kKindText },
        { "INFO", PascalVolume::kKindInfo },
        { "DATA", PascalVolume::kKindData },
        { "GRAF", PascalVolume::kKindGraf },
        { "FOTO", PascalVolume::kKindFoto },
    };
    HRESULT  hr         = S_OK;
    bool     recognized = options.disk.typeName.empty();



    outType = (options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text) ? PascalVolume::kKindText : PascalVolume::kKindNone;

    for (const auto & row : kForms)
    {
        if (_stricmp (options.disk.typeName.c_str(), row.form) == 0)
        {
            outType    = row.kind;
            recognized = true;
        }
    }

    if (!recognized)
    {
        result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype ", options.flagPrefix) + options.disk.typeName
                            + " means nothing on this volume: Apple Pascal takes BAD, CODE, TEXT, INFO, DATA, GRAF or FOTO\n";
        result.exitStatus   = DiskCommandResult::kNoOutput;
        hr                  = E_NOTIMPL;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildPutPayload
//
//  What a file from the host becomes on the disk.
//
//  VERBATIM IS THE ABSENCE OF A CHARACTER CONVERSION AND NOTHING MORE. The
//  bytes go down as they came in; the length and whatever header the type
//  carries are still applied below this, because those record where the file
//  ENDS and are its identity rather than a transformation of it. That is what
//  makes extract-edit-replace safe: the bytes the caller did not touch come
//  back exactly as they were.
//
//  Text is written in the Apple high-ASCII convention on BOTH filesystems, which is
//  the measured answer rather than the assumed one -- the ProDOS fixture
//  volumes' own TXT files are predominantly high-bit with $8D terminators. It
//  is also what the read path decodes, so the two directions agree.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::BuildPutPayload (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    FilePayload               & outPayload,
    DiskCommandResult         & result)
{
    HRESULT                hr           = S_OK;
    Byte                   type         = 0;
    size_t                 badOffset    = 0;
    char                   note[160]    = {};
    std::string            hostText;
    ApplesoftListingError  listingError;
    bool                   hasAddresses = kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos || kind == VolumeKind::Archive;
    bool                   isReadOnly   = false;
    bool                   allowed      = hasAddresses || (!options.disk.hasLoadAddress && options.disk.encoding != CommandLineOptions::DiskOptions::Encoding::Basic);



    CBRFEx (allowed, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
            (result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lload and %Lbasic mean nothing on this volume: it has no load addresses or BASIC programs\n", options.flagPrefix),
             result.exitStatus   = DiskCommandResult::kNoOutput));

    hr = ResolveFileType (options, kind, hostBytes, type, result);
    CHR (hr);

    outPayload.type           = type;
    outPayload.loadAddress    = options.disk.loadAddress;
    outPayload.hasLoadAddress = options.disk.hasLoadAddress;

    if (kind == VolumeKind::Cpm && !options.disk.typeName.empty() && ParseCpmAttributes (options.disk.typeName, isReadOnly, outPayload.isSystem) && isReadOnly)
    {
        outPayload.access    = FilePayload::kAccessLocked;
        outPayload.hasAccess = true;
    }

    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            outPayload.bytes    = hostBytes;
            outPayload.encoding = PayloadEncoding::Verbatim;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            hostText.assign (hostBytes.begin(), hostBytes.end());

            if (kind == VolumeKind::Cpm)
            {
                CpmVolume::EncodeText (hostText, outPayload.bytes);
            }
            else
            {
                hr = (kind == VolumeKind::Pascal) ? PascalVolume::EncodeText (hostText, outPayload.bytes, badOffset)
                                                  : AppleTextCodec::Encode (hostText, AppleTextConvention::HighAscii, outPayload.bytes, badOffset);
            }

            if (FAILED (hr))
            {
                // Naming the offset is the whole value of the refusal. A smart
                // quote pasted into a listing looks identical to a plain one in
                // an editor, and "somewhere in this file" is not actionable.
                snprintf (note, sizeof (note),
                          "byte %u of the file has no Apple II text representation, "
                          "so nothing was converted\n",
                          (unsigned) badOffset);

                result.diagnostics += note;
                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            outPayload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            if (options.disk.hasLoadAddress)
            {
                // An Applesoft program loads where Applesoft keeps its program
                // and nowhere else, so an address here is a request that cannot
                // be honored. Accepting and ignoring it would place the program
                // and leave the caller believing it loads somewhere it does not.
                result.diagnostics += DiskHelpPage::ApplyPrefixes (
                    "%Lload means nothing with %Lbasic: "
                    "an Applesoft BASIC program always loads at $0801\n", options.flagPrefix);
                result.exitStatus   = DiskCommandResult::kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER);
                break;
            }

            hostText.assign (hostBytes.begin(), hostBytes.end());

            hr = ApplesoftTokenizer::Tokenize (hostText, outPayload.bytes, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot make an Applesoft BASIC program of "
                                   "this listing", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            // ProDOS records where a BASIC program loads in the auxiliary type;
            // DOS 3.3 records nothing and ignores this. Setting it on both keeps
            // the branch out of here, where it would be a second statement of
            // which filesystem stores what.
            outPayload.auxType        = ApplesoftTokenizer::kProgramBase;
            outPayload.hasAuxType     = true;
            outPayload.hasLoadAddress = false;
            outPayload.encoding       = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyHostFileName
//
//  Read a host name in one of the forms Casso Explorer writes on Copy or a
//  drag: `NAME.Applesoft BASIC.txt`, `NAME#062000`, `NAME.Binary.$2000.bin`
//  and the rest. With no --as, the entry takes NAME, and a listing's form
//  selects its conversion when the command line selects none. Any other host
//  name is left as it is: `PROG.BIN` goes in as PROG.BIN.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::ApplyHostFileName (CommandLineOptions & inOutOptions, ParsedHostName & outParsed)
{
    size_t        lastSeparator = inOutOptions.disk.hostFile.find_last_of ("/\\");
    std::string   leaf          = (lastSeparator == std::string::npos) ? inOutOptions.disk.hostFile
                                                                       : inOutOptions.disk.hostFile.substr (lastSeparator + 1);
    bool          unconverted   = inOutOptions.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim
                               && inOutOptions.disk.typeName.empty();



    if (!inOutOptions.disk.path.empty() || !HostFileNaming::Parse (TextEncoding::NarrowToWide (leaf), outParsed) || outParsed.catalogName.empty())
    {
        return false;
    }

    //  A conversion with no import is a host file like any other. One with
    //  an import is text on a file system the import isn't for.
    if (outParsed.converted == ParsedHostName::ConvertedKind::Format && FileFormatTable::GetImportBySuffix (TextEncoding::NarrowToWide (leaf)) == FileFormat::None)
    {
        return false;
    }

    inOutOptions.disk.path = outParsed.catalogName;

    if (unconverted && outParsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
    }
    else if (unconverted && outParsed.converted != ParsedHostName::ConvertedKind::None)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyHostFileType
//
//  The ProDOS type in a host name, mapped to DOS 3.3's on a DOS 3.3 image,
//  with the aux type as the load address of a binary file, as Casso Explorer
//  puts the same file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ApplyHostFileType (const ParsedHostName & parsed, VolumeKind kind, FilePayload & inOutPayload)
{
    HRESULT  hr      = S_OK;
    bool     isDos   = kind == VolumeKind::Dos33;
    Byte     dosType = 0;
    bool     mapped  = false;



    if (kind == VolumeKind::Pascal || kind == VolumeKind::Cpm)
    {
        inOutPayload.type = (kind == VolumeKind::Pascal) ? HostFileNaming::MapProDosToPascalKind (parsed.type) : (Byte) 0;
        BAIL_OUT_IF (true, S_OK);
    }

    mapped = !isDos || HostFileNaming::TryMapProDosToDos33 (parsed.type, dosType);
    CBREx (mapped, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    inOutPayload.type = isDos ? dosType : parsed.type;

    if (parsed.hasAux)
    {
        inOutPayload.hasAuxType     = !isDos;
        inOutPayload.auxType        = parsed.aux;
        inOutPayload.hasLoadAddress = inOutPayload.type == (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
        inOutPayload.loadAddress    = parsed.aux;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunPut
//
//  Placing a file from the host on the disk: read it, convert it if asked, let the
//  filesystem compute the whole new volume, render that back into the container
//  and commit it.
//
//  NOTHING IS WRITTEN UNTIL EVERY DECISION HAS BEEN MADE. Each refusal below --
//  no such file on the host, a type nobody recognizes, a name the catalog cannot
//  store, a locked file, a volume with no room -- happens with the image on
//  disk untouched, because the only thing this path can commit is a finished
//  buffer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunPut (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr        = S_OK;
    bool                           named     = !options.disk.hostFile.empty();
    CommandLineOptions             effective = options;
    ParsedHostName                 parsed;
    bool                           hostForm  = false;
    std::string                    diskName;
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    vector<Byte>                   hostBytes;
    vector<Byte>                   edited;
    std::wstring                   importError;
    bool                           imported  = false;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<file>", result));

    hostForm = ApplyHostFileName (effective, parsed);
    diskName = GetOnDiskName (effective);

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    hr = ApplyUser (options, opened.kind, diskName, result);
    CHR (hr);

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, hostBytes);

    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A conversion Casso Explorer wrote that has an import for this file
    //  system is put back in its Apple form, under the name that form has
    //  there, unless the command line has --type, --load, --text or --basic.
    imported = hostForm && parsed.converted == ParsedHostName::ConvertedKind::Format
            && options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim
            && options.disk.typeName.empty() && !options.disk.hasLoadAddress
            && HostFileNaming::CanImport (parsed.format, opened.kind);

    if (imported)
    {
        if (options.disk.path.empty())
        {
            diskName = HostFileNaming::MakeImportName (diskName, parsed.format, opened.kind);
        }

        hr = FileFormatTable::Import (parsed.format, opened.kind, hostBytes, payload, importError);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, TextEncoding::WideToNarrow (importError)));
    }
    else
    {
        hr = BuildPutPayload (effective, opened.kind, hostBytes, payload, result);
        CHR (hr);
    }

    //  The type and address in a name Casso Explorer wrote, unless the command
    //  line has --type, --load, --text or --basic.
    if (hostForm && parsed.hasType && effective.disk.typeName.empty() && !effective.disk.hasLoadAddress
        && effective.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        hr = ApplyHostFileType (parsed, opened.kind, payload);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, "it has a ProDOS type that DOS 3.3 has no file type for"));
    }

    path = FilePath::Parse (diskName);

    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->Write (path, payload, edited);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName,
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunDelete
//
//  Removal, plus the account of what it declined to do.
//
//  THE ACCOUNT IS THE POINT OF REPORTING AT ALL. A delete frees only space the
//  file uniquely owned; anything another catalog entry also claims is left
//  allocated, because freeing it would damage that other file and nothing would
//  say so until somebody read it, possibly weeks later. Leaked space is
//  recoverable at any time and a cross-linked free is not, so the asymmetry
//  runs this way -- and the user is told, on the error stream, with the command
//  still reporting that it succeeded.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunDelete (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr      = S_OK;
    bool                           named   = !options.disk.path.empty();
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    name;
    DeleteOutcome                  outcome;
    vector<Byte>                   edited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    hr = ResolveEntryPath (options, opened, path, name, result);
    CHR (hr);

    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);



        hr = volume->Delete (path, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "delete " + name,
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, name, warning) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunKrunch
//
//  All or nothing: the moved files and the new directory reach the image in one
//  commit, or the image stays as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunKrunch (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           isPascal = false;
    int                            moved    = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   edited;



    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    isPascal = opened.kind == VolumeKind::Pascal;

    CBRFEx (isPascal, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "krunch the disk", "krunch applies to Apple Pascal disks, the only file system here that needs its free space combined"));

    hr = PascalVolume (opened.sectors).Krunch (edited, moved);

    CHRF (hr, result.FailAction (options.disk.imagePath, "krunch the disk",
                                 (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA)) ? std::string ("the directory is damaged, so moving files could overwrite one")
                                                                                 : DescribeVolumeRefusal (hr, opened.kind)));

    if (moved > 0)
    {
        hr = m_session.SaveAndCommit (opened, edited, result);
        CHR (hr);
    }

    result.output += std::format ("{}: {} {} moved\n", options.disk.imagePath, moved, Utils::GetSingularOrPluralForm (moved, "file", "files"));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveVolumePath
//
//  `/MYDISK/GAMES/CHESS` is a whole ProDOS path: the volume, then the way down
//  through it. `GAMES/CHESS` is the same place read from the volume directory.
//  The two forms are what a ProDOS user types, and the first says which disk it
//  means, so it is checked rather than dropped -- a full path naming another
//  volume is a caller acting on the wrong disk, and that is worth turning down.
//
//  A DOS 3.3 NAME IS ONE NAME, SLASHES AND ALL. That filesystem has no
//  directories and no volume names, and a catalog will hold a name with a
//  slash in it, so nothing is stripped there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveVolumePath (
    const CommandLineOptions             & options,
    const DiskImageSession::OpenedImage  & opened,
    const std::string                    & typed,
    std::string                          & outPath,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    FilePath       path      = FilePath::Parse (typed);
    bool           plain     = !path.IsRooted() || opened.kind != VolumeKind::ProDos;
    bool           mine      = false;
    size_t         i         = 0;
    VolumeListing  listing;
    std::string    rest;
    char           note[128] = {};



    outPath = typed;

    BAIL_OUT_IF (plain, S_OK);

    {
        ProDosVolume  volume (opened.sectors);

        hr = volume.Enumerate (listing);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

    mine = !path.IsEmpty() && _stricmp (path.GetComponents()[0].c_str(), listing.volumeName.c_str()) == 0;

    snprintf (note, sizeof (note), "is a path on another volume; this image is /%s", listing.volumeName.c_str());

    CBRFEx (mine, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND),
            result.Fail (options.disk.imagePath, typed, note));

    for (i = 1; i < path.GetComponents().size(); i++)
    {
        rest += (rest.empty() ? "" : "/") + path.GetComponents()[i];
    }

    outPath = rest;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ApplyUser
//
//  --user as the volume layer's U:NAME form, on CP/M alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ApplyUser (const CommandLineOptions & options, VolumeKind kind, std::string & inOutName, DiskCommandResult & result)
{
    HRESULT  hr    = S_OK;
    bool     isCpm = kind == VolumeKind::Cpm;



    BAIL_OUT_IF (!options.disk.hasUser, S_OK);

    CBRFEx (isCpm, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.Fail (options.disk.imagePath, "", ApplyPrefix ("%Luser means nothing on this volume: only CP/M has user numbers")));

    if (options.disk.user != 0)
    {
        inOutName = std::format ("{}:{}", options.disk.user, inOutName);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveEntryPath
//
//  Without --index, the path is the name as typed. With it, the path is the
//  DOS 3.3 entry at that catalog position, and a name given alongside must
//  match that entry, so an outdated index fails instead of acting on a
//  different file.
//
//  --INDEX IS VALID ONLY FOR A NAME THAT SEVERAL ENTRIES SHARE. ProDOS names
//  are unique within a directory, and a DOS 3.3 name that only one entry has
//  already identifies that entry, so any other use is an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveEntryPath (
    const CommandLineOptions             & options,
    const DiskImageSession::OpenedImage  & opened,
    FilePath                             & outPath,
    std::string                          & outName,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    std::string    notAllowed = "--index is only allowed when identifying a specific file with a non-unique name";
    size_t         position  = options.disk.index - 1;
    size_t         sharing   = 0;
    bool           isDos     = opened.kind == VolumeKind::Dos33;
    bool           inRange   = false;
    bool           matches   = true;
    bool           shared    = false;
    VolumeListing  listing;
    std::string    typed;
    char           note[128] = {};



    hr = ResolveVolumePath (options, opened, options.disk.path, typed, result);
    CHR (hr);

    hr = ApplyUser (options, opened.kind, typed, result);
    CHR (hr);

    outName = typed;
    outPath = FilePath::Parse (typed);

    BAIL_OUT_IF (!options.disk.hasIndex, S_OK);

    //  A ProDOS disk, an index past the catalog, and an index for a name only
    //  one entry has are the same mistake, so all three get the same message.
    CBRFEx (isDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.Fail (options.disk.imagePath, "", ApplyPrefix (notAllowed.c_str())));

    {
        Dos33Volume  volume (opened.sectors);

        hr = volume.Enumerate (listing);
    }

    CHRF (hr, result.Fail (options.disk.imagePath, "", "catalog could not be read"));

    inRange = position < listing.entries.size();
    CBRFEx (inRange, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), result.Fail (options.disk.imagePath, "", ApplyPrefix (notAllowed.c_str())));

    if (!typed.empty())
    {
        matches = _stricmp (typed.c_str(), listing.entries[position].name.c_str()) == 0;
    }

    snprintf (note, sizeof (note), "does not match index %u", (unsigned) options.disk.index);
    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), result.Fail (options.disk.imagePath, options.disk.path, note));

    for (const FileEntry & entry : listing.entries)
    {
        sharing += (_stricmp (entry.name.c_str(), listing.entries[position].name.c_str()) == 0) ? 1 : 0;
    }

    shared = sharing > 1;
    CBRFEx (shared, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
            result.Fail (options.disk.imagePath, "", ApplyPrefix (notAllowed.c_str())));

    outName = listing.entries[position].name;
    outPath = FilePath::FromName (outName).WithLeafIndex (position);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBoot
//
//  Which program the disk runs once its operating system has loaded.
//
//  A PROGRAM THAT IS NOT ON THE VOLUME IS REFUSED, BY NAME. Setting a startup
//  program is the one edit whose mistake is invisible until somebody boots the
//  disk -- there is no listing that shows it and no file that appears -- so a
//  typo accepted here surfaces as a machine that boots to an error, hours later
//  and somewhere else. The volume layer looks the name up for exactly this
//  reason, and the message says which name failed rather than that something
//  did.
//
//  The two filesystems reach the same outcome by entirely different means, and
//  neither of them is a write of a file: DOS 3.3 patches a name into its own
//  image and ProDOS reorders its volume directory. Both arrive here as one call
//  and leave through the same commit as every other edit.
//
//  A DOS 3.3 GREETING IS RUN, NOT BRUN, AND THAT IS SAID OUT LOUD. Booting the
//  stock master with a binary named as its greeting was measured: the disk
//  boots, the program does not run, and the screen shows no complaint anybody
//  would connect to it. The name field is the only thing this sets, so the
//  command DOS issues stays RUN -- which leaves a placement that succeeded and a
//  disk that does nothing. Saying so costs one line and the complaints status;
//  refusing outright would be wrong, since a disk whose boot command has been
//  patched by hand is a real thing and its owner knows what they are doing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBoot (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    HRESULT                        listHr   = S_OK;
    bool                           named    = !options.disk.path.empty();
    bool                           runnable = true;
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    name;
    VolumeListing                  listing;
    vector<Byte>                   edited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result);
    CHR (hr);

    hr = ResolveEntryPath (options, opened, path, name, result);
    CHR (hr);

    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);



        hr = volume->SetStartupProgram (path, edited);

        if (SUCCEEDED (hr) && opened.kind == VolumeKind::Dos33)
        {
            listHr = volume->Enumerate (listing);
            IGNORE_RETURN_VALUE (listHr, S_OK);

            runnable = IsRunnableAsDos33Greeting (listing, name);
        }
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "make " + name + " the startup program",
                          ApplyPrefix (DescribeVolumeRefusal (hr, opened.kind))));

    //  REFUSED BEFORE ANYTHING IS WRITTEN, not reported after.
    //
    //  This used to set the name, commit the image, and then say the disk
    //  would boot without running it. The reasoning was that a DOS patched by
    //  hand to BRUN rather than RUN is a real thing and refusing would block
    //  it. What it produced for everyone else was a command that reported
    //  trouble and changed the disk anyway, leaving a volume configured to
    //  start a program that cannot start -- which is what ProDOS refuses
    //  outright, two screens away in the same command.
    CBRFEx (runnable, HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE),
            result.FailAction (options.disk.imagePath, "make " + name + " the startup program",
                               "a booting DOS 3.3 runs its greeting program with RUN, which "
                               "starts an Applesoft BASIC or Integer BASIC program, and this "
                               "file is neither"));

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::HasProDosSystemFile
//
//  Whether the volume carries PRODOS, the file its boot block loads. Without
//  it the loader has nothing to start, so the disk is not bootable whatever
//  else it holds.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::HasProDosSystemFile (const VolumeListing & listing)
{
    for (const FileEntry & entry : listing.entries)
    {
        if (entry.name == "PRODOS")
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::IsRunnableAsDos33Greeting
//
//  The volume layer's rule, reached through this name because the callers here
//  already read this way.
//
//  THE RULE ITSELF MOVED so the assembler can apply the same one. Two routes
//  can set a startup program now, and a copy of this in each is how they would
//  come to accept different things.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                                   const std::string    & name)
{
    return Dos33Volume::IsRunnableAsGreeting (listing, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Run
//
//  A command this build does not implement reports failure rather than doing
//  nothing quietly, so an absent capability cannot be mistaken for a completed
//  operation.
//
//  A HELP REQUEST IS A COMMAND HERE, and it is answered on the output stream with
//  a clean status. It used to be recognized only as the FIRST argument of the
//  whole command line, so `disk --help` offered `--help` to the command table and
//  answered a request for the grammar by refusing to run and complaining about
//  the grammar -- exit 2 for a question the tool knows the answer to.
//
//  A REFUSED COMMAND LINE RUNS NOTHING, for the reason `run` gives: this
//  grammar has no ignorable mistake in it. An option it did not recognize might
//  have named the file to write, chosen the type the catalog records, or given
//  the load address a binary needs, so carrying on would edit a disk on terms
//  nobody asked for. The parser has already said what was wrong, in the words
//  of the argument it could not take; repeating it here would report one
//  mistake twice.
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandResult DiskCommandRunner::Run (const CommandLineOptions & options)
{
    HRESULT                   hr               = S_OK;   // vestigial, for the bails
    DiskCommandResult         result;
    std::vector<std::string>  missing;
    bool                      refused          = options.parseVerdict == CommandLineOptions::ParseVerdict::Refused;
    bool                      allOperandsGiven = false;
    bool                      known            = false;
    VolumeKind                chosen           = VolumeKind::Unknown;



    m_command    = options.disk.command;
    m_flagPrefix = options.flagPrefix;

    CBRF (!refused, result.exitStatus = DiskCommandResult::kNoOutput);

    //  ASKED ONCE, BEFORE ANY COMMAND RUNS. Each command used to check its own
    //  operands wherever it first needed them, which reported whichever one
    //  that command happened to reach first and never the rest.
    missing          = FindMissingParameters (options);
    allOperandsGiven = missing.empty();
    CBRF (allOperandsGiven, ReportMissingParameters (missing, result));

    chosen = FilesystemChoices::FromWord (options.disk.filesystem);
    known  = options.disk.filesystem.empty() || chosen != VolumeKind::Unknown;

    CBRF (known, (result.diagnostics += "Error: unknown file system: " + options.disk.filesystem + "\n",
                  result.exitStatus   = DiskCommandResult::kNoOutput));

    m_session.SetChosenKind (chosen);

    switch (options.disk.command)
    {
        case CommandLineOptions::DiskOptions::Command::List:
            RunList (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Get:
            RunGet (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Put:
            RunPut (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Delete:
            RunDelete (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Boot:
            RunBoot (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Mkdir:
            RunMkdir (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Rmdir:
            RunRmdir (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockRead:
            RunBlockRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockWrite:
            RunBlockWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Krunch:
            RunKrunch (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Test:
            RunTest (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorRead:
            RunSectorRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorWrite:
            RunSectorWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Create:
            RunCreate (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Init:
            RunInit (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Help:
            result.output     += DiskHelpPage::BuildHelpText (options.flagPrefix, m_banner);
            result.exitStatus  = DiskCommandResult::kClean;
            break;

        default:
            //  NAMING NO COMMAND PRINTS THE PAGE AND SAYS NOTHING ELSE. There
            //  is one thing a reader in that position needs and it is the list
            //  of commands; a sentence telling them they gave none is a line
            //  between them and it. The status is still 2, because nothing was
            //  done: a script that reached here with an empty variable has to
            //  be able to tell.
            if (options.disk.commandWord.empty())
            {
                //  AND SAYS SO, or the edge prints the page a second time.
                //  A bad command line makes the caller print the whole disk
                //  page unless the runner has already answered with usage;
                //  this arm was answering with usage and not saying it, so
                //  `disk` alone printed 326 lines where 163 were meant.
                result.output     += DiskHelpPage::BuildHelpText (options.flagPrefix, m_banner);
                result.usageShown  = true;
            }
            else
            {
                result.diagnostics += "Error: unknown disk command: "
                                    + options.disk.commandWord + "\n";
            }

            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            break;
    }

    //  ANNOUNCED AFTER THE COMMIT, NEVER BEFORE, and only when there was one.
    //
    //  A receiver reads the image when it acts on the hint, so an intent sent
    //  first would describe contents that are not on disk yet. Here the command
    //  has finished and the bytes are in the file.
    //
    //  ONLY WHEN THE COMMAND WROTE SOMETHING. Announcing a change after `list`
    //  or `get` would send an emulator to re-read a file nothing touched, which
    //  it would then correctly ignore -- a wasted round trip stating something
    //  untrue.
    AnnounceIntent (options, result);

    //  THE ONE COMMAND'S BLOCK, NOT ALL NINE.
    //
    //  A missing operand already answered this way and a bad option value did
    //  not, so the same command answered `sectorread` with no image in 18
    //  lines and `sectorread --track 99` in 194. Both readers have said which
    //  command they want; the difference was in how they got it wrong, which
    //  is no reason to hand one of them every other command on the page.
    //
    //  Here rather than at each refusal, so a refusal added later cannot
    //  forget it and the sites stay about the reason. An unrecognized command
    //  word is deliberately excluded: it leaves the command as None, and a
    //  reader who has not landed on a command is the one case the whole page
    //  is the answer to.
    if (result.badCommandLine
        && !result.usageShown
        && options.disk.command != CommandLineOptions::DiskOptions::Command::None
        && options.disk.command != CommandLineOptions::DiskOptions::Command::Help)
    {
        result.output     += DiskHelpPage::BuildCommandHelp (m_command, m_flagPrefix);
        result.usageShown  = true;
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetAdvertisedContainers
//
//  The container table, for anyone who has to agree with it.
//
////////////////////////////////////////////////////////////////////////////////

const DiskCommandRunner::ContainerName * DiskCommandRunner::GetAdvertisedContainers (size_t & outCount)
{
    outCount = _countof (s_kContainers);

    return s_kContainers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatContainerWordList
//
//  The advertised words as a list in a sentence: `dsk, do, po and woz`.
//
//  READ OFF THE TABLE RATHER THAN TYPED OUT, because a sentence that names
//  the containers is a promise about what the tool accepts. Written by hand it
//  is a promise nothing keeps: `do` sat in the table, in both of these
//  sentences and in the extension reader, and the builder refused it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatContainerWordList (const char * prefix, const char * conjunction)
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
//  DiskCommandRunner::DescribeSpecRefusal
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

std::string DiskCommandRunner::DescribeSpecRefusal (const BlankDiskSpec & spec)
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
//  DiskCommandRunner::ResolveContainer
//
//  Which container a new image is written as.
//
//  --type when it is given, the file's own extension when it is not. An
//  unknown word is refused BY NAME with the ones that exist: somebody who
//  typed `--type 2mg` meant it, and handing them a .dsk instead is a disk they
//  did not ask for under a name they did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveContainer (const CommandLineOptions & options,
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
                                             + ApplyPrefix ("       Use %Ltype to specify the type.\n"),
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
//  DiskCommandRunner::ResolveContents
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

HRESULT DiskCommandRunner::ResolveContents (const CommandLineOptions & options,
                                            BlankDiskContents        & outContents,
                                            DiskCommandResult        & result)
{
    HRESULT      hr         = S_OK;   // vestigial, for the bail
    bool         recognized = true;
    std::string  asked      = options.disk.formatName;



    for (char & letter : asked)
    {
        letter = (char) tolower ((unsigned char) letter);
    }

    if (asked.empty() || asked == "dos33" || asked == "dos" || asked == "dos3.3")
    {
        outContents = BlankDiskContents::Dos33;
    }
    else if (asked == "prodos")
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
//  DiskCommandRunner::ResolveVolume
//
//  --volume, which is a NUMBER under DOS 3.3 and a NAME under ProDOS.
//
//  The two filesystems label a disk differently and one flag serves both, so
//  which one the word is read as follows from the format already chosen rather
//  than from how the word looks. A ProDOS volume legitimately called `254`
//  would otherwise become a DOS volume number, silently.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveVolume (const CommandLineOptions & options,
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
            (result.diagnostics    += ApplyPrefix ("Error: CP/M has no volume name or number\n"
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
//  DiskCommandRunner::ResolveBoot
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

HRESULT DiskCommandRunner::ResolveBoot (const CommandLineOptions & options,
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
            (result.diagnostics    += ApplyPrefix (
                 "Error: %Lbootable and %Lboot are mutually exclusive\n"
                 "       %Lbootable copies an operating system onto the disk.\n"
                 "       %Lboot writes a binary that runs without one.\n"),
             result.exitStatus      = DiskCommandResult::kNoOutput,
             result.badCommandLine  = true));

    //  Not asked for at all is the ordinary case, not a failure.
    BAIL_OUT_IF (!options.disk.bootable, S_OK);

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
                                            + ApplyPrefix ("       Select Bootable in Casso Explorer's New disk image to download it,\n"
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
                                        + ApplyPrefix ("       Open Casso and use the disk picker to download "
                                                       "it, or supply a\n"
                                                       "       master image with %Lbootable <image>.\n"),
                 result.exitStatus      = DiskCommandResult::kNoOutput,
                 result.badCommandLine  = true));

        master = StockBootDisks::GetPath (which);
    }

    hr = m_fileIo.ReadAllBytes (master, osBytes);
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
//  DiskCommandRunner::LoadSystemSource
//
//  The image a bootable Apple Pascal or CP/M disk is copied from, in any
//  container, checked for what the copy needs before anything is built.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::LoadSystemSource (const std::string   & path,
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



    hr = m_fileIo.ReadAllBytes (path, fileBytes);
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
//  DiskCommandRunner::DescribeNewDisk
//
//  What was just written, in the words the flags used to ask for it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeNewDisk (const BlankDiskSpec & spec)
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

    text += spec.bootable ? ", bootable" : ", not bootable";

    if (spec.fluxTrackMask != 0)
    {
        vector<int>  quarterTracks;
        wstring      list;
        int          track = 0;

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
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
//  DiskCommandRunner::BuildAndWrite
//
//  Everything create and init share: settle the spec, build the bytes, put
//  them where they go.
//
//  ALL OR NOTHING, through the same commit path every other write uses. A
//  build that fails leaves the target exactly as it was, which for `init`
//  means the disk somebody was reformatting is still the disk they had.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::BuildAndWrite (const CommandLineOptions & options,
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

    hr = ResolveContents (options, spec.contents, result);
    CHR (hr);

    hr = ResolveVolume (options, spec, result);
    CHR (hr);

    now             = m_clock();
    spec.pascalDate = PascalVolume::MakeDate (now.wYear, now.wMonth, now.wDay);

    hr = ResolveBoot (options, spec, payload, result);
    CHR (hr);

    hr = ResolveFlux (options, overExisting, spec.fluxTrackMask, result);
    CHR (hr);

    //  THE PAIRING RULES ARE THE BUILDER'S, AND ITS VERDICT IS ASKED FOR
    //  RATHER THAN ITS HRESULT. ValidateSpec answers in E_INVALIDARG, which
    //  asserts, and that was right while the create dialog was the only
    //  caller: its dropdowns cannot express an illegal combination. This
    //  command line can, in one word, so every rule the builder holds is
    //  now reachable by typing and none of them is a caller's bug.
    refusal      = DescribeSpecRefusal (spec);
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
        hr = m_fileIo.Stat (options.disk.imagePath, target.stamp);
        target.stampRecorded = SUCCEEDED (hr);
    }

    hr = m_session.CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += options.disk.imagePath + ": " + DescribeNewDisk (spec) + "\n";
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::AppendTypeExtension
//
//  Gives a bare image name the extension --type asked for.
//
//  Only a name with no extension gains one. A name that already has an
//  extension keeps it, and a trailing dot is how a caller says they want no
//  extension: std::filesystem reports that dot as the extension. The path is
//  built from the narrow string the same way Win32DiskFileIo builds it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::AppendTypeExtension (std::string & imagePath, const std::string & containerType)
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
//  DiskCommandRunner::RunCreate
//
//  A new image file, of a container this tool decides here.
//
//  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
//  keystroke from a disk they no longer have, and `create` is the command they
//  reach for when they are not thinking about what is already there. The
//  refusal names `init`, which is the command for meaning it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunCreate (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT             hr              = S_OK;
    DiskFormat          format          = DiskFormat::Dsk;
    size_t              nibbleTrackSize = 0;
    bool                named           = !options.disk.imagePath.empty();
    bool                alreadyThere    = false;
    bool                isLarge         = false;
    CommandLineOptions  resolved        = options;



    CBRF (named, ReportMissingParameter ("<image>", result));

    AppendTypeExtension (resolved.disk.imagePath, resolved.disk.containerType);

    alreadyThere = m_fileIo.Exists (resolved.disk.imagePath);
    CBRF (!alreadyThere, result.Fail (resolved.disk.imagePath, "",
                                      "is already there, and create will not write over it. "
                                      "Use init to reformat it, or choose another name"));

    isLarge = IsLargeCreate (resolved);

    if (isLarge)
    {
        BuildLargeProDos (resolved, result);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = ResolveContainer (resolved, nibbleTrackSize, format, result);
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
//  DiskCommandRunner::IsLargeCreate
//
//  A create of a ProDOS volume larger than a floppy: a size past 140K, or a
//  container only a block volume has.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::IsLargeCreate (const CommandLineOptions & options)
{
    std::string  size    = options.disk.sizeName;
    bool         isSized = !size.empty() && size != "140k" && size != "140K";



    return isSized || IsBlockOnlyExtension (options.disk.imagePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::IsBlockOnlyExtension
//
//  An extension only a block volume is written with: hard disk images, 2IMG
//  and Disk Copy 4.2.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::IsBlockOnlyExtension (const std::string & imagePath)
{
    ImageContainerKind  kind = ImageContainer::GetKind (imagePath, 0);



    return kind == ImageContainerKind::Blocks || kind == ImageContainerKind::TwoImg || kind == ImageContainerKind::DiskCopy42;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::IsProDosOrderExtension
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::IsProDosOrderExtension (const std::string & imagePath)
{
    std::string  extension = std::filesystem::path (imagePath).extension().string();



    std::transform (extension.begin(), extension.end(), extension.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return extension == ".po";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::TryGetLargeBlockCount
//
//  The block count of a size word. 32M is the 65,535 blocks a ProDOS volume
//  addresses.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::TryGetLargeBlockCount (const std::string & sizeName, uint32_t & outBlocks)
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
//  DiskCommandRunner::BuildLargeProDos
//
//  A formatted ProDOS volume of the size asked for, in the container the
//  image's extension selects: raw blocks for `.po` and `.hdv`, 2IMG in ProDOS
//  order, or Disk Copy 4.2, which holds 800K disks only.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::BuildLargeProDos (const CommandLineOptions & options, DiskCommandResult & result)
{
    constexpr uint32_t  k800KBlocks = 1600;



    HRESULT                        hr         = S_OK;
    ImageContainerKind             kind       = ImageContainer::GetKind (options.disk.imagePath, 0);
    uint32_t                       blocks     = 0;
    bool                           sizeKnown  = TryGetLargeBlockCount (options.disk.sizeName, blocks);
    bool                           isProDos   = options.disk.formatName.empty() || options.disk.formatName == "prodos";
    std::string                    name       = options.disk.volumeName.empty() ? std::string ("NEWDISK") : options.disk.volumeName;
    bool                           nameOk     = ProDosVolume::IsValidName (name);
    bool                           container  = false;
    bool                           fitsDc42   = false;
    vector<Byte>                   volume;
    vector<Byte>                   imageBytes;
    TwoImgHeader                   twoImg;
    DiskCopy42Header               diskCopy;
    DiskImageSession::OpenedImage  target;



    if (kind == ImageContainerKind::None)
    {
        kind = ImageContainerKind::Blocks;
    }

    container = IsBlockOnlyExtension (options.disk.imagePath) || IsProDosOrderExtension (options.disk.imagePath);
    fitsDc42  = kind != ImageContainerKind::DiskCopy42 || blocks == k800KBlocks;

    CBRF (sizeKnown, (result.diagnostics   += ApplyPrefix ("Error: illegal %Lsize value\n"
                                                           "       The sizes are 140K, 800K, 5M, 10M, 20M and 32M.\n"),
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));
    CBRF (isProDos,  (result.diagnostics   += ApplyPrefix ("Error: illegal %Lformat for a large disk\n"
                                                           "       Disks larger than 140K are ProDOS volumes.\n"),
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
    CBRF (nameOk,    (result.diagnostics   += "Error: illegal volume name\n"
                                              "       ProDOS volume names are 1-15 characters, starting with a letter, and can include letters, digits, and periods.\n",
                      result.exitStatus     = DiskCommandResult::kNoOutput,
                      result.badCommandLine = true));

    volume.assign ((size_t) blocks * (size_t) ProDosSkeleton::kBlockByteSize, 0);

    hr = ProDosSkeleton::Write (volume, name);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

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

    hr = m_session.CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += std::format ("{}: ProDOS volume {}, {} blocks\n", options.disk.imagePath, name, blocks);
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReformatLargeProDos
//
//  A new ProDOS volume over an existing block image, at the size the image
//  has. The container's header is kept, as any write through the volume
//  layer keeps it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReformatLargeProDos (const CommandLineOptions & options, const FileStamp & stamp, DiskCommandResult & result)
{
    HRESULT                        hr       = S_OK;
    bool                           isProDos = options.disk.formatName.empty() || options.disk.formatName == "prodos";
    std::string                    name     = options.disk.volumeName.empty() ? std::string ("NEWDISK") : options.disk.volumeName;
    bool                           nameOk   = ProDosVolume::IsValidName (name);
    vector<Byte>                   original;
    vector<Byte>                   sectors;
    vector<Byte>                   imageBytes;
    SectorDecodeReport             report;
    std::string                    refusal;
    DiskImageSession::OpenedImage  target;



    CBRF (isProDos, (result.diagnostics   += ApplyPrefix ("Error: illegal %Lformat for a large disk\n"
                                                          "       Disks larger than 140K are ProDOS volumes.\n"),
                     result.exitStatus     = DiskCommandResult::kNoOutput,
                     result.badCommandLine = true));
    CBRF (nameOk,   (result.diagnostics   += "Error: illegal volume name\n"
                                             "       ProDOS volume names are 1-15 characters, starting with a letter, and can include letters, digits, and periods.\n",
                     result.exitStatus     = DiskCommandResult::kNoOutput,
                     result.badCommandLine = true));

    hr = m_fileIo.ReadAllBytes (options.disk.imagePath, original);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be read"));

    hr = VolumeImage::Load (original, options.disk.imagePath, sectors, report);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "is not an image this tool can read"));

    hr = ProDosSkeleton::Write (sectors, name);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be built"));

    hr = VolumeImage::Save (original, options.disk.imagePath, sectors, imageBytes, refusal);
    CHRF (hr, result.Fail (options.disk.imagePath, "", refusal.empty() ? std::string ("could not be written") : refusal));

    target.imagePath     = options.disk.imagePath;
    target.stamp         = stamp;
    target.stampRecorded = true;
    target.isNew         = false;

    hr = m_session.CommitImage (target, imageBytes, result);
    CHR (hr);

    result.output     += std::format ("{}: ProDOS volume {}, {} blocks\n", options.disk.imagePath, name, ProDosSkeleton::GetBlockCount (sectors.size()));
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildDirectBoot
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

void DiskCommandRunner::BuildDirectBoot (const CommandLineOptions & options,
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
          (result.diagnostics    += ApplyPrefix (
               "Error: %Lbootable and %Lboot are mutually exclusive\n"
               "       %Lbootable copies an operating system onto the disk.\n"
               "       %Lboot writes a binary that runs without one.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    //  A filesystem was asked for AND a disk with none. Refused rather than
    //  quietly dropping one of them.
    formatAgrees = options.disk.formatName.empty() || options.disk.formatName == "none";
    CBRF (formatAgrees,
          (result.diagnostics    += ApplyPrefix ("Error: %Lboot and %Lformat are mutually exclusive\n"
                                                 "       %Lboot writes no filesystem. A direct-boot "
                                                 "disk contains the\n"
                                                 "       binary and nothing else.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = m_fileIo.ReadAllBytes (options.disk.directBootFile, payload);
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

    hr = ResolveFlux (options, false, fluxSpec.fluxTrackMask, result);
    CHR (hr);

    //  A direct-boot disk has no filesystem, so of the builder's rules only
    //  the container's hold on flux applies.
    fluxSpec.format   = format;
    fluxSpec.contents = BlankDiskContents::Unformatted;
    refusal           = DescribeSpecRefusal (fluxSpec);
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

    hr = m_session.CommitImage (target, imageBytes, result);
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
//  DiskCommandRunner::RunInit
//
//  A disk that is already there, formatted again.
//
//  THE CONTAINER IS NOT A CHOICE HERE. It was decided when the file was made,
//  and this command rewrites what is INSIDE it -- so `init` takes no --type, and
//  a reader who wants a different container wants a different file, which is
//  what `create` makes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunInit (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT     hr              = S_OK;
    DiskFormat  format          = DiskFormat::Dsk;
    size_t      nibbleTrackSize = 0;
    FileStamp   stamp;
    bool        named           = !options.disk.imagePath.empty();
    bool        exists          = false;
    bool        untyped         = options.disk.containerType.empty();



    CBRF (named, ReportMissingParameter ("<image>", result));

    exists = m_fileIo.Exists (options.disk.imagePath);
    CBRF (exists, result.Fail (options.disk.imagePath, "", "is not there. Use create to make one"));

    CBRF (untyped,
          (result.diagnostics    += ApplyPrefix (
               "Error: init does not accept %Ltype\n"
               "       The image already has a container type. Use create to make an\n"
               "       image with a different type.\n"),
           result.exitStatus      = DiskCommandResult::kNoOutput,
           result.badCommandLine  = true));

    hr = m_fileIo.Stat (options.disk.imagePath, stamp);
    CHRF (hr, result.Fail (options.disk.imagePath, "", "could not be measured"));

    if (ImageContainer::GetKind (options.disk.imagePath, (size_t) stamp.sizeBytes) != ImageContainerKind::None)
    {
        ReformatLargeProDos (options, stamp, result);

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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ParseTrackList
//
//  "0-2,17" as a mask of whole tracks: numbers from 0 to 34 separated by
//  commas, a range being two of them joined by a hyphen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ParseTrackList (const std::string & text, uint64_t & outMask)
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

        while (pos < text.size() && isdigit ((unsigned char) text[pos]) != 0 && first < NibblizationLayer::kTrackCount)
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

            while (pos < text.size() && isdigit ((unsigned char) text[pos]) != 0 && last < NibblizationLayer::kTrackCount)
            {
                last = last * 10 + (text[pos] - '0');
                pos++;
            }
        }

        ok = first <= last && last < NibblizationLayer::kTrackCount;
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
//  DiskCommandRunner::ResolveFlux
//
//  Which whole tracks the new disk stores as flux. A bare --flux is every
//  track. Reformatting without --flux keeps the tracks the WOZ already
//  stores as flux, so init does not quietly turn a flux disk into a bit one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveFlux (const CommandLineOptions & options,
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
        CHRF (hr, RefuseBadValue (result,
                  "Error: illegal track list\n"
                  "       Tracks are whole numbers from 0 to 34, separated by commas. A\n"
                  "       range is two of them joined by a hyphen, such as 0-2,17.\n"));
    }
    else if (overExisting)
    {
        hr = DiskImageStore::GetSourceFormatByExtension (options.disk.imagePath, format);
        BAIL_OUT_IF (FAILED (hr) || !BlankDiskBuilder::CanHoldFlux (format), S_OK);

        hr = m_fileIo.ReadAllBytes (options.disk.imagePath, existing);
        BAIL_OUT_IF (FAILED (hr), S_OK);

        hr = WozLoader::Load (existing, image);
        BAIL_OUT_IF (FAILED (hr), S_OK);

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorRead
//
//  THE OTHER HALF OF sectorwrite, and the reason it is worth having: get goes
//  through a catalog, so on a disk with no filesystem there was no way at all
//  to read back what had just been written. The listing said as much -- "its
//  files are in a layout this tool does not read" -- which was true and is no
//  longer.
//
//  A count is a parameter here and not on the write because a write knows its
//  own length and a read cannot: what usually records where a file ends is the
//  catalog, and these are the disks that have none.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RefuseBadValue (DiskCommandResult & result, const char * summary)
{
    result.diagnostics    += summary;
    result.exitStatus      = DiskCommandResult::kNoOutput;
    result.badCommandLine  = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorRead
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunSectorRead (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         first        = 0;
    size_t                         total        = 0;
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    //  WHICH NUMBERING, OR NOTHING. The same sixteen sectors answer to two
    //  orders, and a command line that does not say which is how bytes once
    //  landed on the wrong sector -- so there is no default to fall back to.
    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, RefuseBadValue (result,
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibblizationLayer::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibblizationLayer::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibblizationLayer::kTrackCount - 1,
                  NibblizationLayer::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal sector count: %d\n"
                  "       The sector count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, RefuseBadValue (result, summary));

    first = (size_t) (options.disk.track * NibblizationLayer::kSectorsPerTrack
                    + options.disk.sector);
    total = (size_t) (NibblizationLayer::kTrackCount * NibblizationLayer::kSectorsPerTrack);
    fits  = first + (size_t) options.disk.count <= total;

    if (!fits)
    {
        size_t  spare = total - first;

        snprintf (summary, sizeof (summary),
                  "Error: not enough sectors available\n"
                  "       Requested %d %s starting at track %d sector %d,\n"
                  "       but only %zu %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"),
                  options.disk.track, options.disk.sector,
                  spare,
                  Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    //  No filesystem needed, and none looked for. That is the whole point of
    //  the command: a disk built by sectorwrite may have no catalog at all.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    //  Each position maps through the STATED numbering, one at a time. Under
    //  --logical that is the identity into the DOS-ordered buffer; under
    //  --physical it is the interleave, so consecutive positions deliver the
    //  sectors in the order the drive presents them under consecutive
    //  address marks.
    for (int index = 0; index < options.disk.count; index++)
    {
        size_t  at = GetSectorRecordOffset (options.disk.numbering, first + (size_t) index);

        if (at + (size_t) NibblizationLayer::kSectorByteSize > opened.sectors.size())
        {
            break;
        }

        payload.insert (payload.end(),
                        opened.sectors.begin() + (ptrdiff_t) at,
                        opened.sectors.begin() + (ptrdiff_t) (at + NibblizationLayer::kSectorByteSize));
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from track %d %s sector %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.track, how, options.disk.sector,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "sector", "sectors"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, and it matters more here than
    //  anywhere: these bytes have no catalog and no length behind them, so a
    //  sector delivered as zeros looks exactly like a sector that holds zeros.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunSectorWrite
//
//  A file from the host laid into an image at a track and a sector, in
//  whichever numbering the command line declared.
//
//  NO FILESYSTEM IS INVOLVED, WHICH IS THE POINT. A demo that boots its own
//  loader and reads fixed tracks has no catalog to make an entry in and no
//  allocator to ask for space, so `put` cannot express it at all. This writes
//  the bytes given, where it is told, and nothing else.
//
//  THE NUMBERING IS DECLARED, NEVER DEFAULTED. The same sixteen sectors
//  answer to two orders: --logical is what catalogs, RWTS callers and every
//  DOS-era sector editor speak, the identity into a DOS-ordered image; and
//  --physical is the address-field order a boot loader asking the drive ROM
//  sees, the interleave. This command once applied the interleave silently
//  under the belief the image was physical-ordered -- `--sector 1` landed on
//  logical sector 7, and sectorread's matching belief read it back perfectly
//  -- which is why an unstated numbering is refused rather than guessed. The
//  skew case in DirectBootTests is what settled the orientation against
//  DOS's own table at $084D.
//
//  It runs on past the end of a track into the next one, because a payload
//  longer than 4 KB is ordinary and splitting the call per track would put the
//  wrap arithmetic back in the caller's hands. Under --physical the run-on
//  advances by address mark, so page N of the payload sits under mark N --
//  exactly what a loader that files sectors by address mark wants.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunSectorWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           stated       = false;
    bool                           onDisk       = false;
    bool                           fits         = false;
    const char                   * how          = "";
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, ReportMissingParameter ("<file>", result));

    stated = options.disk.numbering != CommandLineOptions::DiskOptions::Numbering::Unstated;

    CBRFEx (stated, E_INVALIDARG, RefuseBadValue (result,
            "Error: --logical or --physical is required\n"
            "       --logical is the numbering used by catalogs, DOS tools, and\n"
            "       reference books. --physical is the address-field order recorded\n"
            "       on the track.\n"));

    how = options.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical
              ? "physical" : "logical";

    onDisk = options.disk.track >= 0 && options.disk.track < NibblizationLayer::kTrackCount
          && options.disk.sector >= 0 && options.disk.sector < NibblizationLayer::kSectorsPerTrack;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: track %d sector %d is out of range\n"
                  "       Tracks are 0-%d and sectors are 0-%d.\n",
                  options.disk.track, options.disk.sector,
                  NibblizationLayer::kTrackCount - 1,
                  NibblizationLayer::kSectorsPerTrack - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A disk with no filesystem is the ordinary case here rather than a
    //  refusal: a demo that boots its own loader has no catalog, and that
    //  is exactly the disk this command exists to write.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    edited = opened.sectors;

    //  Whole sectors, because that is the unit the drive reads. A payload that
    //  does not fill its last one is padded with what was already there rather
    //  than with a value this command invented.
    needed = (payload.size() + (size_t) NibblizationLayer::kSectorByteSize - 1)
           / (size_t) NibblizationLayer::kSectorByteSize;

    {
        size_t  first = (size_t) (options.disk.track * NibblizationLayer::kSectorsPerTrack
                                + options.disk.sector);
        size_t  total = (size_t) (NibblizationLayer::kTrackCount * NibblizationLayer::kSectorsPerTrack);

        fits = first + needed <= total;

        if (!fits)
        {
            size_t       spare  = total - first;
            snprintf (summary, sizeof (summary),
                      "Error: not enough sectors available\n"
                      "       Writing %zu bytes starting at track %d sector %d requires\n"
                      "       %zu %s, but only %zu %s %s on the disk.\n",
                      payload.size(), options.disk.track, options.disk.sector,
                      needed,
                      Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"),
                      spare,
                      Utils::GetSingularOrPluralForm ((long long) spare, "sector", "sectors"),
                      (spare == 1) ? "remains" : "remain");
        }

        CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

        for (size_t index = 0; index < needed; index++)
        {
            size_t  at    = GetSectorRecordOffset (options.disk.numbering, first + index);
            size_t  from  = index * (size_t) NibblizationLayer::kSectorByteSize;
            size_t  count = std::min ((size_t) NibblizationLayer::kSectorByteSize,
                                      payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at track %d %s sector %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.track, how, options.disk.sector,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "sector", "sectors"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::GetSectorRecordOffset
//
//  Positions advance linearly -- sector, then track -- and each one maps
//  through the stated numbering on its own. Under --logical that is the
//  identity into the DOS-ordered buffer; under --physical it is the
//  interleave, answered by the layer that owns the skew rather than by a
//  second copy of the sixteen numbers here.
//
////////////////////////////////////////////////////////////////////////////////

size_t DiskCommandRunner::GetSectorRecordOffset (
    CommandLineOptions::DiskOptions::Numbering  numbering,
    size_t                                      running)
{
    size_t  track    = running / (size_t) NibblizationLayer::kSectorsPerTrack;
    size_t  position = running % (size_t) NibblizationLayer::kSectorsPerTrack;
    bool    physical = numbering == CommandLineOptions::DiskOptions::Numbering::Physical;
    size_t  record   = running;



    if (physical)
    {
        record = track * (size_t) NibblizationLayer::kSectorsPerTrack
               + (size_t) NibblizationLayer::GetDosFileIndexForPhysicalSector ((int) position);
    }

    return record * (size_t) NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBlockRead
//
//  The 512-byte view: a ProDOS block is two sector records spread across its
//  track by the ProDOS interleave, and the map is ProDosSkeleton's -- the
//  same single copy the ProDOS reader, writer and builder address blocks
//  through.
//
//  A BLOCK NUMBER NEEDS NO --logical OR --physical, because blocks have only
//  one order: the block map is defined over the volume, so the question the
//  sector commands must ask has one answer here.
//
//  And it works over any container, not only .po. The session normalizes
//  every image into the same DOS-ordered buffer, so a block is a lens over
//  that buffer -- which is how a ProDOS volume shipped inside a .dsk reads
//  naturally.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBlockRead (const CommandLineOptions & options,
                                      DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes = (size_t) NibblizationLayer::kSectorByteSize;



    HRESULT                        hr           = S_OK;
    bool                           onDisk       = false;
    bool                           isARead      = false;
    bool                           fits         = false;
    int                            blocks       = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    char                           summary[192] = {};



    //  No filesystem needed, and none looked for: a block is geometry, not
    //  a volume, and blocks 0-1 of a disk with no filesystem are still worth
    //  looking at.
    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    blocks = ProDosSkeleton::GetBlockCount (opened.sectors.size());
    onDisk = options.disk.block >= 0 && options.disk.block < blocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, blocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    isARead = options.disk.count >= 1;

    if (!isARead)
    {
        snprintf (summary, sizeof (summary),
                  "Error: illegal block count: %d\n"
                  "       The block count must be 1 or greater.\n",
                  options.disk.count);
    }

    CBRFEx (isARead, E_INVALIDARG, RefuseBadValue (result, summary));

    fits = options.disk.block + options.disk.count <= blocks;

    if (!fits)
    {
        int  spare = blocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Requested %d %s starting at block %d,\n"
                  "       but only %d %s %s on the disk.\n",
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"),
                  options.disk.block,
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    for (int index = 0; index < options.disk.count; index++)
    {
        int  block = options.disk.block + index;

        for (int half = 0; half < 2; half++)
        {
            size_t  at = ProDosSkeleton::GetBlockByteOffset (opened.sectors.size(), block, (size_t) half * kHalfBytes);

            if (at + kHalfBytes > opened.sectors.size())
            {
                break;
            }

            payload.insert (payload.end(),
                            opened.sectors.begin() + (ptrdiff_t) at,
                            opened.sectors.begin() + (ptrdiff_t) (at + kHalfBytes));
        }
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload);
        CHRF (hr, result.Fail (options.disk.hostFile, "", "could not be written"));

        snprintf (summary, sizeof (summary),
                  "%s: %zu %s from block %d, %d %s\n",
                  options.disk.hostFile.c_str(),
                  payload.size(),
                  Utils::GetSingularOrPluralForm ((long long) payload.size(), "byte", "bytes"),
                  options.disk.block,
                  options.disk.count,
                  Utils::GetSingularOrPluralForm (options.disk.count, "block", "blocks"));

        result.output += summary;
    }
    else
    {
        result.payload    = payload;
        result.hasPayload = true;
    }

    result.exitStatus = DiskCommandResult::kClean;

    //  DAMAGE IS REPORTED RATHER THAN HIDDEN, exactly as the sector read
    //  reports it: a block delivered as zeros looks like a block that holds
    //  zeros, and nothing behind these bytes records the difference.
    if (opened.report.HasDataLoss())
    {
        int  lost = opened.report.GetUnrecoveredCount();

        snprintf (summary, sizeof (summary),
                  "%d %s could not be decoded. Any of them in this range "
                  "were delivered as zeros",
                  lost, Utils::GetSingularOrPluralForm (lost, "sector", "sectors"));

        result.diagnostics += DiskCommandResult::FormatFailure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = DiskCommandResult::kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunBlockWrite
//
//  The write half of the 512-byte view, through the same single block map.
//
//  Whole blocks, the way the sector write works in whole sectors: a payload
//  that does not fill its last block leaves the rest of that block as it
//  was, rather than padded with a value this command invented. It runs on
//  into later blocks for the same reason the sector write runs on into
//  later tracks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunBlockWrite (const CommandLineOptions & options,
                                       DiskCommandResult        & result)
{
    constexpr size_t  kHalfBytes  = (size_t) NibblizationLayer::kSectorByteSize;
    constexpr size_t  kBlockBytes = (size_t) ProDosSkeleton::kBlockByteSize;



    HRESULT                        hr           = S_OK;
    size_t                         needed       = 0;
    size_t                         written      = 0;
    bool                           named        = !options.disk.hostFile.empty();
    bool                           onDisk       = false;
    bool                           fits         = false;
    int                            blocks       = 0;
    DiskImageSession::OpenedImage  opened;
    vector<Byte>                   payload;
    vector<Byte>                   edited;
    char                           summary[160] = {};



    CBRF (named, ReportMissingParameter ("<file>", result));

    hr = m_session.OpenImage (options.disk.imagePath, opened, result, false);
    CHR (hr);

    blocks = ProDosSkeleton::GetBlockCount (opened.sectors.size());
    onDisk = options.disk.block >= 0 && options.disk.block < blocks;

    if (!onDisk)
    {
        snprintf (summary, sizeof (summary),
                  "Error: block %d is out of range\n"
                  "       Blocks are 0-%d.\n",
                  options.disk.block, blocks - 1);
    }

    CBRFEx (onDisk, E_INVALIDARG, RefuseBadValue (result, summary));

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, payload);
    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    edited = opened.sectors;

    needed = (payload.size() + kBlockBytes - 1) / kBlockBytes;

    fits = (size_t) options.disk.block + needed <= (size_t) blocks;

    if (!fits)
    {
        int  spare = blocks - options.disk.block;

        snprintf (summary, sizeof (summary),
                  "Error: not enough blocks available\n"
                  "       Writing %zu bytes starting at block %d requires\n"
                  "       %zu %s, but only %d %s %s on the disk.\n",
                  payload.size(), options.disk.block,
                  needed,
                  Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"),
                  spare,
                  Utils::GetSingularOrPluralForm (spare, "block", "blocks"),
                  (spare == 1) ? "remains" : "remain");
    }

    CBRFEx (fits, E_INVALIDARG, RefuseBadValue (result, summary));

    for (size_t index = 0; index < needed; index++)
    {
        int  block = options.disk.block + (int) index;

        for (int half = 0; half < 2; half++)
        {
            size_t  from  = index * kBlockBytes + (size_t) half * kHalfBytes;
            size_t  at    = ProDosSkeleton::GetBlockByteOffset (edited.size(), block, (size_t) half * kHalfBytes);
            size_t  count = 0;

            if (from >= payload.size())
            {
                break;
            }

            count = std::min (kHalfBytes, payload.size() - from);

            if (at + count > edited.size())
            {
                break;
            }

            std::copy (payload.begin() + (ptrdiff_t) from,
                       payload.begin() + (ptrdiff_t) (from + count),
                       edited.begin()  + (ptrdiff_t) at);

            written += count;
        }
    }

    hr = m_session.SaveAndCommit (opened, edited, result);
    CHR (hr);

    snprintf (summary, sizeof (summary),
              "%s: %zu %s at block %d, %zu %s\n",
              options.disk.imagePath.c_str(),
              written,
              Utils::GetSingularOrPluralForm ((long long) written, "byte", "bytes"),
              options.disk.block,
              needed,
              Utils::GetSingularOrPluralForm ((long long) needed, "block", "blocks"));

    result.output     += summary;
    result.exitStatus  = DiskCommandResult::kClean;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::WritesTheImage
//
//  Whether a command puts bytes into the image it was given.
//
//  LISTED RATHER THAN INFERRED. "Did the file change" is not knowable from here
//  without stat-ing it again, and a command that legitimately wrote the same
//  bytes back would answer no.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::WritesTheImage (CommandLineOptions::DiskOptions::Command command)
{
    using Command = CommandLineOptions::DiskOptions::Command;

    return command == Command::Put
        || command == Command::Mkdir
        || command == Command::Rmdir
        || command == Command::Delete
        || command == Command::Boot
        || command == Command::Create
        || command == Command::Init
        || command == Command::SectorWrite
        || command == Command::BlockWrite
        || command == Command::Krunch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::AnnounceIntent
//
//  Tells any running emulator what this write was meant to do.
//
//  IT CANNOT FAIL THE RUN, and that is the contract rather than an oversight.
//  The channel returns nothing, an emulator that misses the hint falls back to
//  asking, and a build that failed over an undelivered courtesy would be worse
//  than the problem this solves.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::AnnounceIntent (const CommandLineOptions & options,
                                        const DiskCommandResult  & result)
{
    bool  wrote  = WritesTheImage (options.disk.command);
    bool  clean  = (result.exitStatus == DiskCommandResult::kClean);
    bool  stated = (options.disk.changeIntent != ExternalChangeIntent::Unstated);



    if (m_intentChannel != nullptr && wrote && clean && stated
        && !options.disk.imagePath.empty())
    {
        m_intentChannel->StateIntent (options.disk.imagePath, options.disk.changeIntent);
    }

    return;
}
