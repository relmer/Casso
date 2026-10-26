#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/ProfileEventProbe.h"
#include "Shell/ScratchEventReplayer.h"

class MachineHost;
class ReverseController;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileHistory
//
//  A profile built from history: the stretches from the keyframe at or
//  before a period's first cycle to the machine's position are replayed on
//  scratch machines with ProfileEventProbe, in the background, so neither
//  the machine nor the window waits on it.
//
//  The period is cut into blocks at keyframes, on a grid of kBlockPositions
//  instructions, so the same stretch of history is the same block whatever
//  the period. The blocks replay on several scratch machines at once, and
//  their counts are added up in order. A block that ends at a keyframe is
//  cached by the positions and checksums of its keyframes, so a later build
//  over it, of the same period or a longer one, replays only the blocks it
//  has not counted. Within a block, a routine's calls and returns are seen
//  in order; a call open at a block's start is counted at top level until
//  it returns. A newer build replaces one under way.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileHistory
{
public:
    using IsStoppedFn = std::function<bool()>;

    //  Instructions to a block, about two minutes of the machine.
    static constexpr uint64_t  kBlockPositions = 50'000'000;
    static constexpr size_t    kMaxWorkers     = 8;
    static constexpr size_t    kMaxCached      = 512;   // blocks' counts cached for later builds

    ProfileHistory (MachineHost & machine, DebugEventHistory & history);
    ~ProfileHistory();

    ProfileHistory             (const ProfileHistory &) = delete;
    ProfileHistory & operator= (const ProfileHistory &) = delete;

    //  History begins again; null while recording is off.
    void  Start (ReverseController * controller);
    void  Stop  ();

    //  Whether the machine is stopped, so the stretch since the newest
    //  keyframe has an end; unset in a test.
    void  SetIsStopped (IsStoppedFn isStopped) { m_isStopped = std::move (isStopped); }
    void  SetWorkQueue (IWorkQueue * queue);
    void  WaitForWork  ();

    //  The profile from fromCycle, or from history's start when it is
    //  earlier, to the machine's position. An error when there is no
    //  history to replay.
    HRESULT  Build (uint64_t fromCycle, std::string & error);

    //  The finished profile, once the last block is in.
    bool  Pump (ProfileCountsPtr & outCounts);

    //  The share replayed so far, 0 to 1, while a build is under way.
    std::optional<double>  GetProgress () const;

    //  The stretches a build from fromCycle replays, oldest first, joined
    //  wherever no gap divides them; for a test.
    void  GetBounds (uint64_t fromCycle, std::vector<DebugEventBounds> & outBounds) const;

    //  Those stretches cut into blocks, oldest first.
    struct Block
    {
        DebugEventBounds  bounds;
        uint64_t          startChecksum = 0;
        uint64_t          endChecksum   = 0;
        bool              isCacheable   = false;   // ends at a keyframe and lies wholly in the period
    };

    void  GetBlocks (uint64_t fromCycle, std::vector<Block> & outBlocks) const;

    //  How many blocks the last build used from the cache; for a test.
    size_t  GetReusedCount () const { return m_reused; }

    //  The grid's step in place of kBlockPositions; for a test.
    void    SetBlockPositions (uint64_t positions) { m_blockPositions = std::max<uint64_t> (positions, 1); m_cached.clear(); }

private:
    struct Worker
    {
        ProfileEventProbe                                         * probe      = nullptr;
        std::unique_ptr<ScratchEventReplayer<ProfileCountsPtr>>     runner;
        uint64_t                                                    generation = 0;
        std::set<size_t>                                            blocks;          // the job's blocks not yet in
    };

    using BlockKey = std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>;

    bool      IsStopped    () const;
    void      CancelAll    ();
    HRESULT   SubmitBlocks (const std::vector<size_t> & pending, uint64_t fromCycle, uint64_t toCycle);
    static BlockKey  GetKey (const Block & block);

    MachineHost                           & m_machine;
    DebugEventHistory                     & m_history;
    ReverseController                     * m_controller     = nullptr;
    std::vector<Worker>                     m_workers;
    std::map<BlockKey, ProfileCountsPtr>    m_cached;
    std::vector<Block>                      m_blocks;
    std::vector<ProfileCountsPtr>           m_blockCounts;
    std::map<uint64_t, size_t>              m_blockAt;   // a block's index by its start
    size_t                                  m_remaining      = 0;
    size_t                                  m_reused         = 0;
    bool                                    m_isBuilding     = false;
    uint64_t                                m_totalSpan      = 0;
    uint64_t                                m_doneSpan       = 0;
    uint64_t                                m_blockPositions = kBlockPositions;
    IsStoppedFn                             m_isStopped;
};
