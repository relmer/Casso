#pragma once

#include "Pch.h"

#include "CassoLink/Session/RawWriteArcs.h"



//  An arc as a pass writes it: the cell its first byte is written at, counted
//  from the first byte of the pass's first arc.
struct RawWritePassArc
{
    size_t  index    = 0;       // its place in the plan's arcs
    int     position = 0;
    size_t  cell     = 0;
    size_t  cells    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass
//
//  A plan's arcs as one stream for the raw write module (RawWrite.s):
//  the arcs at the lowest position, each followed by a stretch the drive
//  doesn't write, then a step of the arm and the arcs at the next position.
//  The module writes from wherever the disk is when it starts, so the arcs
//  are placed by time alone: each is as many cycles after the one before as
//  its angle is past that arc's, for a turn of the drive's length, and a
//  turn more when the arm can't step and settle sooner.
//
//  The cycles of each stretch are the module's: rgap's code up to its first
//  wait, each wait, and what follows a wait.
//
////////////////////////////////////////////////////////////////////////////////

class RawWritePass
{
public:
    std::vector<Byte>             stream;           // kMaxBytes values, then kMaxBytes counts of zeros
    size_t                        count    = 0;     // the bytes of the stream
    int                           position = 0;     // the quarter-track position of the first arc
    std::vector<RawWritePassArc>  arcs;             // in the order written

    //  False when two arcs at one position are too near in angle for the
    //  drive to stop and start writing between them, or the stream is more
    //  than the module's buffers.
    bool  Build (const RawWriteArcPlan & plan, size_t turnCells);

    //  The stream's value for a wait of `cycles`: its rounds of 256 and of
    //  one, with outCycles the wait's length.
    static void  GetWait (size_t cycles, Byte & outLong, Byte & outShort, size_t & outCycles);

    static constexpr Byte    kGapZeros      = 8;        // RGAP: the count of zeros of a stretch not written
    static constexpr Byte    kArmUp         = 8;        // RGUP
    static constexpr Byte    kArmDown       = 9;        // RGDOWN
    static constexpr Byte    kWrite         = 0xFF;     // RGWRITE

private:
    static constexpr Byte    kSync          = 0xFF;
    static constexpr size_t  kCellCycles    = 4;
    static constexpr size_t  kByteCycles    = 32;       // from one store to the next, with no zero bits between
    static constexpr size_t  kSyncCycles    = 40;       // a self-sync byte
    static constexpr size_t  kLeadSyncs     = 4;        // the sync bytes written ahead of an arc
    static constexpr size_t  kPartBytes     = 3;        // a wait's two bytes and what follows it

    //  The module's cycles (rgap).
    static constexpr size_t  kEntryCycles   = 101;      // from the store of the stretch's byte to its first part
    static constexpr size_t  kPhaseCycles   = 51;       // a part that turns a phase off or on, less its wait
    static constexpr size_t  kUpCycles      = 58;       // a part that counts the arm a half track up
    static constexpr size_t  kDownCycles    = 59;       // or down
    static constexpr size_t  kWriteCycles   = 104;      // the last part, to the store of the stream's next byte
    static constexpr size_t  kShortCycles   = 5;        // a wait's round of one
    static constexpr size_t  kLongCycles    = 1284;     // and of 256
    static constexpr size_t  kWaitLess      = 2;
    static constexpr size_t  kMaxShort      = 256;
    static constexpr size_t  kLeastWait     = kShortCycles + kLongCycles - kWaitLess;

    //  The arm's step, as the program's seek has it: the next phase on, a
    //  pause, the last phase off; then a pause for the arm to settle.
    static constexpr size_t  kStepCycles    = 12246;    // 12 ms
    static constexpr size_t  kSettleCycles  = 25512;    // 25 ms
    static constexpr int     kPhases        = 4;
    static constexpr int     kPhaseSwitches = 2;        // a phase's switches: off, then on
    static constexpr int     kHalfTrack     = 2;        // quarter tracks

    //  A part of a stretch not written: a wait and what follows it.
    struct Part
    {
        size_t  wait = 0;       // cycles
        Byte    then = 0;
    };

    //  The parts of the stretch before an arc, other than the last: none for
    //  an arc at the position of the one before, and the arm's steps for an
    //  arc `halfSteps` half tracks from half track `half`. outCycles has
    //  their cycles.
    static void    ListStepParts (int half, int halfSteps, std::vector<Part> & outParts, size_t & outCycles);

    //  The fewest cycles from the store of a stretch's byte to the store of
    //  the first byte after it.
    static size_t  GetLeastStretch (int halfSteps);

    //  The order the arcs are written in, with each arc's first cell.
    static bool    OrderArcs (const RawWriteArcPlan & plan, size_t turnCells, std::vector<size_t> & outOrder, std::vector<size_t> & outCells);

    static void    AppendByte (Byte value, Byte zeros, std::vector<Byte> & ioValues, std::vector<Byte> & ioZeros, size_t & ioCycles);
};
