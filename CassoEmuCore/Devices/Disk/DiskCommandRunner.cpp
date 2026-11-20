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
        hr = ResolveVolumePath (options, opened.kind, opened.sectors, options.disk.path, directory, result);
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
    bool                           hasDirs  = false;
    bool                           madeAny  = false;
    size_t                         i        = 0;
    DiskImageSession::OpenedImage  opened;
    FilePath                       path;
    std::string                    typed;
    vector<Byte>                   staged;
    VolumeKind                     kind     = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardStaged;
    bool                           isCard   = false;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<path>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_fileIo, options.disk.imagePath);

    if (!isCard)
    {
        hr = m_session.OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hasDirs = HasDirectories (kind);

    CBRFEx (hasDirs, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (kind))));

    hr = ResolveVolumePath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), options.disk.path, typed, result);
    CHR (hr);

    path   = FilePath::Parse (typed);
    staged = opened.sectors;

    for (i = 1; i <= path.GetDepth(); i++)
    {
        std::unique_ptr<IVolume>       over    = !isCard ? VolumeImage::CreateVolume (kind, staged) : madeAny ? card.OpenEdited (cardStaged) : nullptr;
        const IVolume                & step    = over != nullptr ? *over : card.GetVolume();
        VolumeListing                  listing;
        std::string                    prefix;
        vector<Byte>                   next;
        BlockBuffer                    cardNext;
        HRESULT                        probe   = S_OK;
        size_t                         j       = 0;
        bool                           there   = false;

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

        hr = isCard ? step.CreateDirectory (FilePath::Parse (prefix), cardNext) : step.CreateDirectory (FilePath::Parse (prefix), next);

        CHRF (hr, result.FailAction (options.disk.imagePath, "create " + prefix,
                              ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

        staged     = next;
        cardStaged = cardNext;
        madeAny    = true;
    }

    CBRFEx (madeAny, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),
            result.FailAction (options.disk.imagePath, "create " + options.disk.path, "it is already on this volume"));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardStaged, result) : m_session.SaveAndCommit (opened, staged, result);
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::HasDirectories
//
//  The file systems mkdir and rmdir change: ProDOS and HFS. The others
//  have no directories, or are read-only.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCommandRunner::HasDirectories (VolumeKind kind)
{
    return kind == VolumeKind::ProDos || kind == VolumeKind::Hfs;
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
    bool                           hasDirs     = false;
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
    bool                           isCard      = false;
    VolumeKind                     kind        = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<path>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_fileIo, options.disk.imagePath);

    if (!isCard)
    {
        hr = m_session.OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hasDirs = HasDirectories (kind);

    CBRFEx (hasDirs, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                               std::format ("this volume is {}, which has no directories", VolumeImage::GetSystemName (kind))));

    hr = ResolveVolumePath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), options.disk.path, typed, result);
    CHR (hr);

    path = FilePath::Parse (typed);

    if (isCard)
    {
        hr = card.GetVolume().BuildRemovalPlan (path, plan);
    }
    else
    {
        hr = VolumeImage::CreateVolume (kind, opened.sectors)->BuildRemovalPlan (path, plan);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

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

    if (!confirmed && CanConfirm())
    {
        confirmed = Confirm ("Remove " + options.disk.path + " and everything below it?");
        declined  = "was not removed";
    }

    CBRFEx (confirmed, HRESULT_FROM_WIN32 (ERROR_CANCELLED),
            result.Fail (options.disk.imagePath, options.disk.path, ApplyPrefix (declined)));

    if (isCard)
    {
        hr = card.GetVolume().RemoveDirectory (path, options.disk.force, cardEdited, outcome);
    }
    else
    {
        hr = VolumeImage::CreateVolume (kind, opened.sectors)->RemoveDirectory (path, options.disk.force, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "remove " + options.disk.path,
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_session.SaveAndCommit (opened, edited, result);
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
    VolumeKind                     kind      = VolumeKind::Unknown;
    DiskImageSession::OpenedImage  opened;
    FilePayload                    payload;
    FilePath                       path;
    std::string                    name;
    char                           note[128] = {};
    std::unique_ptr<IBlockStore>   store;
    std::unique_ptr<IVolume>       blockVolume;
    CommandLineOptions             effective = options;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    DiskFilePayload::ApplyOutputEncoding (effective);

    //  A large ProDOS or HFS volume is read by the block, so a file is copied out
    //  without reading the volume whole. --index applies to DOS 3.3 only.
    if (!options.disk.hasIndex && BlockVolumeOpener::TryOpen (m_fileIo, options.disk.imagePath, m_session.GetChosenKind(), store, blockVolume, kind))
    {
        hr = ResolveEntryPath (options, kind, BlockImage (*store), path, name, result);
        CHR (hr);

        hr = blockVolume->Read (path, payload);
    }
    else
    {
        hr = m_session.OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);

        kind = opened.kind;

        hr = ResolveEntryPath (options, kind, opened.sectors, path, name, result);
        CHR (hr);

        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (opened.kind, opened.sectors);

        hr = volume->Read (path, payload);
    }

    result.failure = hr;

    CHRF (hr, result.FailAction (options.disk.imagePath, "read " + name,
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = DiskFilePayload::ApplyEncoding (effective, payload, result);
    CHR (hr);

    hr = DiskFilePayload::ApplyHostFileConversion (effective, kind, name, payload, result);
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
            CHRF (hr, result.Fail (DiskFilePayload::GetAppleDoublePath (options.disk.hostFile), "", "could not be written"));
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

    if (opened.kind != VolumeKind::Archive)
    {
        DiskRepairCommand::AddVerdict (m_fileIo, options.disk.imagePath, result);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ReadAppleDouble
//
//  Nothing is read from a missing or unreadable `._NAME`.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::ReadAppleDouble (const CommandLineOptions & options, FilePayload & inOutPayload)
{
    HRESULT          hr       = S_OK;
    vector<Byte>     bytes;
    AppleSingleFile  single;
    std::string      error;
    bool             isDouble = false;
    bool             typed    = !options.disk.typeName.empty() || options.disk.hasLoadAddress;



    hr = m_fileIo.ReadAllBytes (DiskFilePayload::GetAppleDoublePath (options.disk.hostFile), bytes);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    isDouble = AppleSingleCodec::IsAppleDouble (bytes);
    BAIL_OUT_IF (!isDouble, S_OK);

    hr = AppleSingleCodec::Decode (bytes, single, error);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    if (single.hasProDosInfo && !typed)
    {
        inOutPayload.type       = (Byte) single.fileType;
        inOutPayload.auxType    = (Word) single.auxType;
        inOutPayload.hasAuxType = true;
    }

    inOutPayload.resourceFork    = single.resourceFork;
    inOutPayload.finderInfo      = single.finderInfo;
    inOutPayload.hasResourceFork = single.hasResourceFork;

Error:
    return;
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

    hr = m_fileIo.WriteAllBytes (DiskFilePayload::GetAppleDoublePath (hostFile), bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::CommitCard
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::CommitCard (const std::string & imagePath, CardVolumeEdit & card, const BlockBuffer & edited, DiskCommandResult & result)
{
    std::string  refusal;
    HRESULT      hr      = card.Commit (edited, refusal);



    if (FAILED (hr))
    {
        DiskImageSession::RefuseCommit (imagePath, refusal, result);
    }

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
    bool                           isCard    = false;
    VolumeKind                     kind      = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;
    FileStamp                      hostStamp;
    HRESULT                        hrStamp   = S_OK;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<file>", result));

    hostForm = DiskFilePayload::ApplyHostFileName (effective, parsed);
    diskName = DiskFilePayload::GetOnDiskName (effective);

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_fileIo, options.disk.imagePath);

    if (!isCard)
    {
        hr = m_session.OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hr = ApplyUser (options, kind, diskName, result);
    CHR (hr);

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, hostBytes);

    CHRF (hr, result.Fail (options.disk.hostFile, "", "cannot be read"));

    //  A conversion Casso Explorer wrote that has an import for this file
    //  system is put back in its Apple form, under the name that form has
    //  there, unless the command line has --type, --load, --text or --basic.
    imported = hostForm && parsed.converted == ParsedHostName::ConvertedKind::Format
            && options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim
            && options.disk.typeName.empty() && !options.disk.hasLoadAddress
            && HostFileNaming::CanImport (parsed.format, kind);

    if (imported)
    {
        if (options.disk.path.empty())
        {
            diskName = HostFileNaming::MakeImportName (diskName, parsed.format, kind);
        }

        hr = FileFormatTable::Import (parsed.format, kind, hostBytes, payload, importError);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, TextEncoding::WideToNarrow (importError)));
    }
    else
    {
        hr = DiskFilePayload::BuildPutPayload (effective, kind, hostBytes, payload, result);
        CHR (hr);
    }

    //  The type and address in a name Casso Explorer wrote, unless the command
    //  line has --type, --load, --text or --basic.
    if (hostForm && parsed.hasType && effective.disk.typeName.empty() && !effective.disk.hasLoadAddress
        && effective.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        hr = DiskFilePayload::ApplyHostFileType (parsed, kind, payload);
        CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName, "it has a ProDOS type that DOS 3.3 has no file type for"));
    }

    ReadAppleDouble (options, payload);

    //  A file with no Apple dates is dated when it was last written on the PC.
    if (!payload.hasDates)
    {
        hrStamp = m_fileIo.Stat (options.disk.hostFile, hostStamp);

        if (SUCCEEDED (hrStamp))
        {
            HostAppleFile::SetHostDates (hostStamp.modifiedUnix, payload);
        }
    }

    path = FilePath::Parse (diskName);

    if (isCard)
    {
        hr = card.GetVolume().Write (path, payload, cardEdited);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (kind, opened.sectors);

        hr = volume->Write (path, payload, edited);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "write " + diskName,
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_session.SaveAndCommit (opened, edited, result);
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
    bool                           isCard  = false;
    VolumeKind                     kind    = VolumeKind::Unknown;
    CardVolumeEdit                 card;
    BlockBuffer                    cardEdited;



    CBRFEx (named, E_INVALIDARG, ReportMissingParameter ("<name>", result));

    //  A ProDOS or HFS volume on a card partition is changed by the block.
    isCard = card.TryOpen (m_fileIo, options.disk.imagePath);

    if (!isCard)
    {
        hr = m_session.OpenImage (options.disk.imagePath, opened, result);
        CHR (hr);
    }

    kind = isCard ? card.GetKind() : opened.kind;

    hr = ResolveEntryPath (options, kind, isCard ? card.GetImage() : BlockImage (opened.sectors), path, name, result);
    CHR (hr);

    if (isCard)
    {
        hr = card.GetVolume().Delete (path, cardEdited, outcome);
    }
    else
    {
        std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (kind, opened.sectors);

        hr = volume->Delete (path, edited, outcome);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "delete " + name,
                          ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, kind))));

    hr = isCard ? CommitCard (options.disk.imagePath, card, cardEdited, result) : m_session.SaveAndCommit (opened, edited, result);
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
                                                                                 : DiskCommandText::DescribeVolumeRefusal (hr, opened.kind)));

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
    VolumeKind                             kind,
    const BlockImage                     & sectors,
    const std::string                    & typed,
    std::string                          & outPath,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    FilePath       path      = FilePath::Parse (typed);
    bool           plain     = !path.IsRooted() || kind != VolumeKind::ProDos;
    bool           mine      = false;
    size_t         i         = 0;
    VolumeListing  listing;
    std::string    rest;
    char           note[128] = {};



    outPath = typed;

    BAIL_OUT_IF (plain, S_OK);

    {
        ProDosVolume  volume (sectors);

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
    VolumeKind                             kind,
    const BlockImage                     & sectors,
    FilePath                             & outPath,
    std::string                          & outName,
    DiskCommandResult                    & result)
{
    HRESULT        hr        = S_OK;
    std::string    notAllowed = "--index is only allowed when identifying a specific file with a non-unique name";
    size_t         position  = options.disk.index - 1;
    size_t         sharing   = 0;
    bool           isDos     = kind == VolumeKind::Dos33;
    bool           inRange   = false;
    bool           matches   = true;
    bool           shared    = false;
    VolumeListing  listing;
    std::string    typed;
    char           note[128] = {};



    hr = ResolveVolumePath (options, kind, sectors, options.disk.path, typed, result);
    CHR (hr);

    hr = ApplyUser (options, kind, typed, result);
    CHR (hr);

    outName = typed;
    outPath = FilePath::Parse (typed);

    BAIL_OUT_IF (!options.disk.hasIndex, S_OK);

    //  A ProDOS disk, an index past the catalog, and an index for a name only
    //  one entry has are the same mistake, so all three get the same message.
    CBRFEx (isDos, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
            result.Fail (options.disk.imagePath, "", ApplyPrefix (notAllowed.c_str())));

    {
        std::vector<Byte>  whole = sectors.ToVector();
        Dos33Volume        volume (whole);

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

    hr = ResolveEntryPath (options, opened.kind, opened.sectors, path, name, result);
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
            m_sectorCommands.RunBlockRead (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::BlockWrite:
            m_sectorCommands.RunBlockWrite (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Krunch:
            RunKrunch (options, result);
            break;

        case CommandLineOptions::DiskOptions::Command::Test:
            RunTest (options, result);
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
