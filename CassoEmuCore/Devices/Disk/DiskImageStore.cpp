#include "Pch.h"

#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Store/DiskSourceFormat.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  Test-friendly mount path that bypasses the host filesystem. The
//  virtualPath is used purely as the round-trip identifier; production
//  code uses Mount() which reads from disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    MountDiagnosis  ignored;



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
//  does, so a controller attached by that sink reads the new image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT   hr = S_OK;



    hr = LoadIntoBay (slot, drive, virtualPath, fmt, bytes, outDiagnosis);
    CHR (hr);

    EmitBayChange (slot, drive, BayChange::Inserted);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadIntoBay
//
//  Replaces a bay's image with one loaded from bytes, flushing the old one
//  first. When the loader cannot load the bytes, the bay ends empty.
//
//  THE OLD IMAGE IS FREED BEFORE THE LOAD, so a bay that had a disk and ends
//  empty calls the bay-change sink with Ejected. Without that call, a
//  controller attached to the old image reads freed memory after a failed
//  remount.
//
//  A WOZ image of any disk but a 5.25-inch one is refused before the bay is
//  touched, as a file in another bay is: the bay keeps its disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::LoadIntoBay (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT                 hr         = S_OK;
    bool                    wasMounted = false;
    bool                    is525      = true;
    WozLoader::Description  about;



    outDiagnosis              = MountDiagnosis();
    outDiagnosis.format       = fmt;
    outDiagnosis.fileByteSize = bytes.size();

    CBRAEx (slot >= 0 && slot < DiskBayTable::kSlotCount && drive >= 0 && drive < DiskBayTable::kDriveCount, E_INVALIDARG);

    if (fmt == DiskFormat::Woz)
    {
        WozLoader::Describe (bytes, about);

        //  A file that isn't a WOZ at all fails in the loader, with its reason.
        is525 = !about.isWoz || about.diskType == WozLoader::kDiskType525;
    }

    CBRFEx (is525, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT), outDiagnosis.failure = MountFailure::WozNot525);

    {
        DiskBay &   entry = m_bays.GetBay (slot, drive);

        wasMounted = entry.mounted;

        //  The watch is keyed by the old image's directory, so it is dropped
        //  before the path changes; Mount watches the new one on success.
        if (wasMounted)
        {
            hr = m_flusher.FlushEntry (entry, FlushMoment::Running);
            IGNORE_RETURN_VALUE (hr, S_OK);

            m_watches.EndWatching (slot, drive);
        }

        entry.image   = make_unique<DiskImage> ();
        entry.path    = virtualPath;
        entry.format  = fmt;
        entry.mounted = true;

        entry.image->LoadFromBytes (fmt, bytes, virtualPath);

        // A format the loader rejects leaves the slot empty rather than
        // half-mounted.
        if (!entry.image->IsLoaded())
        {
            entry.image.reset();
            entry.path.clear();
            entry.mounted        = false;
            entry.salvageOffered = false;
            outDiagnosis         = DiskSourceFormat::ClassifyLoadFailure (fmt, bytes);
            hr = E_FAIL;

            if (wasMounted)
            {
                entry.sharedState.Eject();
                EmitBayChange (slot, drive, BayChange::Ejected);
            }
        }
        else
        {
            // Decide the salvage question once, here, where the cost is paid
            // by a mount the user already asked for. Undamaged images settle
            // it for free; a damaged one is decoded once instead of on every
            // menu draw.
            SalvageAssessment  assessment;
            HRESULT            hrAssess = m_salvager.AssessSalvage (slot, drive, assessment);

            entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  Production mount path. Reads the file from the host filesystem then
//  routes through the appropriate loader by extension.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path)
{
    MountDiagnosis  ignored;



    return Mount (slot, drive, path, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  The same mount, saying why it refused. The two failures BEFORE any loader
//  runs are settled here and nowhere else: a name no loader claims, and bytes
//  that never arrived. Neither is visible further in -- the loaders are handed
//  a buffer and a format, and by then the file name and the read are history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path,
                               MountDiagnosis & outDiagnosis)
{
    HRESULT       hr        = S_OK;
    DiskFormat    fmt       = DiskFormat::Dsk;
    bool          elsewhere = false;
    vector<Byte>  bytes;



    outDiagnosis = MountDiagnosis();

    //  ONE FILE, ONE DRIVE, and refused before a byte is read -- the bytes are
    //  not what is wrong with it. Two bays on one file each hold their own
    //  DiskImage, and a flush writes the whole image, so from the guest's
    //  first write each drive overwrites whatever the other saved. One
    //  external change then raises the conflict twice, writes two rescue
    //  copies and puts two dialogs up in a row.
    //
    //  THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THIS. Re-mounting is how
    //  a machine switch and a reload put the disk back, so the bay being
    //  mounted into is the one bay this does not look at.
    elsewhere = m_bays.IsFileInAnotherBay (path, slot, drive, outDiagnosis.occupiedDrive);

    CBRFEx (!elsewhere, E_INVALIDARG,
            outDiagnosis.failure = MountFailure::AlreadyMounted);

    hr = DiskSourceFormat::GetSourceFormatByExtension (path, fmt);
    CHRF (hr, outDiagnosis.failure = MountFailure::UnknownExtension);

    outDiagnosis.format = fmt;

    hr = m_files.ReadImageFile (path, bytes);
    CHRF (hr, outDiagnosis.failure = MountFailure::FileUnreadable);

    hr = LoadIntoBay (slot, drive, path, fmt, bytes, outDiagnosis);
    CHR (hr);

    //  Recorded AFTER the read and only on success, matching the command
    //  line's read-then-stamp order. A stamp taken before the read could
    //  describe a file the loaded bytes did not come from, and a stamp
    //  recorded for a mount that failed would sit on an empty bay.
    m_bays.GetBay (slot, drive).sharedState.Mount (m_files.ReadIdentity (path));

    //  Mount registers the watch. It is orchestration rather than platform
    //  work, so it lives here and not in whatever built the watcher.
    m_watches.BeginWatching (slot, drive);

    //  A disk went in. The shell wires the controller and lights the door from
    //  here rather than from the path that called Mount, so a command-line
    //  mount and a picker mount and a machine-switch remount all react alike.
    EmitBayChange (slot, drive, BayChange::Inserted);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsSalvageOffered (int slot, int drive) const
{
    if (!DiskBayTable::IsValidBay (slot, drive))
    {
        return false;
    }

    return m_bays.GetBay (slot, drive).salvageOffered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
//  Auto-flush dirty before releasing. Honors FR-025.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive)
{
    HRESULT   hr = S_OK;



    // An out-of-range bay and an empty one are both nothing to eject.
    if (DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).mounted)
    {
        DiskBay &   entry = m_bays.GetBay (slot, drive);

        // Flush failures are reported to the user by FlushEntry itself; the
        // eject proceeds either way, because refusing to unmount would leave
        // the user with no way to get the disk out.
        hr = m_flusher.FlushEntry (entry, FlushMoment::Ejecting);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  EXCEPT WHEN THE FLUSH PUT A QUESTION UP, which it does only when
        //  the guest's version has nowhere to go and this eject is what would
        //  destroy it. The disk stays until the answer comes, and the answer
        //  finishes what this call started -- both of the answers it offers
        //  end with the drive empty, so nothing is stuck.
        if (entry.sharedState.IsEjectWhenAnswered())
        {
            return;
        }

        //  Before the path goes: the watch is keyed by the directory the
        //  image sits in, and there is no finding that from a cleared path.
        m_watches.EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted = false;
        entry.sharedState.Eject();

        //  The disk left. Emitted after the bay is empty, so the handler that
        //  detaches the controller sees the post-eject state.
        EmitBayChange (slot, drive, BayChange::Ejected);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  FR-034 / Phase 4 contract: keep mounts mounted, flush every dirty
//  image so a soft reset never loses user writes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SoftReset()
{
    HRESULT   hr = m_flusher.FlushAll();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  FR-035 / data-model.md: unmount everything, flushing dirty as we go.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PowerCycle()
{
    int   slot  = 0;
    int   drive = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            Eject (slot, drive);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetImage / IsMounted / GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * DiskImageStore::GetImage (int slot, int drive)
{
    // Null for a bad bay is the same answer as for an empty one: no image.
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMediaId
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskImageStore::GetMediaId (int slot, int drive) const
{
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).mediaId : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteStateRestored
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteStateRestored (int slot, int drive)
{
    if (DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).image != nullptr)
    {
        m_bays.GetBay (slot, drive).mediaId = m_bays.TakeNextMediaId();
        EmitBayChange (slot, drive, BayChange::Restored);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMounted
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMounted (int slot, int drive) const
{
    return DiskBayTable::IsValidBay (slot, drive) && m_bays.GetBay (slot, drive).mounted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

const string & DiskImageStore::GetSourcePath (int slot, int drive) const
{
    // Returns a reference, so a bad bay yields the member empty string rather
    // than a temporary.
    return DiskBayTable::IsValidBay (slot, drive) ? m_bays.GetBay (slot, drive).path : m_emptyPath;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMountedSourcePaths
//
//  Every mounted entry's backing path with its bay. Entries mounted from
//  bytes with an empty virtual path are skipped -- there is no host file to
//  collide with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskImageStore::MountedSource> DiskImageStore::GetMountedSourcePaths() const
{
    std::vector<MountedSource>  result;
    int                         slot   = 0;
    int                         drive  = 0;



    for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
        {
            const DiskBay &  entry = m_bays.GetBay (slot, drive);

            if (entry.mounted && !entry.path.empty())
            {
                result.push_back (MountedSource{ entry.path, slot, drive });
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState
//
//  What a bay knows about its image beyond the bytes.
//
//  NULL FOR AN OUT-OF-RANGE BAY rather than a reference to a shared empty, so a
//  caller cannot write into a placeholder and believe it recorded something.
//
////////////////////////////////////////////////////////////////////////////////

MountedImageState * DiskImageStore::GetSharedState (int slot, int drive)
{
    MountedImageState *  state = nullptr;



    if (DiskBayTable::IsValidBay (slot, drive))
    {
        state = &m_bays.GetBay (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState  (const)
//
//  The same answer for a caller that only reads it.
//
////////////////////////////////////////////////////////////////////////////////

const MountedImageState * DiskImageStore::GetSharedState (int slot, int drive) const
{
    const MountedImageState *  state = nullptr;



    if (DiskBayTable::IsValidBay (slot, drive))
    {
        state = &m_bays.GetBay (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EmitBayChange
//
//  Tells the shell a bay's disk changed.
//
//  EVERY MUTATION PATH ENDS HERE. Mount, user eject, pick-up, lost file -- the
//  door and its sounds are lit from this one call, so a new path that moves a
//  disk cannot forget to. A null sink is a headless or test session with no
//  drive on screen, and does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EmitBayChange (int slot, int drive, BayChange change)
{
    if (slot >= 0 && slot < DiskBayTable::kSlotCount && drive >= 0 && drive < DiskBayTable::kDriveCount)
    {
        m_bays.GetBay (slot, drive).mediaId = m_bays.TakeNextMediaId();
    }

    if (m_hooks.bayChangeSink)
    {
        m_hooks.bayChangeSink (slot, drive, change);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountExternallyModifiedDisk
//
//  Swaps a mounted image's contents for the ones on disk.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE POINT. The ordinary remount path
//  flushes what the bay held before loading, which here would write the old
//  disk straight over the change that prompted the swap.
//
//  THE NEW BYTES LOAD INTO A FRESH IMAGE FIRST, so a load that fails leaves the
//  mounted disk exactly as it was. The machine is running and what it holds is
//  known-good; there is no version of this worth half-doing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes)
{
    HRESULT                  hr         = S_OK;
    DiskBay                & entry      = m_bays.GetBay (slot, drive);
    SalvageAssessment        assessment;
    HRESULT                  hrAssess   = S_OK;
    bool                     usable     = false;
    unique_ptr<DiskImage>    loaded     = make_unique<DiskImage> ();



    loaded->LoadFromBytes (entry.format, bytes, entry.path);

    usable = loaded->IsLoaded();
    CBR (usable);

    //  THE CONTENTS MOVE, THE OBJECT STAYS. The controller holds a raw pointer
    //  to this DiskImage -- SetExternalDisk hands one over at mount -- so
    //  replacing the unique_ptr would leave the drive reading freed memory the
    //  instant a disk was reloaded. Assigning through keeps the address the
    //  drive was given.
    *entry.image = std::move (*loaded);

    //  The identity is refreshed from the file the bytes came from, so the
    //  swap does not immediately look like another external change.
    entry.sharedState.SetIdentity (m_files.ReadIdentity (entry.path));
    entry.mediaId = m_bays.TakeNextMediaId();

    hrAssess             = m_salvager.AssessSalvage (slot, drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    //  The disk in the drive was replaced under the running machine. Swapped,
    //  not Inserted: the door opens and closes rather than only closing,
    //  because from the user's seat a disk came out and another went in.
    EmitBayChange (slot, drive, BayChange::Swapped);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::RepointBayToFile
//
//  Moves a bay onto a different file. The disk in the drive does not change.
//
//  A SAVE-AS RATHER THAN A MOUNT. Nothing is loaded and nothing is swapped, so
//  a guest reading the drive sees no interruption; only the file this bay will
//  read and write from here on is different.
//
//  THE WATCH MOVES WITH IT, and the old file stops being this bay's business.
//  Two disks out of one folder share a watch, so the common case of a
//  timestamped copy beside its original costs nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::RepointBayToFile (int slot, int drive, const string & newPath)
{
    HRESULT  hr     = S_OK;
    bool     usable = DiskBayTable::IsValidBay (slot, drive) && !newPath.empty();



    CBR (usable);

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        CBR (entry.mounted && entry.image != nullptr);

        //  Before the path moves, while EndWatching can still tell which
        //  directory this bay was using.
        m_watches.EndWatching (slot, drive);

        entry.path = newPath;

        //  A fresh identity for the new file, and nothing pending against the
        //  old one. Mount is what records both.
        entry.sharedState.Mount (m_files.ReadIdentity (newPath));

        m_watches.BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EjectLostImage
//
//  Empties a bay whose file has gone.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE DIFFERENCE FROM Eject. The ordinary
//  eject persists a dirty image first, which here would mean writing the disk
//  back to a path the user has just been told no longer exists -- recreating
//  the file they deleted, or failing and reporting a second problem about the
//  first one.
//
//  WHATEVER THE USER WANTED SAVED IS ALREADY SAVED by the time this runs. The
//  offer comes first and this is what follows it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EjectLostImage (int slot, int drive)
{
    if (!DiskBayTable::IsValidBay (slot, drive))
    {
        return;
    }

    {
        DiskBay &  entry = m_bays.GetBay (slot, drive);

        m_watches.EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted        = false;
        entry.salvageOffered = false;
        entry.sharedState.Eject();
    }

    //  A file that vanished empties the drive the same as a user eject, and
    //  the door and its sound follow the same way.
    EmitBayChange (slot, drive, BayChange::Ejected);

    return;
}





