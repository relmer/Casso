#include "Pch.h"

#include "DiskCommandRunner.h"
#include "AppleTextCodec.h"
#include "VolumeImage.h"
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
//  DiskCommandRunner::OpenImage
//
//  Reads the image, normalizes its sector order, and identifies the filesystem.
//  Each failure explains itself rather than collapsing into one message,
//  because "cannot read the file" and "read it but do not recognize it" send
//  the user somewhere different.
//
//  IT ALSO RECORDS THE SIZE AND MODIFICATION TIME, HERE AND NOWHERE ELSE. The
//  staleness check compares what the file looked like when its contents were
//  taken against what it looks like at the moment of commit; a stamp taken any
//  later than this agrees with itself and closes no window at all.
//
//  A stamp the platform declines to give is recorded as ABSENT rather than as
//  zero, so a commit can refuse instead of silently comparing two zeros and
//  concluding nothing changed. Reading does not need it and is unaffected.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::OpenImage (
    const std::string  & imagePath,
    OpenedImage        & outOpened,
    DiskCommandResult  & result)
{
    HRESULT       hr      = S_OK;
    bool          named   = !imagePath.empty();
    HRESULT       statHr  = S_OK;
    vector<Byte>  fileBytes;



    outOpened.imagePath = imagePath;

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

    statHr                    = m_fileIo.Stat (imagePath, outOpened.stamp);
    outOpened.stampRecorded   = SUCCEEDED (statHr);

    hr = VolumeImage::Load (fileBytes, imagePath, outOpened.sectors, outOpened.report);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (imagePath, "",
            "is not a disk image this tool can read") + "\n";
        result.exitStatus   = kNoOutput;
        return hr;
    }

    outOpened.kind = VolumeImage::DetectFilesystem (outOpened.sectors);

    if (outOpened.kind == VolumeKind::Unknown)
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
//  DiskCommandRunner::RefuseCommit
//
//  One place that turns a refused commit into what the user sees, so a path
//  cannot report the reason without also setting the status, or the other way
//  round.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RefuseCommit (
    const std::string  & imagePath,
    const std::string  & reason,
    DiskCommandResult  & result)
{
    result.diagnostics += Failure (imagePath, "", reason) + "\n";
    result.exitStatus   = kNoOutput;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::CommitImage
//
//  The whole of putting a computed image where the old one was, in the order
//  the guarantees require: refuse while somebody else has it, refuse if it
//  moved under us, write the new bytes somewhere they cannot be mistaken for
//  the image, and only then let them become the image in one step.
//
//  NOTHING HERE MAY LEAVE THE TARGET PART-WRITTEN, which is why the new bytes
//  never go near it. They go to a temporary beside it and arrive by an atomic
//  replace, so an interruption at any point leaves either the old image or the
//  new one, and never a mixture.
//
//  THE CLEANUP IS DRIVEN BY THE PLAN, NOT BY EACH FAILURE PATH. A removal
//  written beside each bail-out is a removal the next bail-out forgets: the
//  ordering rule is asked once, at the single exit, from a record of how far
//  the sequence actually got. That record tracks the furthest step ATTEMPTED,
//  because a write that failed halfway is exactly the case with a partial file
//  sitting there and exactly the case a reader assumes is clean.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::CommitImage (
    const OpenedImage   & opened,
    const vector<Byte>  & newImageBytes,
    DiskCommandResult   & result)
{
    HRESULT               hr            = S_OK;
    HRESULT               removeHr      = S_OK;
    bool                  held          = false;
    bool                  stale         = false;
    bool                  foundFreeName = false;
    bool                  cleanUp       = false;
    unsigned              attempt       = 0;
    CommitPlan::Progress  progress;
    FileStamp             observed;
    std::string           tempPath;



    // The probe comes first because it is the only check that costs nothing to
    // be wrong about: refusing before anything exists leaves no cleanup to get
    // right. It catches another TOOL holding the file and cannot catch this
    // emulator, which keeps no handle on a mounted image.
    progress.furthestAttempted = CommitPlan::Step::Probe;

    held = m_fileIo.IsHeldByAnotherProcess (opened.imagePath);
    CBRFEx (!held, HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION),
            RefuseCommit (opened.imagePath,
                          "is open in another program -- close it and try again", result));

    progress.furthestAttempted = CommitPlan::Step::Reverify;

    CBRFEx (opened.stampRecorded, HRESULT_FROM_WIN32 (ERROR_CANT_ACCESS_FILE),
            RefuseCommit (opened.imagePath,
                          "could not be checked for changes when it was read, so it will "
                          "not be written over", result));

    hr = m_fileIo.Stat (opened.imagePath, observed);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "has gone away since it was read", result));

    stale = CommitPlan::IsStale (opened.stamp.sizeBytes, opened.stamp.modifiedUnix,
                                 observed.sizeBytes,     observed.modifiedUnix);

    // STG_E_NOTCURRENT says exactly this and nothing else -- the object changed
    // since it was last read. The Win32 table has no code for it, and inventing
    // a near-miss from that table would read as a different problem in a log.
    CBRFEx (!stale, STG_E_NOTCURRENT,
            RefuseCommit (opened.imagePath,
                          "changed since it was read -- nothing was written, read it "
                          "again and retry", result));

    // Step over anything already sitting at the name we would take. This is
    // the abandoned-temporary case; the invocation tag inside the name is what
    // handles two live invocations, since both of those would otherwise find
    // attempt zero free at the same instant.
    for (attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
    {
        tempPath      = CommitPlan::TemporaryPathFor (opened.imagePath, m_invocationTag, attempt);
        foundFreeName = !m_fileIo.Exists (tempPath);

        if (foundFreeName)
        {
            break;
        }
    }

    CBRFEx (foundFreeName, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS),
            RefuseCommit (opened.imagePath,
                          "already has that many temporary files beside it -- remove them "
                          "and try again", result));

    progress.furthestAttempted = CommitPlan::Step::WriteTemporary;

    hr = m_fileIo.WriteAllBytes (tempPath, newImageBytes);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "could not be written beside -- the folder may be read-only "
                            "or full", result));

    progress.furthestAttempted = CommitPlan::Step::Replace;

    hr = m_fileIo.ReplaceAtomically (tempPath, opened.imagePath);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "could not be replaced -- it may be read-only or in use", result));

    progress.replaceSucceeded = true;

Error:
    cleanUp = CommitPlan::ShouldRemoveTemporary (progress);

    if (cleanUp)
    {
        removeHr = m_fileIo.Remove (tempPath);
        IGNORE_RETURN_VALUE (removeHr, S_OK);
    }

    return hr;
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
    OpenedImage         opened;
    VolumeListing       listing;
    char                summary[128] = {};



    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
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
            result.output += (opened.kind == VolumeKind::Dos33)
                           ? FormatDos33Entry (entry)
                           : FormatProDosEntry (entry, options.disk.longListing);

            result.output += "\n";
        }

        snprintf (summary, sizeof (summary), "\n%u %s free of %u\n",
                  (unsigned) listing.freeUnits,
                  (opened.kind == VolumeKind::Dos33) ? "sectors" : "blocks",
                  (unsigned) listing.totalUnits);

        result.output += summary;
    }

    // Damage from the catalog walk, and from the track layer beneath it.
    //
    // Each message must say the LISTING IS INCOMPLETE, not merely that the disk
    // is damaged. A pipeline that treats status 1 as a warning and carries on
    // will otherwise consume a truncated listing that reads as whole, and its
    // log will describe a disk problem rather than a missing-entries problem.
    // The status carries the distinction for a script; the wording has to carry
    // it for whoever reads the log afterwards.
    for (const std::string & note : listing.damage)
    {
        result.diagnostics += Failure (options.disk.imagePath, "",
            note + " -- THIS LISTING IS INCOMPLETE, entries may be missing") + "\n";
        result.exitStatus   = kWithComplaints;
    }

    if (opened.report.HasDataLoss())
    {
        snprintf (summary, sizeof (summary),
                  "%d sector(s) could not be decoded and read back as zeros "
                  "-- THIS LISTING IS INCOMPLETE, entries may be missing",
                  opened.report.GetUnrecoveredCount());

        result.diagnostics += Failure (options.disk.imagePath, "", summary) + "\n";
        result.exitStatus   = kWithComplaints;
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
    HRESULT      hr = S_OK;
    std::string  hostText;



    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            // Both conventions decode identically -- once the high bit is
            // ignored, high-ASCII and plain-ASCII text differ in nothing -- so
            // this choice is inert on the read path. It becomes load-bearing
            // when writing, which is where the terminator gets chosen.
            //
            // High ASCII is the measured answer on both filesystems: the ProDOS
            // fixture volumes carry TXT files that are predominantly high-bit
            // with $8D terminators, not the plain seven-bit form usually
            // assumed for that filesystem.
            AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, hostText);

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            result.diagnostics += "--basic is not available in this build: "
                                  "no Applesoft tokenizer yet\n";
            result.exitStatus   = kNoOutput;
            hr                  = E_NOTIMPL;
            break;

        default:
            result.diagnostics += "unknown encoding -- try: --text, --basic, --verbatim\n";
            result.exitStatus   = kNoOutput;
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
    HRESULT             hr        = S_OK;
    bool                named     = !options.disk.path.empty();
    OpenedImage         opened;
    FilePayload         payload;
    FilePath            path;
    char                note[128] = {};



    if (!named)
    {
        result.diagnostics += "no file named to extract\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (options.disk.path);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
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

    hr = ApplyEncoding (options, payload, result);
    BAIL_OUT_IF (FAILED (hr), hr);

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

    if (opened.report.HasDataLoss())
    {
        snprintf (note, sizeof (note),
                  "%d sector(s) could not be decoded -- THIS FILE IS INCOMPLETE, "
                  "unreadable sectors were delivered as zeros",
                  opened.report.GetUnrecoveredCount());

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
