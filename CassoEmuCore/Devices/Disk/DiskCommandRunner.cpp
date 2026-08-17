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
    outOpened.fileBytes       = fileBytes;

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

std::string DiskCommandRunner::DescribeVolumeRefusal (HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        return "is locked on this volume -- unlock it on the disk before writing over it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DISK_FULL))
    {
        return "does not fit: the volume has no room left, either for the file's "
               "contents or for another catalog entry";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME))
    {
        return "is not a name this filesystem can store -- it must be a single "
               "component starting with a letter, short enough for the catalog, "
               "and free of commas and control characters";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))
    {
        return "is not on this volume";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE))
    {
        return "is larger than this filesystem can record for one file";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER))
    {
        return "is a binary, which has to be told where it loads -- give --addr $XXXX";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED))
    {
        return "is a directory, and this tool does not go inside one -- so removing "
               "it would strand everything beneath it";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF))
    {
        return "has a sector chain that cannot be followed to its end";
    }

    return "was refused by the filesystem on this volume";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::DescribeReplaceFailure
//
//  WRITE PROTECTION ARRIVES HERE AND NOWHERE ELSE when it comes from the host
//  file's read-only attribute. Nothing about the image's contents says it may
//  not be written, so the volume layer computes a perfectly good new image and
//  the platform refuses at the last step. Reporting that as a generic failure
//  would leave the user with a refusal and no idea which of the two things to
//  fix.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::DescribeReplaceFailure (HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
    {
        return "is write-protected -- clear its read-only attribute and try again. "
               "Nothing was written";
    }

    return "could not be replaced -- it may be read-only or in use";
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
    CHRF (hr, RefuseCommit (opened.imagePath, DescribeReplaceFailure (hr), result));

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
//  DiskCommandRunner::OnDiskNameFor
//
//  --as when the caller gave one, and otherwise the host file's own last
//  component. Nothing is stripped or shortened on the way: the caller already
//  chose that name, and inventing a different one would leave them looking for
//  a file the catalog does not hold. A name the filesystem cannot store is
//  refused by the volume layer, which is where that rule belongs.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandRunner::OnDiskNameFor (const CommandLineOptions & options)
{
    size_t  lastSeparator = options.disk.hostFile.find_last_of ("/\\");



    if (!options.disk.path.empty())
    {
        return options.disk.path;
    }

    if (lastSeparator == std::string::npos)
    {
        return options.disk.hostFile;
    }

    return options.disk.hostFile.substr (lastSeparator + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::ResolveFileType
//
//  The two filesystems number their types differently and name them
//  differently, so the spellings accepted here are each one's own. A letter
//  that means Applesoft on DOS 3.3 and nothing on ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  A caller who named no type gets the one that matches the conversion they
//  asked for -- text when they asked for text, a binary otherwise, which is
//  what a build loop places.
//
//  AN UNRECOGNIZED SPELLING IS REFUSED, not defaulted. Defaulting would place
//  the file under a type the caller did not ask for and say nothing, and the
//  guest would report the mismatch much later as a file that will not load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::ResolveFileType (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    Byte                      & outType,
    DiskCommandResult         & result)
{
    HRESULT      hr         = S_OK;
    bool         isDos      = kind == VolumeKind::Dos33;
    bool         isText     = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text;
    bool         named      = !options.disk.typeName.empty();
    bool         recognized = true;
    size_t       i          = 0;
    std::string  spelling   = options.disk.typeName;



    outType = isDos ? (isText ? Dos33Volume::kTypeText  : Dos33Volume::kTypeBinary)
                    : (isText ? ProDosVolume::kTypeText : ProDosVolume::kTypeBinary);

    BAIL_OUT_IF (!named, S_OK);

    for (i = 0; i < spelling.size(); i++)
    {
        if (spelling[i] >= 'a' && spelling[i] <= 'z')
        {
            spelling[i] = (char) (spelling[i] - 'a' + 'A');
        }
    }

    if (isDos)
    {
        if      (spelling == "T" || spelling == "TXT") { outType = Dos33Volume::kTypeText; }
        else if (spelling == "I" || spelling == "INT") { outType = Dos33Volume::kTypeInteger; }
        else if (spelling == "A" || spelling == "BAS") { outType = Dos33Volume::kTypeApplesoft; }
        else if (spelling == "B" || spelling == "BIN") { outType = Dos33Volume::kTypeBinary; }
        else if (spelling == "R" || spelling == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                           { recognized = false; }
    }
    else
    {
        if      (spelling == "T" || spelling == "TXT") { outType = ProDosVolume::kTypeText; }
        else if (spelling == "B" || spelling == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (spelling == "A" || spelling == "BAS") { outType = ProDosVolume::kTypeBasic; }
        else if (spelling == "S" || spelling == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                           { recognized = false; }
    }

    if (!recognized)
    {
        result.diagnostics += "--type " + options.disk.typeName + " means nothing on this volume -- "
                            + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                                     : "ProDOS takes TXT, BIN, BAS or SYS")
                            + "\n";

        result.exitStatus   = kNoOutput;
        hr                  = E_NOTIMPL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::BuildPutPayload
//
//  What the host file becomes on the disk.
//
//  VERBATIM IS THE ABSENCE OF A CHARACTER CONVERSION AND NOTHING MORE. The
//  bytes go down as they came in; the length and whatever header the type
//  carries are still applied below this, because those record where the file
//  ENDS and are its identity rather than a transformation of it. That is what
//  makes extract-edit-replace safe: the bytes the caller did not touch come
//  back exactly as they were.
//
//  Text is written in the high-ASCII convention on BOTH filesystems, which is
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
    HRESULT      hr        = S_OK;
    Byte         type      = 0;
    size_t       badOffset = 0;
    char         note[160] = {};
    std::string  hostText;



    hr = ResolveFileType (options, kind, type, result);
    CHR (hr);

    outPayload.type           = type;
    outPayload.loadAddress    = options.disk.loadAddress;
    outPayload.hasLoadAddress = options.disk.hasLoadAddress;

    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            outPayload.bytes    = hostBytes;
            outPayload.encoding = PayloadEncoding::Verbatim;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            hostText.assign (hostBytes.begin(), hostBytes.end());

            hr = AppleTextCodec::Encode (hostText, AppleTextConvention::HighAscii,
                                         outPayload.bytes, badOffset);

            if (FAILED (hr))
            {
                // Naming the offset is the whole value of the refusal. A smart
                // quote pasted into a listing looks identical to a plain one in
                // an editor, and "somewhere in this file" is not actionable.
                snprintf (note, sizeof (note),
                          "byte %u of the host file has no Apple II text representation, "
                          "so nothing was converted\n",
                          (unsigned) badOffset);

                result.diagnostics += note;
                result.exitStatus   = kNoOutput;
                break;
            }

            outPayload.encoding = PayloadEncoding::HostText;
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::SaveAndCommit
//
//  The second half of every edit: render the new sector buffer back into the
//  container it came from, then put it where the old one was.
//
//  THE ORDER IS THE GUARANTEE. Rendering can still refuse -- a bit-stream image
//  whose edit lands on a track that cannot be re-encoded as standard sectors is
//  refused whole, never track by track -- and a refusal at that point has
//  touched nothing, because the commit has not begun. Committing first and
//  rendering afterwards is not a rearrangement of this; it is the half-written
//  image the all-or-nothing rule exists to forbid.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskCommandRunner::SaveAndCommit (
    const OpenedImage   & opened,
    const vector<Byte>  & editedSectors,
    DiskCommandResult   & result)
{
    HRESULT       hr = S_OK;
    std::string   refusal;
    std::string   reason;
    vector<Byte>  newFileBytes;



    hr     = VolumeImage::Save (opened.fileBytes, opened.imagePath, editedSectors,
                                newFileBytes, refusal);

    reason = refusal.empty()
           ? std::string ("could not be written back in the format it came from")
           : refusal;

    CHRF (hr, RefuseCommit (opened.imagePath, reason, result));

    hr = CommitImage (opened, newFileBytes, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner::RunPut
//
//  Placing a host file on the disk: read it, convert it if asked, let the
//  filesystem compute the whole new volume, render that back into the container
//  and commit it.
//
//  NOTHING IS WRITTEN UNTIL EVERY DECISION HAS BEEN MADE. Each refusal below --
//  no such host file, a type nobody recognizes, a name the catalog cannot
//  store, a locked file, a volume with no room -- happens with the image on
//  disk untouched, because the only thing this path can commit is a finished
//  buffer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandRunner::RunPut (const CommandLineOptions & options, DiskCommandResult & result)
{
    HRESULT       hr       = S_OK;
    bool          named    = !options.disk.hostFile.empty();
    std::string   diskName = OnDiskNameFor (options);
    OpenedImage   opened;
    FilePayload   payload;
    FilePath      path;
    vector<Byte>  hostBytes;
    vector<Byte>  edited;



    if (!named)
    {
        result.diagnostics += "no host file named to place -- put takes the file to "
                              "copy onto the disk\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, E_INVALIDARG);
    }

    hr = OpenImage (options.disk.imagePath, opened, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    hr = m_fileIo.ReadAllBytes (options.disk.hostFile, hostBytes);

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.hostFile, "", "cannot be read") + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = BuildPutPayload (options, opened.kind, hostBytes, payload, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    path = FilePath::Parse (diskName);

    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Write (path, payload, edited);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, diskName,
                                       DescribeVolumeRefusal (hr)) + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = SaveAndCommit (opened, edited, result);
    BAIL_OUT_IF (FAILED (hr), hr);

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
    HRESULT        hr    = S_OK;
    bool           named = !options.disk.path.empty();
    OpenedImage    opened;
    FilePath       path;
    DeleteOutcome  outcome;
    vector<Byte>   edited;



    if (!named)
    {
        result.diagnostics += "no file named to delete\n";
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

        hr = volume.Delete (path, edited, outcome);
    }

    if (FAILED (hr))
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path,
                                       DescribeVolumeRefusal (hr)) + "\n";
        result.exitStatus   = kNoOutput;
        BAIL_OUT_IF (true, hr);
    }

    hr = SaveAndCommit (opened, edited, result);
    BAIL_OUT_IF (FAILED (hr), hr);

    for (const std::string & warning : outcome.warnings)
    {
        result.diagnostics += Failure (options.disk.imagePath, options.disk.path, warning) + "\n";
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
            RunPut (options, result);
            break;

        case CommandLineOptions::DiskOptions::Verb::Delete:
            RunDelete (options, result);
            break;

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
