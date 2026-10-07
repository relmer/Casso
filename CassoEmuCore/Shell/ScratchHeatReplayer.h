#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"
#include "Core/MachineConfig.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatRebuildJob.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/Replayer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineBuilder.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchHeatReplayer
//
//  Rebuilds a fading heat map by replaying history on a second machine of
//  its own, built from the running machine's configuration as the history
//  timeline's picture machine is, so the running machine is never touched
//  and nobody waits on it. Each of the job's parts, newest first, is loaded
//  from its keyframe over the job's disks, mounted from their images and
//  never written back, and replayed to its end with the recorded inputs as
//  reverse execution replays them, while a heat map of its own counts every
//  access, folded at each frame boundary of machine time and once more where
//  the part ends, as the running map would be folded. Each part's heat is
//  handed back as soon as it is done.
//
//  Jobs run one at a time on a pool thread of its own, or on a queue a test
//  hands in. A job submitted while another waits replaces it, and one being
//  replayed is abandoned at its next frame, so only the newest finishes.
//
//  SetMachine is called on the thread that builds the running machine;
//  Submit, TryTakeResult and Cancel on the thread that runs it; Rebuild on
//  one worker at a time, or by a test.
//
////////////////////////////////////////////////////////////////////////////////

class ScratchHeatReplayer : public IHeatRebuilder
{
public:
                ScratchHeatReplayer  ();
                ~ScratchHeatReplayer () override;

    ScratchHeatReplayer             (const ScratchHeatReplayer &) = delete;
    ScratchHeatReplayer & operator= (const ScratchHeatReplayer &) = delete;

    void        SetMachine     (const MachineConfig & config, const std::wstring & name);
    void        SetWorkQueue   (IWorkQueue * queue) { m_queue = queue; }

    HRESULT     Submit         (std::shared_ptr<const HeatRebuildJob> job) override;
    bool        TryTakeResult  (HeatRebuildResult & outResult) override;
    void        Cancel         () override;
    void        WaitForWork    ();

    //  The replay itself, on the calling thread, each part's heat in turn.
    HRESULT     Rebuild        (const HeatRebuildJob & job, std::vector<HeatRebuildResult> & outResults);

private:
    //  The medium each drive bay holds, slot-major, zero for none.
    using BayIds = std::array<uint64_t, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;

    static void RunJob         (void * context);
    void        RunPending     ();
    HRESULT     Run            (const HeatRebuildJob & job, const std::function<void (HeatRebuildResult &&)> & onPart);
    HRESULT     RunPart        (const HeatRebuildJob & job, size_t part, HeatRebuildResult & outResult);
    HRESULT     UseQueue       ();
    HRESULT     BuildMachine   ();
    HRESULT     MountDisks     (const HeatRebuildJob & job);
    HRESULT     Replay         (const HeatRebuildJob & job, const HeatRebuildPart & part, HeatRebuildResult & outResult);
    bool        IsAbandoned    (const HeatRebuildJob & job) const;

    //  What the next build is made from, and how many times it has changed.
    std::mutex                              m_configLock;
    MachineConfig                           m_config;
    std::wstring                            m_name;
    uint64_t                                m_configGeneration = 0;

    //  The jobs, between the threads.
    std::mutex                              m_lock;
    std::shared_ptr<const HeatRebuildJob>   m_next;
    std::deque<HeatRebuildResult>           m_results;
    bool                                    m_isRunning        = false;
    std::atomic<uint64_t>                   m_latest           = 0;      // the generation still wanted, 0 for none

    ThreadPoolWorkQueue                     m_ownQueue;
    IWorkQueue                            * m_queue            = nullptr;

    //  The worker's own.
    MachineBuildServices                    m_services;
    std::unique_ptr<MachineHost>            m_machine;
    std::unique_ptr<MachineBuilder>         m_builder;
    KeyframeStore                           m_noKeyframes;
    std::unique_ptr<Replayer>               m_replayer;
    KeyframeUnpacker                        m_unpacker;
    AccessHeatMap                           m_map;
    std::vector<Byte>                       m_state;
    BayIds                                  m_mounted          = {};
    uint64_t                                m_builtGeneration  = 0;
};
