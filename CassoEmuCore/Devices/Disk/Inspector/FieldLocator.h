#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/Inspector/LatchFramer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FieldMarks
//
//  The marks the locator matches. Standard by default; the inspector's decode
//  settings add custom ones. Every list is tried in order, and a field records
//  the nibbles it actually found.
//
////////////////////////////////////////////////////////////////////////////////

struct FieldMarks
{
    vector<DiskMarkPattern>  address16;
    vector<DiskMarkPattern>  address13;
    vector<DiskMarkPattern>  data;
    vector<DiskMarkPattern>  addressEpilogue;
    vector<DiskMarkPattern>  dataEpilogue;

    static FieldMarks  MakeStandard();
};





////////////////////////////////////////////////////////////////////////////////
//
//  LocatedField
//
//  One address or data field on a framed track. Nibble indices are into the
//  track's nibble list and wrap at its end, so a field that spans the index is
//  whole. An address field's checksum is the exclusive OR of volume, track
//  and sector; a data field's is the chain's (DiskFieldFormat).
//
////////////////////////////////////////////////////////////////////////////////

enum class FieldRole
{
    Address,
    Data,
};


struct LocatedField
{
    FieldRole                                       role                  = FieldRole::Address;
    DiskFieldKind                                   kind                  = DiskFieldKind::Sixteen;
    int                                             firstNibble           = 0;
    int                                             nibbleCount           = 0;
    int                                             bodyNibble            = 0;
    uint32_t                                        startCell             = 0;
    uint32_t                                        endCell               = 0;
    std::array<Byte, DiskMarkPattern::kMaxNibbles>  prologueFound         = {};
    std::array<Byte, DiskMarkPattern::kMaxNibbles>  epilogueFound         = {};
    bool                                            isEpilogueStandard    = false;
    bool                                            hasEpilogueTail       = false;
    Byte                                            volume                = 0;
    Byte                                            track                 = 0;
    Byte                                            sector                = 0;
    Byte                                            checksumStored        = 0;
    Byte                                            checksumComputed      = 0;
    bool                                            isAddressChecksumGood = false;
    DataFieldDecode                                 data;
    bool                                            hasNoise              = false;
    int                                             pairedField           = -1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FieldLocator
//
//  Finds every address field and every data field on a framed track and pairs
//  them. A data field pairs with the address field before it when its prologue
//  starts within the search window after that address field and before the
//  next address prologue; it pairs with one address field at most. A data
//  field with no address field takes the encoding whose table holds more of
//  its nibbles.
//
////////////////////////////////////////////////////////////////////////////////

class FieldLocator
{
public:
    static constexpr int  kDataSearchWindowNibbles = 48;

    static void  Locate (const FramedTrack & track, const FieldMarks & marks, vector<LocatedField> & outFields);

private:
    static Byte  GetNibble          (const FramedTrack & track, int index);
    static bool  MatchesAny         (const FramedTrack & track, int index, const vector<DiskMarkPattern> & patterns, int & outLength);
    static bool  IsAddressAt        (const FramedTrack & track, int index, const FieldMarks & marks, DiskFieldKind & outKind, int & outLength);
    static void  ReadEpilogue       (const FramedTrack & track, int index, const vector<DiskMarkPattern> & patterns, LocatedField & inOutField);
    static void  ReadAddressField   (const FramedTrack & track, int index, int prologueLength, LocatedField & outField);
    static void  ReadDataField      (const FramedTrack & track, int index, int prologueLength, DiskFieldKind kind, LocatedField & outField);
    static void  ChooseOrphanKind   (const FramedTrack & track, const FieldMarks & marks, LocatedField & inOutField);
    static void  SetCellSpan        (const FramedTrack & track, LocatedField & inOutField);
    static void  PairFields         (const FramedTrack & track, const vector<int> & addressStarts, vector<LocatedField> & inOutFields);
};
