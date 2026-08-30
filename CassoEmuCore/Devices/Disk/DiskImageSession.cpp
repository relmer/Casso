#include "Pch.h"

#include "DiskImageSession.h"
#include "VolumeImage.h"
#include "WozLoader.h"
#include "NibblizationLayer.h"
#include "Core/TextEncoding.h"




//
//  The META keys worth leading with, each under a label of our own.
//
//  A WOZ image's metadata is a bag of key/value pairs in whatever order its
//  writer emitted them, and the interesting ones are not first: `image_date`
//  and `contributor` come before `title` on several of the images this was
//  measured against. So the ones that identify the SOFTWARE are pulled to the
//  front, and everything else follows in the order the file stores it --
//  nothing is dropped, because a key nobody anticipated is exactly the sort of
//  thing somebody is looking for.
//
static constexpr const char *  s_kppszMetaHighlights[][2] =
{
    { "title",            "title"     },
    { "subtitle",         "subtitle"  },
    { "publisher",        "publisher" },
    { "developer",        "developer" },
    { "copyright",        "copyright" },
    { "version",          "version"   },
    { "language",         "language"  },
    { "requires_machine", "machine"   },
    { "requires_ram",     "RAM"       },
    { "notes",            "notes"     },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::FormatDetailLine
//
//  A value nobody recorded produces NOTHING, rather than a label followed by
//  empty space. That is what lets the callers below offer every field they know
//  how to read without also deciding, field by field, whether this particular
//  image answered -- and it keeps a listing of a sparse image from being mostly
//  blank labels.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageSession::FormatDetailLine (const char * label, const std::string & value)
{
    HRESULT       hr           = S_OK;   // vestigial, for the bail
    const size_t  kLabelColumn = 16;
    bool          hasValue     = !value.empty();
    std::string   text;



    BAIL_OUT_IF (!hasValue, S_OK);

    text = std::string ("  ") + label;

    while (text.size() < kLabelColumn)
    {
        text += " ";
    }

    text += value + "\n";

Error:
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::DescribeWozChunks
//
//  What a WOZ image records about itself: who imaged it, what it is, how much
//  of the surface carries data, and -- for a commercially pressed disk -- its
//  title and publisher.
//
//  THE QUARTER-TRACK COUNT IS REAL INFORMATION AND NOT TRIVIA. The head steps
//  in quarter tracks; a disk written on half or quarter tracks is a copy
//  protection, deliberately unreadable by a drive that only visits whole ones.
//  When the positions carrying data outnumber the track slots behind them, the
//  image is telling the reader why an ordinary catalog was never going to work.
//
//  META arrives as UTF-8, which the format specifies, and is converted to the
//  code page the rest of these strings are in -- so the diagnostic reaching the
//  output boundary is in ONE encoding rather than a mixture the boundary would
//  then have to guess about.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageSession::DescribeWozChunks (const std::vector<Byte> & fileBytes)
{
    HRESULT                 hr        = S_OK;   // vestigial, for the bail
    WozLoader::Description  woz;
    std::string             text;
    std::string             media;
    char                    note[160] = {};



    WozLoader::Describe (fileBytes, woz);

    BAIL_OUT_IF (!woz.isWoz, S_OK);

    snprintf (note, sizeof (note), "WOZ %d bit-stream image, INFO version %d",
              woz.wozVersion, woz.infoVersion);

    text += FormatDetailLine ("format",  note);
    text += FormatDetailLine ("creator", woz.creator);

    media = (woz.diskType == WozLoader::kDiskType525) ? "5.25-inch disk"
          : (woz.diskType == WozLoader::kDiskType35)  ? "3.5-inch disk"
                                                      : "disk of an unrecorded size";

    if (woz.writeProtected) { media += ", write-protected"; }
    if (woz.synchronized)   { media += ", tracks synchronized to each other"; }
    if (woz.cleaned)        { media += ", cleaned of drive noise"; }

    text += FormatDetailLine ("media", media);

    if (woz.hasBootSectorFormat)
    {
        const char *  boot =
            (woz.bootSectorFormat == WozLoader::kBootSector16)   ? "16-sector"
          : (woz.bootSectorFormat == WozLoader::kBootSector13)   ? "13-sector"
          : (woz.bootSectorFormat == WozLoader::kBootSectorBoth) ? "both 13- and 16-sector"
                                                                 : "not recorded";

        text += FormatDetailLine ("boots as", boot);
    }

    snprintf (note, sizeof (note),
              "%d track positions carry data, reached at %d of the 160 quarter-track "
              "stops the head can make",
              woz.trackSlotsWithData, woz.quarterTracksWithData);

    text += FormatDetailLine ("surface", note);

    // The named fields first, then whatever else the image chose to record.
    for (const auto & highlight : s_kppszMetaHighlights)
    {
        for (const WozLoader::MetaField & field : woz.meta)
        {
            if (field.key == highlight[0])
            {
                text += FormatDetailLine (highlight[1], TextEncoding::Utf8ToNarrow (field.value));
                break;
            }
        }
    }

    for (const WozLoader::MetaField & field : woz.meta)
    {
        bool  highlighted = false;

        for (const auto & highlight : s_kppszMetaHighlights)
        {
            if (field.key == highlight[0])
            {
                highlighted = true;
                break;
            }
        }

        if (!highlighted)
        {
            std::string  label = field.key;

            for (char & c : label)
            {
                c = (c == '_') ? ' ' : c;
            }

            text += FormatDetailLine (label.c_str(), TextEncoding::Utf8ToNarrow (field.value));
        }
    }

Error:
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::DescribeSurface
//
//  What the decoded sectors show, which is a different question from what the
//  container claims.
//
//  "BOOTABLE, NO FILESYSTEM" IS THE TRUE ANSWER FOR A GREAT MANY DISKS and is
//  the one worth saying out loud. Track 0 sector 0 is what the drive's ROM
//  reads and jumps into; a disk whose first sector carries code boots and runs,
//  whatever it does about files afterwards. This project's own demo disk is
//  exactly that, and reporting only "no filesystem" describes it as though it
//  were broken.
//
//  The per-track decode outcome comes from the read that already happened, so
//  it costs nothing and says the thing a copy-protected disk most wants
//  understood: the tracks are there, they simply are not standard sectors.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageSession::DescribeSurface (const OpenedImage & opened)
{
    int          complete    = 0;
    int          partial     = 0;
    int          unformatted = 0;
    int          trackCount  = opened.report.GetTrackCount();
    int          track       = 0;
    size_t       i           = 0;
    bool         bitStream   = trackCount > 0;
    bool         trackZeroOk = true;
    bool         bootCode    = false;
    std::string  text;
    char         note[512]   = {};



    // Geometry belongs to a SECTOR-ORDER file and to nothing else. Printing it
    // for a bit-stream image would describe the buffer this tool decoded into
    // rather than the disk, and for a disk that decoded into almost none of it
    // that is an actively misleading thing to say.
    if (!bitStream)
    {
        snprintf (note, sizeof (note), "%d tracks x %d sectors x %d bytes = %d bytes",
                  NibblizationLayer::kTrackCount,
                  NibblizationLayer::kSectorsPerTrack,
                  NibblizationLayer::kSectorByteSize,
                  NibblizationLayer::kImageByteSize);

        text += FormatDetailLine ("geometry", note);
    }

    for (track = 0; track < trackCount; track++)
    {
        switch (opened.report.GetOutcome (track))
        {
            case TrackDecodeOutcome::Complete:    complete++;    break;
            case TrackDecodeOutcome::Partial:     partial++;     break;
            case TrackDecodeOutcome::Unformatted: unformatted++; break;
            default:                                             break;
        }
    }

    if (bitStream)
    {
        trackZeroOk = opened.report.GetOutcome (0) == TrackDecodeOutcome::Complete;

        // NO CAUSE IS ASSERTED, only the count. A track that will not decode
        // as standard sectors may be protected, may be a format this tool does
        // not read, or may be damaged, and nothing available here separates the
        // three. What IS worth saying is that the first of those is ordinary,
        // so a reader does not conclude their disk is broken.
        snprintf (note, sizeof (note),
                  "of %d tracks, %d read as standard 16-sector data, %d only partly,\n"
                  "                and %d had no standard address fields at all%s",
                  trackCount, complete, partial, unformatted,
                  complete == 0 ? ".\n                A disk that boots and runs can still"
                                  " read this way: most\n                protected software"
                                  " wrote a track format of its own"
                                : "");

        text += FormatDetailLine ("decoded", note);
    }

    // ZEROS IN THE BUFFER MEAN TWO DIFFERENT THINGS and only one of them is
    // "blank". A track that decoded cleanly and holds zeros really is empty; a
    // track that never decoded reads back as zeros too, and calling that one
    // blank would tell somebody their bootable disk does not boot.
    if (!trackZeroOk)
    {
        text += FormatDetailLine ("boot sector",
            "track 0 did not decode as standard sectors, so what it holds\n"
            "                cannot be judged from here");
    }
    else
    {
        for (i = 0; i < (size_t) NibblizationLayer::kSectorByteSize && i < opened.sectors.size(); i++)
        {
            if (opened.sectors[i] != 0)
            {
                bootCode = true;
                break;
            }
        }

        text += FormatDetailLine ("boot sector",
                            bootCode
                                ? "track 0 sector 0 carries code. The drive's ROM reads\n"
                                  "                that sector and jumps into it, so this image"
                                  " boots something.\n                It simply keeps its files"
                                  " somewhere this tool does not read"
                                : "track 0 sector 0 is blank, so nothing here would boot");
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::DescribeUnrecognizedImage
//
//  What is left to say once neither filesystem is there, which on real disks is
//  a great deal.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageSession::DescribeUnrecognizedImage (const OpenedImage & opened)
{
    return DescribeWozChunks (opened.fileBytes) + DescribeSurface (opened);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::OpenImage
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

HRESULT DiskImageSession::OpenImage (
    const std::string  & imagePath,
    OpenedImage        & outOpened,
    DiskCommandResult  & result,
    bool                 requireFilesystem)
{
    HRESULT         hr         = S_OK;
    bool            named      = !imagePath.empty();
    bool            identified = false;
    HRESULT         statHr     = S_OK;
    MountDiagnosis  diagnosis;
    vector<Byte>    fileBytes;



    outOpened.imagePath = imagePath;

    //  Unreachable through the runner, which reports missing operands before
    //  any command runs; kept for a caller reaching the session directly. The
    //  sentence matches the runner's; the usage block is the runner's to print.
    CBRFEx (named, E_INVALIDARG,
            (result.diagnostics += "Error: required parameter <image> missing\n",
             result.exitStatus   = DiskCommandResult::kNoOutput));

    //  The reason clause comes from the shared diagnosis rather than being
    //  written here, so the console and the emulator refuse the same file with
    //  the same sentence. The read failure is the one this function diagnoses
    //  itself: it owns the file I/O, and nothing further in ever sees it.
    hr = m_fileIo.ReadAllBytes (imagePath, fileBytes);
    CHRF (hr, (diagnosis.failure = MountFailure::FileUnreadable,
               result.Fail (imagePath, "", diagnosis.Describe())));

    statHr                    = m_fileIo.Stat (imagePath, outOpened.stamp);
    outOpened.stampRecorded   = SUCCEEDED (statHr);
    outOpened.fileBytes       = fileBytes;

    hr = VolumeImage::Load (fileBytes, imagePath, outOpened.sectors, outOpened.report, diagnosis);
    CHRF (hr, result.Fail (imagePath, "", diagnosis.Describe()));

    outOpened.kind = VolumeImage::DetectFilesystem (outOpened.sectors);

    // THE STATUS AND THE STREAM BOTH STAY WHAT THEY WERE. A caller still
    // got no catalog, so this is still status 2 and still goes to the error
    // stream -- a script that pipes a listing must not suddenly find a
    // survey in the pipe. What changed is only how much the message says.
    identified = outOpened.kind != VolumeKind::Unknown || !requireFilesystem;
    CBRF (identified,
          (result.Fail (imagePath, "", kNoFilesystemText),
             result.diagnostics += DescribeUnrecognizedImage (outOpened)));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::RefuseCommit
//
//  One place that turns a refused commit into what the user sees, so a path
//  cannot report the reason without also setting the status, or the other way
//  round.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageSession::RefuseCommit (
    const std::string  & imagePath,
    const std::string  & reason,
    DiskCommandResult  & result)
{
    result.diagnostics += DiskCommandResult::Failure (imagePath, "", reason) + "\n";
    result.exitStatus   = DiskCommandResult::kNoOutput;

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::DescribeReplaceFailure
//
//  WRITE PROTECTION ARRIVES HERE AND NOWHERE ELSE when it comes from the host
//  file's read-only attribute. Nothing about the image's contents says it may
//  not be written, so the volume layer computes a perfectly good new image and
//  the platform refuses at the last step. Reporting that as a generic failure
//  would leave the user with a refusal and no idea which of the two things to
//  fix.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageSession::DescribeReplaceFailure (HRESULT hr)
{
    const char *  sentence = (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))
                                 ? "is write-protected. Clear its read-only attribute "
                                   "and try again. Nothing was written"
                                 : "could not be replaced. It may be read-only or in use";



    return sentence;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession::CommitImage
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

HRESULT DiskImageSession::CommitImage (
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
            RefuseCommit (opened.imagePath, kInUseRefusalText, result));

    progress.furthestAttempted = CommitPlan::Step::Reverify;

    //  A disk being made for the first time has nothing to have changed, so
    //  the check is skipped rather than failed. See OpenedImage::isNew.
    if (!opened.isNew)
    {
        CBRFEx (opened.stampRecorded, HRESULT_FROM_WIN32 (ERROR_CANT_ACCESS_FILE),
                RefuseCommit (opened.imagePath,
                              "could not be checked for changes when it was read, so it will "
                              "not be written over", result));

        hr = m_fileIo.Stat (opened.imagePath, observed);
        CHRF (hr, RefuseCommit (opened.imagePath,
                                "has gone away since it was read", result));

        stale = CommitPlan::IsStale (opened.stamp.sizeBytes, opened.stamp.modifiedUnix,
                                     observed.sizeBytes,     observed.modifiedUnix);

        // STG_E_NOTCURRENT says exactly this and nothing else -- the object
        // changed since it was last read. The Win32 table has no code for it,
        // and inventing a near-miss from it would read as a different problem
        // in a log.
        CBRFEx (!stale, STG_E_NOTCURRENT,
                RefuseCommit (opened.imagePath,
                              "changed since it was read. Nothing was written; read it "
                              "again and retry", result));
    }

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
                          "already has that many temporary files beside it. Remove them "
                          "and try again", result));

    progress.furthestAttempted = CommitPlan::Step::WriteTemporary;

    hr = m_fileIo.WriteAllBytes (tempPath, newImageBytes);
    CHRF (hr, RefuseCommit (opened.imagePath,
                            "could not be written beside. The folder may be read-only "
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
//  DiskImageSession::SaveAndCommit
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

HRESULT DiskImageSession::SaveAndCommit (
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
