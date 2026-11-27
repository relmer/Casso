#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"

struct FieldMarks;
struct FramedNibble;
struct FramedTrack;





////////////////////////////////////////////////////////////////////////////////
//
//  MarkDiscovery
//
//  Finds the address and data prologs a track uses when no mark list has
//  them. An address prolog is three nibbles followed by a 4-and-4 body whose
//  checksum matches and whose track is the physical track; a data prolog is
//  three nibbles after such a field, followed by a 6-and-2 or 5-and-3 body
//  whose checksum matches. The data field's encoding sets whether the
//  address prolog is a 16-sector or a 13-sector one. A prolog found fewer
//  than kMinMatches times is left out, so one chance match in a track's data
//  does not become a mark.
//
////////////////////////////////////////////////////////////////////////////////

class MarkDiscovery
{
public:
    static constexpr int  kMinMatches = 2;

    static void  Discover (const FramedTrack & track, int physicalTrack, FieldMarks & outMarks);

private:
    using Prolog = std::array<Byte, 3>;

    struct DataHit
    {
        Prolog         prolog = {};
        DiskFieldKind  kind   = DiskFieldKind::Sixteen;

        auto  operator<=> (const DataHit & other) const = default;
    };

    struct Field
    {
        Prolog           address = {};
        vector<DataHit>  hits;
    };

    static Byte  GetNibble        (const vector<FramedNibble> & nibbles, size_t index);
    static bool  IsAddressBodyAt  (const vector<FramedNibble> & nibbles, size_t index, int physicalTrack);
    static void  FindData         (const vector<FramedNibble> & nibbles, size_t from, vector<DataHit> & outHits);
    static bool  IsDataBodyAt     (const vector<FramedNibble> & nibbles, size_t index, DiskFieldKind kind);
    static void  AddMark          (vector<DiskMarkPattern> & ioMarks, const Prolog & nibbles);
};
