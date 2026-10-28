#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"


class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMedia
//
//  The disks kept in memory after they leave their bays, so a snapshot
//  taken while one was in a drive can put it back.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreMedia
{
public:
    using Entry = DiskBayEntry;

    explicit DiskStoreMedia (DiskImageStore & store) : m_store (store) {}

    //  DiskImageStore's media retention and seating.
    void           SetMediaRetention  (bool isOn);
    bool           IsRetaining        () const { return m_isRetaining; }
    size_t         GetRetainedCount   () const { return m_retained.size(); }
    void           SetPositionSource  (const uint64_t * source) { m_positionSource = source; }
    bool           CanSeatMedia       (int slot, int drive, uint64_t mediaId) const;
    HRESULT        SeatMedia          (int slot, int drive, uint64_t mediaId, bool & outChanged);
    void           ReportSeatedMedia  (int slot, int drive, uint64_t previousMediaId);
    void           PruneRetainedMedia (uint64_t oldestPosition);

    //  Moves a bay's disk into the kept list (or drops it without retention)
    //  and leaves the bay empty. No flush, no bay change.
    void           RetireBay (Entry & entry);

private:
    DiskImageStore &  m_store;

    std::vector<Entry>       m_retained;
    const uint64_t *         m_positionSource = nullptr;
    bool                     m_isRetaining    = false;
};
