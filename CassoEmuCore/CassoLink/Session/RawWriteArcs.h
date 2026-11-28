#pragma once

#include "Pch.h"

#include "CassoLink/Session/RawWriteStream.h"



//  An arc as one stream of the image has it, at that stream's positions.
struct RawWriteArcRead
{
    std::vector<int>  positions;
    RawWriteStream    stream;
};



//  A stretch of a turn with data, with blank on either side of it.
struct RawWriteArc
{
    RawWriteStream                stream;           // its bytes, from the read with the most
    std::vector<RawWriteArcRead>  reads;            // every read of all of it
    size_t                        start = 0;        // its first cell in the group's turn
    size_t                        cells = 0;
    std::vector<int>              positions;        // the quarter-track positions all of it is found at
    std::vector<int>              partPositions;    // and those a part of it is found at
};



//  The arcs of positions less than a full track apart, in order of angle.
struct RawWriteArcGroup
{
    std::vector<RawWriteArc>  arcs;
    size_t                    turn = 0;    // the cells of the turn the arcs' starts are in
    std::vector<int>          positions;   // every position with one of the arcs
};



//  The write of a group's arcs: each arc once, at one position.
struct RawWriteArcPlan
{
    struct Arc
    {
        int             position = 0;
        size_t          start    = 0;          // its first cell in the plan's turn
        size_t          cells    = 0;
        RawWriteStream  stream;
    };

    std::vector<Arc>                  arcs;          // in order of position, then of angle
    size_t                            turn = 0;
    double                            lean = 0.0;    // how far above its position the image has each track, in quarter tracks
    std::vector<int>                  erased;        // the positions erased with sync bytes before the arcs are written
    std::vector<int>                  positions;     // every position of the group, none of which is written as a track
    std::vector<std::pair<int, int>>  lost;          // the positions of two arcs at the same angle, one of which is lost
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs
//
//  A protected disk can have data at positions less than a full track apart,
//  as Karateka has at tracks 2 and 2.5: each is a few arcs of a turn with
//  blank between, placed so that no arc is beside another at the same angle.
//  A write is about three quarter tracks wide, so a whole turn written at one
//  of those positions trims the turn written at the other. Find lists the
//  arcs and the positions each is found at, so each can be written once.
//
//  An image's positions aren't aligned to each other in angle, so an arc is
//  matched from one position to another by its bytes, and its angle is taken
//  from a position that has it and an arc already placed.
//
////////////////////////////////////////////////////////////////////////////////

class RawWriteArcs
{
public:
    //  A quarter-track position with data. Positions with the same data have
    //  the same stream.
    struct Source
    {
        int                     position = 0;
        const RawWriteStream  * stream   = nullptr;
    };

    //  The groups of arcs in an image's positions. A track with data all the
    //  way around isn't in a group, and neither is a track whose arcs aren't
    //  found at another position less than a full track away.
    static void  Find  (std::span<const Source> sources, std::vector<RawWriteArcGroup> & outGroups);

    //  A stream's arcs, in order from its first byte, with outTurn its cells.
    //  None for a stream with data all the way around or with no data.
    static void  Split (const RawWriteStream & stream, std::vector<RawWriteArc> & outArcs, size_t & outTurn);

    //  How far above a track's position the drive that read the image found
    //  it, in quarter tracks: the middle of the positions each track with
    //  data all the way around is found at, less the track's, averaged.
    static double  GetLean (std::span<const Source> sources);

    //  The position each of a group's arcs is written at: the half or whole
    //  track nearest the middle of the positions it is found at, with the
    //  lean taken off. False when that is one position for all of them, so
    //  the group is written as a track.
    static bool    Plan    (const RawWriteArcGroup & group, double lean, RawWriteArcPlan & outPlan);

    //  The plans of a WOZ image's groups, those with arcs at more than one
    //  position, with outGroups every group. The error of a file that
    //  doesn't load.
    static HRESULT      PlanImage    (std::span<const Byte> fileBytes, std::vector<RawWriteArcPlan> & outPlans, std::vector<RawWriteArcGroup> & outGroups);

    //  A plan for the activity log: its positions, and each arc's position,
    //  angle and length.
    static std::string  DescribePlan (const RawWriteArcPlan & plan);

private:
    static constexpr Byte    kTopBit        = 0x80;
    static constexpr size_t  kBlankZeros    = 8;        // the zero bits after a byte that blank begins with
    static constexpr size_t  kJoinBytes     = 16;       // blank bytes this close are one blank stretch
    static constexpr size_t  kMinGapCells   = 400;      // the shortest blank stretch that ends an arc
    static constexpr size_t  kBlankPercent  = 15;       // the least of a turn that is blank on a track of arcs
    static constexpr size_t  kChunkBytes    = 12;       // the chunk of an arc looked for in another
    static constexpr size_t  kChunkValues   = 5;        // the distinct bytes of a chunk worth looking for
    static constexpr double  kSameShare     = 0.75;     // the part of an arc found in another that makes them one arc
    static constexpr double  kSameLength    = 0.8;      // and how long the shorter is, of the longer
    static constexpr double  kPartShare     = 0.4;      // the part found that makes one a part of the other
    static constexpr size_t  kPercent       = 100;
    static constexpr size_t  kNearlyAll     = 98;       // the percentage of an arc's longest read that another read has, to be written in its place
    static constexpr int     kFullTrack     = 4;        // quarter tracks
    static constexpr int     kHalfTrack     = 2;
    static constexpr double  kWholeBias     = 0.5;      // how much nearer a half track is than a whole one, in quarter tracks, before the lowest arc is written at it
    static constexpr size_t  kFillerShare   = 4;        // an arc with fewer than one chunk in this many worth looking for is sync bytes or filler
    static constexpr size_t  kMarginCells   = 160;      // the cells between two arcs' angles, below which one overwrites the other
    static constexpr size_t  kMinArcBytes   = 128;      // an arc shorter than this is a misread, and isn't written
    static constexpr size_t  kRealPositions = 2;        // an arc found at this many positions is on the disk, not a misread
    static constexpr double  kLeanShare     = 0.9;      // the part of a position's chunks found in a track that makes it that track
    static constexpr size_t  kLeanStep      = 8;        // one chunk in this many is looked for

    //  One stream of the image: its positions and arcs, and for each arc the
    //  arc of the group it is and whether all of it is here.
    struct Turn
    {
        const RawWriteStream      * stream = nullptr;
        std::vector<int>            positions;
        std::vector<RawWriteArc>    arcs;
        std::vector<int>            arcOf;
        std::vector<bool>           isWhole;
        size_t                      cells  = 0;
        int                         group  = -1;
    };

    //  An arc matched across turns: the turn and arc with the most bytes.
    struct Match
    {
        size_t  turn = 0;
        size_t  arc  = 0;
    };

    static void    ListTurns     (std::span<const Source> sources, std::vector<Turn> & outTurns);
    static void    MatchArcs     (std::vector<Turn> & ioTurns, std::vector<Match> & outMatches);
    static void    GroupTurns    (std::vector<Turn> & ioTurns, int & outGroups);
    static void    BuildGroup    (const std::vector<Turn> & turns, const std::vector<Match> & matches, int group, RawWriteArcGroup & outGroup);
    static void    PlaceArcs     (const std::vector<Turn> & turns, const std::vector<size_t> & members, size_t reference, std::map<int, size_t> & ioStarts);

    //  The part of `arc`'s chunks found in `other`, 0 to 1.
    static double  GetSharedPart (const RawWriteStream & arc, const RawWriteStream & other);

    static bool    AreNear       (const Turn & first, const Turn & second);

    //  The bits of a stream's bytes, with no zero bits between them, as text.
    static std::string  GetValueBits  (const RawWriteStream & stream);

    //  The part of a position's chunks found in a track with data all the
    //  way around, 0 to 1.
    static double       GetTrackShare (const RawWriteStream & position, const RawWriteStream & track);

    //  The arcs of a plan that another arc would be written over, to drop.
    static void         FindOverwritten (const RawWriteArcPlan & plan, const std::vector<size_t> & found, std::vector<bool> & outDropped, std::vector<std::pair<int, int>> & outLost);

    //  The middle of the positions all of an arc is found at, with the lean
    //  taken off.
    static double  GetMiddle     (const RawWriteArc & arc, double lean);

    //  Whether an arc is written: one of kMinArcBytes or more that isn't
    //  sync bytes or one byte repeated, as the erase before it writes.
    static bool    IsWritten     (const RawWriteArc & arc);

    //  Whether two arcs of a turn are within kMarginCells of each other in angle.
    static bool    AreAtOneAngle (const RawWriteArcPlan::Arc & first, const RawWriteArcPlan::Arc & second, size_t turn);
    static bool    IsBlank       (const RawWriteStream & stream, size_t at) { return (stream.values[at] & kTopBit) == 0 || stream.zeros[at] >= kBlankZeros; }
};
