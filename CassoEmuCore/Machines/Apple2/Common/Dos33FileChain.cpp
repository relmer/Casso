#include "Pch.h"

#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Devices/Disk/ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChain::Dos33FileChain
//
////////////////////////////////////////////////////////////////////////////////

Dos33FileChain::Dos33FileChain (const Dos33SectorAccess & access)
    : m_access (access)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChain::CollectDataSectors
//
//  Walks a file's track/sector list, gathering its data sectors in order.
//
//  Both the list chain and the data sectors go through the guard, so a file
//  whose list points back into itself -- or at another file's list -- stops
//  rather than spinning. Returns false when the walk hit a bound.
//
//  AN ENTRY THAT CLAIMS NO SECTORS IS NOT A BROKEN CHAIN. Real disks carry
//  catalog entries that occupy no storage at all -- Merlin Pro's own disk uses
//  them to draw section headings in CATALOG, with a sector count of zero and a
//  track/sector pointer of $7F/$7F that was never meant to be followed. DOS
//  lists them without complaint, so a reader that called them damaged would
//  report a shipped disk as broken.
//
//  The discriminator is the entry's own sector count, not the pointer: zero
//  sectors means the entry declares it occupies nothing, so there is nothing to
//  reach and nothing lost. A pointer that leads nowhere while the entry claims
//  sectors IS damage, and stays reported.
//
//  A zero track/sector pair is a sparse hole, which DOS 3.3 writes for a file
//  with gaps. It is kept in the list and read back as zeros, because that is
//  what the guest sees.
//
////////////////////////////////////////////////////////////////////////////////

bool Dos33FileChain::CollectDataSectors (
    const Dos33CatalogEntry  & entry,
    vector<uint32_t>         & outUnits,
    vector<uint32_t>         & outListUnits,
    ChainWalkGuard           & guard) const
{
    int   listTrack  = entry.tsListTrack;
    int   listSector = entry.tsListSector;
    int   pair       = 0;
    bool  ok         = true;



    // An entry occupying no sectors has no chain, which is a fact about the
    // entry rather than a failure to read one.
    if (entry.sectorCount == 0)
    {
        return true;
    }

    while (ok && listTrack != 0)
    {
        bool  stepOk = guard.TryVisit (m_access.ToUnit (listTrack, listSector));

        if (!stepOk || listTrack >= m_access.GetGeometry().tracks
                    || listSector >= m_access.GetGeometry().sectorsPerTrack)
        {
            ok = false;
            break;
        }

        // EVERY list sector, not merely the first. A file large enough to need
        // a second list occupies that sector as surely as it occupies its data,
        // and an accounting that saw only the head left the rest allocated with
        // nothing owning it -- which a delete would then fail to return.
        outListUnits.push_back (m_access.ToUnit (listTrack, listSector));

        for (pair = 0; pair < kTsPairsPerList; pair++)
        {
            size_t  at    = kTsOffFirstPair + (size_t) pair * 2;
            Byte    dataT = m_access.ReadByte (listTrack, listSector, at);
            Byte    dataS = m_access.ReadByte (listTrack, listSector, at + 1);

            if (dataT == 0 && dataS == 0)
            {
                continue;   // sparse hole -- reads as zeros
            }

            if (dataT >= m_access.GetGeometry().tracks
             || dataS >= m_access.GetGeometry().sectorsPerTrack)
            {
                ok = false;
                break;
            }

            outUnits.push_back (m_access.ToUnit (dataT, dataS));
        }

        // Both halves of the next pointer come from the sector being left, and
        // must be read BEFORE either is overwritten. Taking the sector half
        // from the file's head instead walks correctly on the first hop -- the
        // head IS the current sector there -- and then loops on the second,
        // which reports a healthy file as an unfollowable chain.
        {
            int  nextTrack  = m_access.ReadByte (listTrack, listSector, kTsOffNextTrack);
            int  nextSector = m_access.ReadByte (listTrack, listSector, kTsOffNextSector);

            listTrack  = nextTrack;
            listSector = nextSector;
        }
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChain::ZeroUnits
//
//  Every sector a new file occupies is cleared before anything is written into
//  it. A track/sector list still holding a previous file's pairs would be read
//  as this file's data, and the tail of a partly filled last sector would hand
//  back bytes nobody ever wrote.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33FileChain::ZeroUnits (vector<Byte> & buffer, const vector<uint32_t> & units) const
{
    for (uint32_t unit : units)
    {
        size_t  at = m_access.GetGeometry().GetSectorOffset (m_access.GetTrack (unit), m_access.GetSector (unit));

        if (at + (size_t) NibbleFormat::kSectorByteSize <= buffer.size())
        {
            std::fill (buffer.begin() + (ptrdiff_t) at,
                       buffer.begin() + (ptrdiff_t) (at + NibbleFormat::kSectorByteSize),
                       (Byte) 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileChain::PlaceFile
//
//  Lays the allocated sectors out as DOS does: a track/sector list, then the
//  data sectors it describes, then the next list, and so on. The allocator hands
//  the units over in exactly that order, so this walks them straight through.
//
//  Each list records the file-relative index of the first data sector it
//  describes, which is what lets a reader joining the chain partway know where
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33FileChain::PlaceFile (
    vector<Byte>            & buffer,
    const vector<uint32_t>  & units,
    size_t                    dataSectorCount,
    const vector<Byte>      & stored) const
{
    size_t  unitCount  = units.size();
    size_t  cursor     = 0;
    size_t  placed     = 0;
    size_t  storedSize = stored.size();



    while (cursor < unitCount)
    {
        uint32_t  listUnit   = units[cursor];
        int       listTrack  = m_access.GetTrack (listUnit);
        int       listSec    = m_access.GetSector (listUnit);
        size_t    remaining  = dataSectorCount - placed;
        size_t    inThisList = std::min ((size_t) kTsPairsPerList, remaining);
        size_t    k          = 0;

        cursor++;

        m_access.WriteByteAt (buffer, listTrack, listSec, kTsOffSectorOffsetLo, (Byte) (placed & 0xFF));
        m_access.WriteByteAt (buffer, listTrack, listSec, kTsOffSectorOffsetHi, (Byte) ((placed >> 8) & 0xFF));

        for (k = 0; k < inThisList; k++)
        {
            uint32_t  dataUnit = units[cursor + k];
            size_t    pairAt   = kTsOffFirstPair + k * 2;
            size_t    from     = (placed + k) * (size_t) NibbleFormat::kSectorByteSize;
            size_t    to       = m_access.GetGeometry().GetSectorOffset (m_access.GetTrack (dataUnit), m_access.GetSector (dataUnit));
            size_t    left     = (from < storedSize) ? storedSize - from : 0;
            size_t    copyable = std::min ((size_t) NibbleFormat::kSectorByteSize, left);

            m_access.WriteByteAt (buffer, listTrack, listSec, pairAt,     (Byte) m_access.GetTrack (dataUnit));
            m_access.WriteByteAt (buffer, listTrack, listSec, pairAt + 1, (Byte) m_access.GetSector (dataUnit));

            std::copy (stored.begin() + (ptrdiff_t) from,
                       stored.begin() + (ptrdiff_t) (from + copyable),
                       buffer.begin() + (ptrdiff_t) to);
        }

        cursor += inThisList;
        placed += inThisList;

        if (cursor < unitCount)
        {
            m_access.WriteByteAt (buffer, listTrack, listSec, kTsOffNextTrack,  (Byte) m_access.GetTrack (units[cursor]));
            m_access.WriteByteAt (buffer, listTrack, listSec, kTsOffNextSector, (Byte) m_access.GetSector (units[cursor]));
        }
    }
}





