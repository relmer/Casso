#include "Pch.h"

#include "Ui/DiskInspector/OrphanFields.h"
#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OrphanFields::Find
//
////////////////////////////////////////////////////////////////////////////////

vector<int> OrphanFields::Find (const TrackAnalysis & track)
{
    vector<int>  orphans;
    bool         isPaired = false;



    for (size_t f = 0; f < track.fields.size(); f++)
    {
        isPaired = false;

        for (const AnalyzedSector & sector : track.sectors)
        {
            isPaired = isPaired || sector.dataField == static_cast<int> (f);
        }

        if (track.fields[f].role == FieldRole::Data && !isPaired)
        {
            orphans.push_back (static_cast<int> (f));
        }
    }

    return orphans;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OrphanFields::FindAt
//
////////////////////////////////////////////////////////////////////////////////

int OrphanFields::FindAt (const TrackAnalysis & track, int nibble)
{
    int  found = -1;



    for (int f : Find (track))
    {
        const LocatedField &  field = track.fields[static_cast<size_t> (f)];

        if (found < 0 && nibble >= field.firstNibble && nibble < field.firstNibble + field.nibbleCount)
        {
            found = f;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OrphanFields::GuessSector
//
//  Address fields number a track's sectors in their order on the track,
//  so a sector that lies between two others whose numbers are two apart is
//  most likely the one between them.
//
////////////////////////////////////////////////////////////////////////////////

int OrphanFields::GuessSector (const TrackAnalysis & track, int field)
{
    int  at      = track.fields[static_cast<size_t> (field)].firstNibble;
    int  total   = std::max (static_cast<int> (track.framed.nibbles.size()), 1);
    int  before  = -1;
    int  after   = -1;
    int  nearest = INT_MAX;
    int  next    = INT_MAX;
    int  count   = 16;
    int  guess   = -1;



    //  Distances around the track, so the last sector before the index is
    //  the one before a field near its start.
    for (const AnalyzedSector & sector : track.sectors)
    {
        int  start = track.fields[static_cast<size_t> (sector.addressField)].firstNibble;
        int  back  = ((at - start) % total + total) % total;
        int  ahead = ((start - at) % total + total) % total;

        count = (sector.kind == DiskFieldKind::Sixteen) ? 16 : 13;

        if (back < nearest)
        {
            nearest = back;
            before  = sector.sector;
        }

        if (ahead < next)
        {
            next  = ahead;
            after = sector.sector;
        }
    }

    if (before >= 0 && after >= 0 && (after - before + count) % count == 2)
    {
        guess = (before + 1) % count;
    }

    return guess;
}