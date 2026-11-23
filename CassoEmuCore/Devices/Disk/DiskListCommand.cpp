#include "Pch.h"

#include "DiskListCommand.h"
#include "DiskImageSession.h"
#include "IDiskFileIo.h"
#include "DiskCommandText.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33StartupProgram.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NativeListing.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Utils.h"
#include "Machines/Apple2/Common/ProDosStartup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskListCommand::GetDos33TypeLetter
//
//  The letters DOS itself prints. The lock bit is already masked off by the
//  reader, so only the type remains.
//
////////////////////////////////////////////////////////////////////////////////

char DiskListCommand::GetDos33TypeLetter (Byte type)
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
//  DiskListCommand::FormatDos33Entry
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

std::string DiskListCommand::FormatDos33Entry (const FileEntry & entry)
{
    char  sectors[8] = {};



    snprintf (sectors, sizeof (sectors), "%03u", (unsigned) (entry.sizeUnits & 0x3FF));

    return std::string (entry.isLocked ? "*" : " ")
         + std::string (1, GetDos33TypeLetter (entry.type))
         + " " + sectors + " " + entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskListCommand::FormatProDosEntry
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

std::string DiskListCommand::FormatProDosEntry (const FileEntry & entry, size_t nameWidth)
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
//  DiskListCommand::RunList
//
//  Damage does not fail the listing. The developer this serves is recovering
//  old disks, and the entries that DID read are the ones they need; a refusal
//  would withhold exactly the information the situation calls for. It earns the
//  complaints status instead, so a script can tell a partial read from a clean
//  one without parsing anything.
//
////////////////////////////////////////////////////////////////////////////////

void DiskListCommand::RunList (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT                        hr           = S_OK;
    DiskImageSession::OpenedImage  opened;
    VolumeListing                  listing;
    std::string                    directory;
    char                           summary[128] = {};
    bool                           isFlat       = false;
    size_t                         nameWidth    = kNameWidth;
    std::string                    note;



    hr = m_context.GetSession().OpenImage (options.disk.imagePath, opened, result);
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
                              m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, opened.kind))));

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
            if (!Dos33StartupProgram (Dos33SectorAccess (opened.sectors)).HasDosImage())
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
//  DiskListCommand::AppendNativeListing
//
//  A directory typed for a file system with none is turned down, as it is on
//  DOS 3.3.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskListCommand::AppendNativeListing (
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
                                 m_context.ApplyPrefix (DiskCommandText::DescribeVolumeRefusal (hr, opened.kind))));

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
//  DiskListCommand::ReportListingDamage
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

void DiskListCommand::ReportListingDamage (
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
//  DiskListCommand::AppendDirectoryRows
//
//  A RECURSIVE ROW GIVES ITS FULL PATH, because a listing that reaches
//  several directories has rows whose names repeat: two files called NOTES in
//  different directories are two rows reading NOTES, and nothing in the output
//  says which is which. The row is formatted from a copy whose name is the
//  path, so the columns stay exactly as a one-directory listing prints them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskListCommand::AppendDirectoryRows (
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
//  DiskListCommand::MeasureNameWidth
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskListCommand::MeasureNameWidth (const IVolume & volume, const std::string & directory, size_t & inOutWidth)
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
//  DiskListCommand::HasProDosSystemFile
//
//  Whether the volume carries PRODOS, the file its boot block loads. Without
//  it the loader has nothing to start, so the disk is not bootable whatever
//  else it holds.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskListCommand::HasProDosSystemFile (const VolumeListing & listing)
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
