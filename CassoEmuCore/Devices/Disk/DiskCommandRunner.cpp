#include "Pch.h"

#include "DiskCommandRunner.h"
#include "Dos33Volume.h"
#include "ProDosVolume.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DiskCommandRunner
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandRunner::DiskCommandRunner (IDiskFileIo & fileIo)
    : m_fileIo (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Failure
//
//  Image, file, reason, in that order. A script's user sees the first line and
//  needs to know which disk before anything else; the reason is useless without
//  it when a build places twenty files.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::Failure (
    const std::string & imagePath,
    const std::string & fileName,
    const std::string & reason)
{
    std::string  text = imagePath.empty() ? std::string ("(no image)") : imagePath;



    if (!fileName.empty())
    {
        text += ": " + fileName;
    }

    text += ": " + reason;

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Dos33TypeLetter
//
//  The letters DOS itself prints. The lock bit is already masked off by the
//  reader, so only the type remains.
//
////////////////////////////////////////////////////////////////////////////////

char DiskCommandRunner::Dos33TypeLetter (Byte type)
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
         + std::string (1, Dos33TypeLetter (entry.type))
         + " " + sectors + " " + entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::FormatProDosEntry
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::FormatProDosEntry (const FileEntry & entry, bool longForm)
{
    char         detail[64] = {};
    std::string  text;



    text = std::string (entry.isLocked ? "*" : " ") + entry.name;

    while (text.size() < 18)
    {
        text += " ";
    }

    snprintf (detail, sizeof (detail), "%s $%02X %5u",
              entry.isDirectory ? "DIR" : "   ",
              (unsigned) entry.type,
              (unsigned) entry.sizeUnits);

    text += detail;

    if (longForm)
    {
        char  more[64] = {};

        snprintf (more, sizeof (more), "  eof=%u aux=$%04X",
                  (unsigned) entry.eofBytes, (unsigned) entry.auxType);

        text += more;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::OpenVolume
//
//  Reads the image, normalizes its sector order, and identifies the filesystem.
//  Each failure explains itself rather than collapsing into one message,
//  because "cannot read the file" and "read it but do not recognize it" send
//  the user somewhere different.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::OpenVolume (
    const std::string   & imagePath,
    vector<Byte>        & outSectors,
    VolumeKind          & outKind,
    SectorDecodeReport  & outReport,
    DiskCommandResult   & result)
{
    HRESULT       hr    = S_OK;
    bool          named = !imagePath.empty();
    vector<Byte>  fileBytes;



    if (!named)
    {
        result.diagnostics += "no disk image named\n";
        result.exitStatus   = kNoOutput;
        return E_INVALIDARG;
    }

    hr = m_fileIo.ReadAllBytes (imagePath, fileBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (imagePath, "", "cannot be read") + "\n";
        result.exitStatus   = kNoOutput;
        return hr;
    }

    hr = VolumeImage::Load (fileBytes, imagePath, outSectors, outReport);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (imagePath, "",
            "is not a disk image this tool can read") + "\n";
        result.exitStatus   = kNoOutput;
        return hr;
    }

    outKind = VolumeImage::DetectFilesystem (outSectors);

    if (outKind == VolumeKind::Unknown)
    {
        result.diagnostics += Failure (imagePath, "",
            "carries no DOS 3.3 or ProDOS filesystem this tool recognizes") + "\n";
        result.exitStatus   = kNoOutput;
        return E_FAIL;
    }

    return S_OK;
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
    HRESULT             hr           = S_OK;
    VolumeKind          kind         = VolumeKind::Unknown;
    vector<Byte>        sectors;
    SectorDecodeReport  report;
    VolumeListing       listing;
    char                summary[128] = {};



    hr = OpenVolume (options.disk.imagePath, sectors, kind, report, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    {
        Dos33Volume   dos (sectors);
        ProDosVolume  pro (sectors);
        IVolume     & volume = (kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Enumerate (listing);

        if (FAILED (hr))
        {
            result.diagnostics += Failure (options.disk.imagePath, "",
                "catalog could not be read") + "\n";
            result.exitStatus   = kNoOutput;
            BAIL_OUT_IF (true, hr);
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

        for (const FileEntry & entry : listing.entries)
        {
            result.output += (kind == VolumeKind::Dos33)
                           ? FormatDos33Entry (entry)
                           : FormatProDosEntry (entry, options.disk.longListing);

            result.output += "\n";
        }

        snprintf (summary, sizeof (summary), "\n%u %s free of %u\n",
                  (unsigned) listing.freeUnits,
                  (kind == VolumeKind::Dos33) ? "sectors" : "blocks",
                  (unsigned) listing.totalUnits);

        result.output += summary;
    }

    // Damage from the catalog walk, and from the track layer beneath it.
    for (const std::string & note : listing.damage)
    {
        result.diagnostics += Failure (options.disk.imagePath, "", note) + "\n";
        result.exitStatus   = kWithComplaints;
    }

    if (report.HasDataLoss())
    {
        snprintf (summary, sizeof (summary),
                  "%d sector(s) could not be decoded and read back as zeros",
                  report.GetUnrecoveredCount());

        result.diagnostics += Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = kWithComplaints;
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
    HRESULT             hr        = S_OK;
    VolumeKind          kind      = VolumeKind::Unknown;
    bool                named     = !options.disk.path.empty();
    vector<Byte>        sectors;
    SectorDecodeReport  report;
    FilePayload         payload;
    FilePath            path;
    char                note[128] = {};



    if (!named)
    {
        result.diagnostics += "no file named to extract\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenVolume (options.disk.imagePath, sectors, kind, report, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (sectors);
        ProDosVolume  pro (sectors);
        IVolume     & volume = (kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Read (path, payload);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
            "could not be read from this volume") + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    if (!options.disk.hostFile.empty())
    {
        hr = m_fileIo.WriteAllBytes (options.disk.hostFile, payload.bytes);

        if (FAILED (hr))
        {
            result.diagnostics += Failure (options.disk.hostFile, "",
                "could not be written") + "\n";
            result.exitStatus   = kNoOutput;
            BAIL_OUT_IF (true, hr);
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
                  options.disk.path.c_str(),
                  (unsigned) payload.loadAddress,
                  (unsigned) payload.bytes.size());

        result.diagnostics += note;
    }

    if (report.HasDataLoss())
    {
        snprintf (note, sizeof (note),
                  "%d sector(s) could not be decoded; extracted content may be incomplete",
                  report.GetUnrecoveredCount());

        result.diagnostics += Failure (options.disk.imagePath, options.disk.path, note) + "\n";
        result.exitStatus   = kWithComplaints;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::Run
//
//  A verb this build does not implement reports failure rather than doing
//  nothing quietly, so an absent capability cannot be mistaken for a completed
//  operation.
//
////////////////////////////////////////////////////////////////////////////////

DiskCommandResult DiskCommandRunner::Run (const CommandLineOptions & options)
{
    DiskCommandResult  result;



    switch (options.disk.verb)
    {
        case CommandLineOptions::DiskOptions::Verb::List:
            RunList (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Get:
            RunGet (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Put:
        case CommandLineOptions::DiskOptions::Verb::Delete:
        case CommandLineOptions::DiskOptions::Verb::Boot:
            result.diagnostics += "that disk verb is not available in this build\n";
            result.exitStatus   = kNoOutput;
            break;

        default:
            result.diagnostics += "unknown disk verb -- try: list, get, put, delete, boot\n";
            result.exitStatus   = kNoOutput;
            break;
    }

    return result;
}
