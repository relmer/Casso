#include "Pch.h"

#include "DiskStoreMount.h"
#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  Test-friendly mount path that bypasses the host filesystem. The
//  virtualPath is used purely as the round-trip identifier; production
//  code uses Mount() which reads from disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    MountDiagnosis  ignored;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    return MountFromBytes (slot, drive, virtualPath, fmt, bytes, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  The same mount, saying why it refused. A load that fails is classified from
//  the bytes it was given, which is where the answer is: the store knows how
//  many bytes arrived and which container the name promised, and the WOZ
//  loader knows whether its header was ever there.
//
//  A mount that succeeds calls the bay-change sink with Inserted, as Mount
//  does, so a controller attached by that sink reads the new image. The save
//  of the disk it replaces runs on the I/O queue, and this waits for it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT               hr   = S_OK;
    shared_ptr<MountJob>  job  = std::make_shared<MountJob>();
    shared_ptr<bool>      done = std::make_shared<bool> (false);



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    job->slot   = slot;
    job->drive  = drive;
    job->path   = virtualPath;
    job->format = fmt;
    job->bytes  = bytes;
    job->done   = [done] (HRESULT, const MountDiagnosis &) { *done = true; };

    ReplaceBay (job, [this, job] (HRESULT hrReplace) { FinishMountFromBytes (job, hrReplace); });

    hr = m_store.WaitForIo (*done);
    CHR (hr);

    hr = job->result;
    CHR (hr);

Error:
    outDiagnosis = job->diagnosis;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishMountFromBytes
//
//  The replace is over: its guest copy is reported, then the insert.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::FinishMountFromBytes (const shared_ptr<MountJob> & job, HRESULT hr)
{
    ReportReplaceOutcome (job->slot, job->drive, job->path, hr, job->outcome);

    if (SUCCEEDED (hr))
    {
        m_store.EmitBayChange (job->slot, job->drive, BayChange::Inserted);
    }

    job->result = hr;
    job->done (hr, job->diagnosis);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceBay
//
//  A mount over a bay, in its order: the new image is built from its bytes
//  first, and after a load that fails the disk is in the drive; then the disk in
//  the bay is saved, and the two are swapped only when the save left no write
//  outside a file.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::ReplaceBay (const shared_ptr<MountJob> & job, std::function<void (HRESULT)> done)
{
    HRESULT                            hr      = S_OK;
    bool                               isValid = DiskImageStore::IsValidBay (job->slot, job->drive);
    shared_ptr<unique_ptr<DiskImage>>  loaded  = std::make_shared<unique_ptr<DiskImage>>();



    job->diagnosis              = MountDiagnosis();
    job->diagnosis.format       = job->format;
    job->diagnosis.fileByteSize = job->bytes.size();

    CBRAEx (isValid, E_INVALIDARG);

    hr = LoadImage (job->format, job->bytes, job->path, *loaded, job->diagnosis);
    CHR (hr);

    SaveBeforeReplace (job, [this, job, loaded, done] (HRESULT hrSave)
    {
        if (SUCCEEDED (hrSave))
        {
            SwapInImage (m_store.GetEntry (job->slot, job->drive), std::move (*loaded), job->path, job->format);
        }

        NoteKeptDisk (*job, hrSave);
        done (hrSave);
    });

Error:
    if (FAILED (hr))
    {
        if (isValid)
        {
            NoteKeptDisk (*job, hr);
        }

        done (hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteKeptDisk
//
//  A disk that stays in the drive is stated in the decline's text.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::NoteKeptDisk (MountJob & job, HRESULT hr)
{
    if (FAILED (hr) && m_store.GetEntry (job.slot, job.drive).mounted && job.diagnosis.keptPath.empty())
    {
        job.diagnosis.keptPath = m_store.GetEntry (job.slot, job.drive).path;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveBeforeReplace
//
//  The save before a mount over the bay. After it, the mount goes ahead when
//  every guest write is in a file, and otherwise declines with UnsavedWrites,
//  with the save's error when the save failed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::SaveBeforeReplace (const shared_ptr<MountJob> & job, std::function<void (HRESULT)> done)
{
    Entry  & entry = m_store.GetEntry (job->slot, job->drive);



    if (!entry.mounted || entry.image == nullptr)
    {
        done (S_OK);
    }
    else
    {
        m_store.m_save.Save (entry, FlushMoment::Replacing, [this, job, done] (HRESULT hrSave, const FlushOutcome & outcome)
        {
            UNREFERENCED_PARAMETER (hrSave);

            DiskStoreSave::MergeOutcome (outcome, job->outcome);
            done (DecideReplace (*job));
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecideReplace
//
//  Whether the disk in the bay can be replaced after its save: it is clean, or
//  the new file is another one and the recovery copy is current. A remount of
//  the same file does not count a recovery copy, because re-reading the file
//  would replace the session with the file's older content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::DecideReplace (MountJob & job)
{
    HRESULT   hr         = S_OK;
    Entry   & entry      = m_store.GetEntry (job.slot, job.drive);
    bool      isSameFile = false;
    bool      canReplace = true;



    if (entry.mounted && entry.image != nullptr)
    {
        isSameFile = DiskImageStore::IsSameFile (entry.path, job.path);
        canReplace = !entry.image->IsDirty() || (!isSameFile && m_store.m_save.IsRecoveryCurrent (entry));
    }

    if (!canReplace)
    {
        job.diagnosis.failure       = MountFailure::UnsavedWrites;
        job.diagnosis.saveError     = job.outcome.saveError;
        job.diagnosis.keptPath      = entry.path;
        job.diagnosis.isSameFile    = isSameFile;
        job.diagnosis.occupiedDrive = entry.drive;
        job.diagnosis.wasReported   = entry.sharedState.IsSaveFailureReported();

        entry.sharedState.MarkSaveFailureReported (string());
    }

    CBREx (canReplace, FAILED (job.outcome.saveError) ? job.outcome.saveError : E_PENDING);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  Production mount path. ONE FILE, ONE DRIVE, and declined before a byte is
//  read -- the bytes are not what is wrong with it. Two bays on one file each
//  have a separate DiskImage, and a save writes the whole image, so from the
//  guest's first write each drive overwrites whatever the other saved. One
//  external change then raises the conflict twice, writes two rescue copies
//  and puts two dialogs up in a row.
//
//  THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THIS. Re-mounting is how a
//  machine switch and a reload put the disk back, so the bay being mounted
//  into is the one bay this does not look at.
//
//  THE FILE IN THE BAY IS SAVED BEFORE IT IS READ. Reading first would load
//  the file without the writes the save is about to put in it, and the next
//  save would write that older disk back over them. When nothing is unsaved
//  after the save and the file is as the bay last read or wrote it, the
//  mounted image is the file, so it stays: nothing is read or swapped, and
//  the history does not get a media change.
//
//  The saves, the stat and the read run on the I/O queue, and done runs when
//  the mount is over.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::Mount (int slot, int drive, const string & path, MountDone done)
{
    HRESULT               hr        = S_OK;
    shared_ptr<MountJob>  job       = std::make_shared<MountJob>();
    bool                  isValid   = DiskImageStore::IsValidBay (slot, drive);
    bool                  elsewhere = false;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    job->slot       = slot;
    job->drive      = drive;
    job->path       = path;
    job->isFromFile = true;
    job->done       = std::move (done);

    CBRAEx (isValid, E_INVALIDARG);

    elsewhere = m_store.IsFileInAnotherBay (path, slot, drive, job->diagnosis.occupiedDrive);

    CBRFEx (!elsewhere, E_INVALIDARG,
            job->diagnosis.failure = MountFailure::AlreadyMounted);

    hr = DiskImageStore::GetSourceFormatByExtension (path, job->format);
    CHRF (hr, job->diagnosis.failure = MountFailure::UnknownExtension);

    job->diagnosis.format = job->format;
    job->isSameFile       = m_store.GetEntry (slot, drive).mounted && DiskImageStore::IsSameFile (m_store.GetEntry (slot, drive).path, path);

    if (job->isSameFile)
    {
        SaveBeforeReplace (job, [this, job] (HRESULT hrSave) { CheckSameFile (job, hrSave); });
    }
    else
    {
        ReadMountFile (job);
    }

Error:
    if (FAILED (hr))
    {
        FinishMount (job, hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckSameFile
//
//  After the save before a remount of the bay's file: the file is stat'd on
//  the I/O queue, and a file that matches the bay's identity, over a clean
//  disk, is not read again.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::CheckSameFile (const shared_ptr<MountJob> & job, HRESULT hr)
{
    DiskFileOps  ops = m_store.GetFileOps();



    if (FAILED (hr))
    {
        FinishMount (job, hr);
    }
    else
    {
        m_store.m_ioQueue->Post ([job, ops] { job->identity = ops.ReadIdentity (job->path); },
                         [this, job]
        {
            Entry  & entry       = m_store.GetEntry (job->slot, job->drive);
            bool     isUnchanged = entry.mounted && entry.image != nullptr
                                && !entry.image->IsDirty()
                                && entry.sharedState.GetIdentity().recorded
                                && entry.sharedState.GetIdentity().Matches (job->identity);



            if (isUnchanged)
            {
                FinishMount (job, S_OK);
            }
            else
            {
                ReadMountFile (job);
            }
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMountFile
//
//  The file's bytes, read on the I/O queue, and its identity stat'd after the
//  read, matching the command line's read-then-stamp order. A stamp taken
//  before the read could be of a file the loaded bytes did not come from.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::ReadMountFile (const shared_ptr<MountJob> & job)
{
    DiskFileOps  ops = m_store.GetFileOps();



    m_store.m_ioQueue->Post ([job, ops]
    {
        job->result = ops.Read (job->path, job->bytes);

        if (SUCCEEDED (job->result))
        {
            job->identity = ops.ReadIdentity (job->path);
        }
    },
    [this, job]
    {
        if (FAILED (job->result))
        {
            job->diagnosis.failure = MountFailure::FileUnreadable;
            FinishMount (job, job->result);
        }
        else
        {
            ReplaceBay (job, [this, job] (HRESULT hrReplace) { FinishMountFromFile (job, hrReplace); });
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishMountFromFile
//
//  The identity is set on the bay only on success, so after a failed mount
//  an empty bay has no stamp. Mount registers the watch: it is orchestration
//  rather than platform work, so it lives here and not in whatever built the
//  watcher. The bay change is emitted here rather than by the caller, so a
//  command-line mount, a picker mount and a machine-switch remount all react
//  alike, and the media change after it, because reverse execution
//  snapshots the machine there and the drive's head state must already be
//  what the guest runs on.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::FinishMountFromFile (const shared_ptr<MountJob> & job, HRESULT hr)
{
    if (SUCCEEDED (hr))
    {
        m_store.GetEntry (job->slot, job->drive).sharedState.Mount (job->identity);
        m_store.m_reload.BeginWatching (job->slot, job->drive);
        m_store.EmitBayChange (job->slot, job->drive, BayChange::Inserted);
        m_store.NotifyMediaChanged();
    }

    FinishMount (job, hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishMount
//
//  A file mount is over: a guest copy the save moved is reported, and done
//  runs with the result.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::FinishMount (const shared_ptr<MountJob> & job, HRESULT hr)
{
    if (DiskImageStore::IsValidBay (job->slot, job->drive))
    {
        NoteKeptDisk (*job, hr);
        ReportReplaceOutcome (job->slot, job->drive, job->path, hr, job->outcome);
    }

    job->result = hr;

    if (job->done)
    {
        job->done (hr, job->diagnosis);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount, waiting
//
//  The mount above, for a caller that uses the result before it returns.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::Mount (int slot, int drive, const string & path,
                               MountDiagnosis & outDiagnosis)
{
    HRESULT                     hr        = S_OK;
    shared_ptr<bool>            isDone    = std::make_shared<bool> (false);
    shared_ptr<HRESULT>         result    = std::make_shared<HRESULT> (S_OK);
    shared_ptr<MountDiagnosis>  diagnosis = std::make_shared<MountDiagnosis>();



    Mount (slot, drive, path, [isDone, result, diagnosis] (HRESULT hrMount, const MountDiagnosis & mounted)
    {
        *result    = hrMount;
        *diagnosis = mounted;
        *isDone    = true;
    });

    hr = m_store.WaitForIo (*isDone);
    CHR (hr);

    hr = *result;
    CHR (hr);

Error:
    outDiagnosis = *diagnosis;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMount::LoadImage
//
//  The image the bytes make, built apart from every bay, so the bay is as it
//  was when the file does not load. On failure, outDiagnosis indicates why.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::LoadImage (DiskFormat fmt, const vector<Byte> & bytes, const string & path,
                                   unique_ptr<DiskImage> & outImage, MountDiagnosis & outDiagnosis)
{
    HRESULT  hr       = S_OK;
    bool     isLoaded = false;



    outImage = make_unique<DiskImage>();
    outImage->LoadFromBytes (fmt, bytes, path);

    isLoaded = outImage->IsLoaded();

    if (!isLoaded)
    {
        outImage.reset();
        outDiagnosis = DiskImageStore::ClassifyLoadFailure (fmt, bytes);
    }

    CBR (isLoaded);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMount::SwapInImage
//
//  Puts a loaded image in a bay. The disk that was there is retired, its
//  watch dropped first because the watch is keyed by its directory, and the
//  salvage question is settled once, here, where the cost is paid by a mount
//  the user already asked for. Undamaged images settle it for free; a damaged
//  one is decoded once instead of on every menu draw.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::SwapInImage (Entry & entry, unique_ptr<DiskImage> loaded,
                                  const string & path, DiskFormat fmt)
{
    SalvageAssessment  assessment;
    HRESULT            hrAssess = S_OK;



    if (entry.mounted)
    {
        m_store.m_reload.EndWatching (entry.slot, entry.drive);
        m_store.m_media.RetireBay (entry);
    }

    entry.image   = std::move (loaded);
    entry.path    = path;
    entry.format  = fmt;
    entry.mounted = true;

    hrAssess             = m_store.m_salvage.AssessSalvage (entry.slot, entry.drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    m_store.MarkStatusChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMount::ReportReplaceOutcome
//
//  Reports a guest copy the save before a mount moved off a file another
//  program changed. The text depends on how the mount ended: the same file
//  reloaded, another file inserted, or no swap at all, with the bay on the
//  copy.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMount::ReportReplaceOutcome (int slot, int drive, const string & newPath, HRESULT hr,
                                           const FlushOutcome & outcome)
{
    ChangePrompt  prompt;



    if (outcome.movedTo.empty() || !m_store.m_reportSink)
    {
        return;
    }

    if (FAILED (hr))
    {
        prompt = ChangePrompt::ComposeConflictReport (outcome.movedFrom, drive, outcome.movedTo);
    }
    else if (DiskImageStore::IsSameFile (outcome.movedFrom, newPath))
    {
        prompt = ChangePrompt::ComposeReloadReport (outcome.movedFrom, drive, false, m_store.m_machineName,
                                                    ChangeAuthor::AnotherProgram, outcome.movedTo, true);
    }
    else
    {
        prompt = ChangePrompt::ComposeReplacedReport (outcome.movedFrom, drive, outcome.movedTo, newPath);
    }

    m_store.m_reportSink (slot, drive, prompt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  Production mount path. Reads the file from the host filesystem then
//  routes through the appropriate loader by extension.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::Mount (int slot, int drive, const string & path)
{
    MountDiagnosis  ignored;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    return Mount (slot, drive, path, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountRestored
//
//  Puts a disk saved in a machine state back in its bay: the image is built
//  from the bytes the state carries, never read from the file, so the guest
//  sees what it saw at the save even if the file has changed since. The
//  bay still answers to the file's path, which is where a later guest write
//  is flushed, and the file is watched from here like any mounted image.
//
//  THE IMAGE IS LOADED FIRST, so the bay is as it was when the bytes do not
//  load. A disk in the bay is retired without a save, so the caller saves
//  first, and the swap does not happen while a guest write is in no file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMount::MountRestored (
    int                    slot,
    int                    drive,
    const string        &  path,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    HRESULT                hr           = S_OK;
    bool                   isValid      = DiskImageStore::IsValidBay (slot, drive);
    bool                   isSafeToSwap = false;
    unique_ptr<DiskImage>  loaded;
    MountDiagnosis         diagnosis;
    ImageIdentity          identity;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    CBRAEx (isValid, E_INVALIDARG);

    hr = LoadImage (fmt, bytes, path, loaded, diagnosis);
    CHR (hr);

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        isSafeToSwap = m_store.m_save.AreWritesSaved (entry);
        CBREx (isSafeToSwap, E_UNEXPECTED);

        SwapInImage (entry, std::move (loaded), path, fmt);
    }

    identity = m_store.ReadIdentity (path);

    if (identity.recorded)
    {
        m_store.GetEntry (slot, drive).sharedState.Mount (identity);
        m_store.m_reload.BeginWatching (slot, drive);
    }

    m_store.EmitBayChange (slot, drive, BayChange::Inserted);
    m_store.NotifyMediaChanged();

Error:
    return hr;
}




