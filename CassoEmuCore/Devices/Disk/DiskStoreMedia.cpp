#include "Pch.h"

#include "DiskStoreMedia.h"
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
//  SetMediaRetention
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMedia::SetMediaRetention (bool isOn)
{
    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    m_isRetaining = isOn;

    if (!isOn)
    {
        m_retained.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanSeatMedia
//
////////////////////////////////////////////////////////////////////////////////

bool DiskStoreMedia::CanSeatMedia (int slot, int drive, uint64_t mediaId) const
{
    bool  canSeat = false;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    if (!DiskImageStore::IsValidBay (slot, drive))
    {
        return false;
    }

    canSeat = mediaId == 0 || m_store.GetMediaId (slot, drive) == mediaId;

    for (const Entry & kept : m_retained)
    {
        canSeat = canSeat || kept.image->GetImageId() == mediaId;
    }

    return canSeat;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeatMedia
//
//  Puts the kept disk with mediaId in the bay, keeping whatever the bay held,
//  or empties the bay for zero. Used to load a machine snapshot, so it flushes
//  nothing and reports no bay change: the disks move back to where they were,
//  with the writes they had, and nothing about the files changes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskStoreMedia::SeatMedia (int slot, int drive, uint64_t mediaId, bool & outChanged)
{
    HRESULT  hr      = S_OK;
    bool     canSeat = CanSeatMedia (slot, drive, mediaId);
    size_t   index   = 0;
    Entry    kept;



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    outChanged = false;

    CBREx (canSeat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    BAIL_OUT_IF (m_store.GetMediaId (slot, drive) == mediaId, S_OK);

    {
        Entry &  entry = m_store.GetEntry (slot, drive);

        if (entry.mounted)
        {
            m_store.m_reload.EndWatching (slot, drive);
            RetireBay (entry);
        }

        outChanged = true;

        BAIL_OUT_IF (mediaId == 0, S_OK);

        while (m_retained[index].image->GetImageId() != mediaId)
        {
            index++;
        }

        kept = std::move (m_retained[index]);
        m_retained.erase (m_retained.begin() + static_cast<ptrdiff_t> (index));

        entry.image          = std::move (kept.image);
        entry.path           = kept.path;
        entry.format         = kept.format;
        entry.mounted        = true;
        entry.salvageOffered = kept.salvageOffered;
        entry.sharedState    = kept.sharedState;

        m_store.MarkStatusChanged();
        m_store.m_reload.BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportSeatedMedia
//
//  SeatMedia emits no bay change, which suits a step through history. When
//  the shell's drives have to match the seated disks, as after a state load
//  that failed, the caller emits the change here: an eject for a bay left
//  empty, and an insert for a bay holding a disk it did not hold before, as
//  a mount over an occupied bay emits.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMedia::ReportSeatedMedia (
    int       slot,
    int       drive,
    uint64_t  previousMediaId)
{
    uint64_t  mediaId = m_store.GetMediaId (slot, drive);



    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    if (!DiskImageStore::IsValidBay (slot, drive) || mediaId == previousMediaId)
    {
        return;
    }

    m_store.EmitBayChange (slot, drive, (mediaId == 0) ? BayChange::Ejected : BayChange::Inserted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PruneRetainedMedia
//
//  A disk that left its bay before the oldest snapshot is in none of them,
//  so nothing can put it back. One that left at the oldest snapshot's
//  position can still be in it: a changed file is taken up from the drive's
//  tick, partway through an instruction and before the position counts that
//  instruction, so the disk it replaces leaves at the position of the
//  snapshot taken at the instruction's start.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMedia::PruneRetainedMedia (uint64_t oldestPosition)
{
    ASSERT_THREAD_OWNERSHIP (m_store.m_ownership);

    std::erase_if (m_retained, [oldestPosition] (const Entry & kept) { return kept.retiredAt < oldestPosition; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RetireBay
//
//  Empties a bay. With retention on, its disk is kept with everything the bay
//  knew about it, stamped with the position it left at.
//
////////////////////////////////////////////////////////////////////////////////

void DiskStoreMedia::RetireBay (Entry & entry)
{
    Entry  kept;



    if (m_isRetaining && entry.image != nullptr)
    {
        kept.image          = std::move (entry.image);
        kept.path           = entry.path;
        kept.format         = entry.format;
        kept.mounted        = true;
        kept.salvageOffered = entry.salvageOffered;
        kept.sharedState    = entry.sharedState;
        kept.retiredAt      = (m_positionSource != nullptr) ? *m_positionSource : 0;

        m_retained.push_back (std::move (kept));
    }

    entry.image.reset();
    entry.path.clear();
    entry.mounted        = false;
    entry.salvageOffered = false;
    entry.sharedState.Eject();

    m_store.MarkStatusChanged();
}




