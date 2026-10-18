#pragma once

#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"

struct InputRecord;
class InputJournal;
struct ReplayDisk;
class ReplayDiskCopier;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRebuildPart
//
//  One stretch of a rebuild's window: the keyframe it begins at, still
//  packed, with its position, cycle and journal index, and the position and
//  cycle it ends at, where the next newer part begins or, for the newest,
//  where the heat is wanted; and the map's kept bits as of its keyframe,
//  which RAM had been written among them, so a read before written counts
//  as it did.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatRebuildPart
{
    PackedKeyframe         start;
    uint64_t               startPosition = 0;
    uint64_t               startCycle    = 0;
    size_t                 journalIndex  = 0;
    uint64_t               endPosition   = 0;
    uint64_t               endCycle      = 0;
    std::vector<uint64_t>  kept;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRebuildJob
//
//  Everything a fading heat map's rebuild needs, copied out of history on
//  the thread that runs the machine so the replay can run on another: the
//  window's parts, newest first, so the heat nearest the machine comes in
//  first; the recorded inputs from the oldest part's start, with the index
//  of the first; and the disks in the bays. The map counts from countFrom
//  on, and fades a single access away in fadeSeconds of machine time.
//
//  The same replay looks up an address's last access in a stretch of
//  history, given a query: the last write or read of an address in a space
//  over the part replayed.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatRebuildJob
{
    struct Query
    {
        bool       isSet   = false;
        HeatSpace  space   = HeatSpace::Cpu;
        bool       isWrite = true;
        Word       address = 0;
    };

    uint64_t                        generation  = 0;
    std::vector<HeatRebuildPart>    parts;
    size_t                          inputsFrom  = 0;
    std::vector<InputRecord>        inputs;
    std::vector<ReplayDisk>         disks;
    uint64_t                        countFrom   = 0;
    double                          fadeSeconds = 0.0;
    Query                           query;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRebuildResult
//
//  The heat one part of a rebuild built, one entry per kind and address as
//  the map keeps it, as of the cycle the part ends at; which part, and
//  whether it is the last to come; how long it took; and whether it worked.
//  A part that failed is the last. For a query, the last access the part
//  made, if it made one.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatRebuildResult
{
    uint64_t            generation   = 0;
    size_t              part         = 0;
    bool                isLast       = false;
    uint64_t            cycle        = 0;
    std::vector<float>  heat;
    uint64_t            instructions = 0;
    double              ms           = 0.0;
    HRESULT             hr           = S_OK;
    bool                hasAccess    = false;
    HeatLastAccess      access;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHeatRebuilder
//
//  Runs heat rebuilds away from the thread that runs the machine, one at a
//  time, handing back each part's heat as it is done. A job submitted while
//  another waits replaces it, and one running is abandoned at its next
//  frame, so only the newest is finished. Results are taken on the
//  machine's thread, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

class IHeatRebuilder
{
public:
    virtual          ~IHeatRebuilder() = default;

    virtual HRESULT  Submit        (std::shared_ptr<const HeatRebuildJob> job) = 0;
    virtual bool     TryTakeResult (HeatRebuildResult & outResult) = 0;
    virtual void     Cancel        () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHeatAccessFinder
//
//  Replays a job with a query on the calling thread and gives the access it
//  found.
//
////////////////////////////////////////////////////////////////////////////////

class IHeatAccessFinder
{
public:
    virtual          ~IHeatAccessFinder() = default;

    virtual HRESULT  FindAccess (const HeatRebuildJob & job, HeatRebuildResult & outResult) = 0;
};