#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/Finding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalysis and its parts
//
//  Everything the disk inspector knows about one track record: its framed
//  nibbles with a kind for each, the fields and the sectors they make, the
//  record's class, its measurements and its findings. Built by TrackAnalyzer
//  from a TrackCopy; immutable once built.
//
////////////////////////////////////////////////////////////////////////////////

enum class NibbleKind : Byte
{
    Sync,
    AddressPrologue,
    AddressField,
    AddressEpilogue,
    DataPrologue,
    DataField,
    DataEpilogue,
    Other,
    Noise,
};


enum class SectorState
{
    Good,
    BadAddress,
    BadData,
    NotChecked,
    NoDataField,
};


//  Why "Edit sector" is unavailable for a sector, or None when it is not.
enum class EditBlock
{
    None,
    NoDataField,
    AddressChecksumFailed,
    NoiseInDataField,
    NibblesOutsideTable,
    DamagedRecord,
};


enum class TrackClass
{
    NothingRecorded,
    Damaged,
    Sixteen,
    Thirteen,
    ThirteenAndSixteen,
    Unformatted,
    Nonstandard,
};


struct AnalyzedSector
{
    int            addressField    = -1;
    int            dataField       = -1;
    Byte           sector          = 0;
    DiskFieldKind  kind            = DiskFieldKind::Sixteen;
    SectorState    state           = SectorState::Good;
    bool           isAddressGood   = false;
    bool           isAddressCheck  = true;
    bool           isDataGood      = false;
    bool           isDataCheck     = true;
    int            passingIndex    = 0;
    int            dos33Logical    = -1;
    int            prodosBlock     = -1;
    int            prodosHalf      = -1;
    EditBlock      editBlock       = EditBlock::None;
};


struct SyncRun
{
    int       firstNibble = 0;
    int       count       = 0;
    uint32_t  startCell   = 0;
    int       widthCells  = 0;
};


struct FieldGap
{
    int      nibblesBefore = 0;
    SyncRun  syncBefore;
};


struct TrackMeasurements
{
    uint32_t         lengthCells     = 0;
    int              nibbleCount     = 0;
    vector<SyncRun>  syncRuns;
    SyncRun          longestSync;
    double           sector0Angle    = -1;
    double           firstFieldAngle = -1;
    double           turnTicks       = 0;
    double           rpm             = 0;
    double           meanCellTicks   = 0;
    double           deviation       = 0;
};


//  What the disk tells the analyzer about a record: the whole track it plays
//  (and, for a record centered on a half track, the other whole track an
//  address field may give), whether it came from a nibble image, and whether
//  it could not be read from the file.
struct TrackContext
{
    int           physicalTrack = 0;
    int           alsoTrack     = -1;
    bool          isNibbleImage = false;
    bool          isDamaged     = false;
};


struct TrackAnalysis
{
    int                    slot              = -1;
    TrackKind              kind              = TrackKind::Bits;
    uint64_t               guestWriteCount   = 0;
    TrackContext           context;
    FramedTrack            framed;
    vector<NibbleKind>     nibbleKinds;
    vector<Byte>           isFailedChecksum;
    vector<int>            fieldOfNibble;
    vector<LocatedField>   fields;
    vector<FieldGap>       fieldGaps;
    vector<AnalyzedSector> sectors;
    TrackClass             trackClass        = TrackClass::NothingRecorded;
    TrackMeasurements      measurements;
    vector<Finding>        findings;
    int                    sectorsGood       = 0;
    int                    sectorsFound      = 0;
    int                    sectorsNotChecked = 0;
    int                    sectorsBad        = 0;
    bool                   has16             = false;
    bool                   has13             = false;
};
