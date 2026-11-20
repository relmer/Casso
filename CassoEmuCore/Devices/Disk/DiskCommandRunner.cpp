#include "Pch.h"

#include "DiskCommandRunner.h"
#include "DiskCommandText.h"
#include "DiskFilePayload.h"
#include "Seams/IIntentChannel.h"
#include "AppleTextCodec.h"
#include "CommandLineParser.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/ProDosTypeNames.h"
#include "Devices/Disk/BlockBuffer.h"
#include "Devices/Disk/Partitions/CardVolumeEdit.h"
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
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "FileTypes/FileFormatTable.h"
#include "IntegerBasicDetokenizer.h"
#include "IntegerBasicTokenizer.h"
#include "Utils.h"
#include "Devices/Disk/BlockVolumeOpener.h"
#include "Devices/Disk/DiskCollectionCommands.h"
#include "Devices/Disk/DiskInspectCommand.h"
#include "Devices/Disk/DiskRepairCommand.h"
#include "Devices/Disk/DiskThumbnailCommand.h"
#include "DiskCheck/ThumbnailStore.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/ProDosStartup.h"





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
                                       || command == Command::BlockWrite || command == Command::GetThumbnail;
    bool                      wantsPic  = command == Command::SetThumbnail;
    bool                      wantsName = command == Command::Get || command == Command::Delete
                                       || command == Command::Boot;
    bool                      wantsPath = command == Command::Mkdir || command == Command::Rmdir;
    bool                      wantsText = command == Command::Search;
    bool                      onFolder  = command == Command::Dupes || command == Command::Search;



    //  Help asks for nothing, and a command the table does not know is
    //  answered as an unknown command rather than as a missing operand.
    HRESULT  hr    = S_OK;   // vestigial, for the bail
    bool     asked = command != Command::None && command != Command::Help;



    BAIL_OUT_IF (!asked, S_OK);

    if (options.disk.imagePath.empty())
    {
        missing.push_back (onFolder ? "<folder>" : "<image>");
    }

    if (wantsText && options.disk.path.empty())
    {
        missing.push_back ("<text>");
    }

    if (wantsName && options.disk.path.empty())
    {
        missing.push_back ("<name>");
    }

    if (wantsPath && options.disk.path.empty())
    {
        missing.push_back ("<path>");
    }

    if (wantsPic && options.disk.picturePath.empty())
    {
        missing.push_back ("<picture>");
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

std::string DiskCommandRunner::FormatProDosEntry (const FileEntry & entry, size_t nameWidth)
{
    char         detail[96] = {};
    std::string  text;



    text = std::string (entry.isLocked ? "*" : " ") + entry.name;

    while (text.size() < nameWidth + 1)
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
    size_t                         nameWidth    = kNameWidth;
    std::string                    note;



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
        hr = m_fileCommands.ResolveVolumePath (options, opened.kind, opened.sectors, options.disk.path, directory, result);
        CHR (hr);

        if (options.disk.recurse)
        {
            hr = MeasureNameWidth (*volume, directory, nameWidth);
        }

        if (SUCCEEDED (hr))
        {
            hr = AppendDirectoryRows (*volume, opened.kind, directory, options.disk.recurse, nameWidth, result);
        }

        CHRF (hr, result.FailAction (options.disk.imagePath, "list " + options.disk.path,
                              ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, opened.kind))));

        if (opened.kind == VolumeKind::Tape)
        {
            snprintf (summary, sizeof (summary), "\n%u files on the tape\n", (unsigned) listing.totalUnits);
        }
        else
        {
            snprintf (summary, sizeof (summary), "\n%u %s free of %u\n",
                      (unsigned) listing.freeUnits,
                      (opened.kind == VolumeKind::Dos33 || opened.kind == VolumeKind::Gutenberg || opened.kind == VolumeKind::Rdos || opened.kind == VolumeKind::Infocom) ? "sectors" : "blocks",
                      (unsigned) listing.totalUnits);
        }

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
        else if (opened.kind == VolumeKind::ProDos)
        {
            note = ProDosStartup::DescribeListingNote (listing.startup, HasProDosSystemFile (listing));

            if (!note.empty())
            {
                result.output += "\n" + note + "\n";
            }
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
                                 ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, opened.kind))));

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
    size_t               nameWidth,
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

        result.output += (kind == VolumeKind::Dos33) ? FormatDos33Entry (shown) : FormatProDosEntry (shown, nameWidth);

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
                hr = AppendDirectoryRows (volume, kind, below, true, nameWidth, result);
                CHR (hr);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::MeasureNameWidth
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::MeasureNameWidth (const IVolume & volume, const std::string & directory, size_t & inOutWidth)
{
    HRESULT        hr = S_OK;
    VolumeListing  listing;



    hr = volume.EnumerateDirectory (FilePath::Parse (directory), listing);
    CHR (hr);

    for (const FileEntry & entry : listing.entries)
    {
        std::string  shown = directory.empty() ? entry.name : directory + "/" + entry.name;

        //  The two spaces after a 15-character ProDOS name in a flat listing.
        inOutWidth = std::max (inOutWidth, shown.size() + 2);

        if (entry.isDirectory)
        {
            hr = MeasureNameWidth (volume, shown, inOutWidth);
            CHR (hr);
        }
    }

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

    hr = m_fileCommands.ResolveEntryPath (options, opened.kind, opened.sectors, path, name, result);
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
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, opened.kind))));

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
//  DiskCommandRunner::RunThumbnail
//
//  Through the thumbnail store in the folder set, or the one under the
//  user's local application data with none set.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunThumbnail (const CommandLineOptions & options, DiskCommandResult & result)
{
    ThumbnailStore  store (m_thumbnailFolder.empty() ? ThumbnailStore::GetDefaultFolder() : m_thumbnailFolder);



    DiskThumbnailCommand::Run (options, store, result);
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
            if (m_fileIo.IsFolder (options.disk.imagePath))
            {
                DiskCollectionCommands::RunList (m_fileIo, options.disk.imagePath, result);
            }
            else
            {
                RunList (options, result);
            }

            break;

        case CommandLineOptions::DiskOptions::Command::Dupes:
        case CommandLineOptions::DiskOptions::Command::Search:
            DiskCollectionCommands::Run (m_fileIo, options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Inspect:
            DiskInspectCommand::Run (m_fileIo, options.disk.imagePath, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Repair:
            DiskRepairCommand::Run (m_fileIo, options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SetThumbnail:
        case CommandLineOptions::DiskOptions::Command::GetThumbnail:
        case CommandLineOptions::DiskOptions::Command::RemoveThumbnail:
            RunThumbnail (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Get:
            m_fileCommands.RunGet (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Put:
            m_fileCommands.RunPut (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Delete:
            m_fileCommands.RunDelete (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Boot:
            RunBoot (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Mkdir:
            m_fileCommands.RunMkdir (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Rmdir:
            m_fileCommands.RunRmdir (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockRead:
            m_sectorCommands.RunBlockRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockWrite:
            m_sectorCommands.RunBlockWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Krunch:
            m_fileCommands.RunKrunch (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Test:
            m_fileCommands.RunTest (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorRead:
            m_sectorCommands.RunSectorRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::SectorWrite:
            m_sectorCommands.RunSectorWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Create:
            m_createCommand.RunCreate (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Init:
            m_createCommand.RunInit (options, result);
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

