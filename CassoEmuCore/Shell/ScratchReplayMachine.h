#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReplayDiskCopier.h"
#include "Debugger/Reverse/Replayer.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/MachineBuilder.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchReplayMachine
//
//  A second machine that replays the running machine's history, built from
//  the running machine's configuration as the history timeline's picture
//  machine is, so the running machine is never touched: a Prng and a video
//  timing model before the devices, a disk store that never writes a file,
//  and a replayer over a keyframe store of its own that holds nothing, so a
//  replay checks no keyframe and loads no boundary.
//
//  Its disks are mounted from the images a replay hands it, each only when a
//  different medium is in the bay, with as many track slots as the medium
//  has; a snapshot then loads every track over them.
//
//  SetMachine is called on the thread that builds the running machine; the
//  rest on the one thread that replays.
//
////////////////////////////////////////////////////////////////////////////////

class ScratchReplayMachine
{
public:
                  ScratchReplayMachine  () = default;
                  ~ScratchReplayMachine ();

    ScratchReplayMachine             (const ScratchReplayMachine &) = delete;
    ScratchReplayMachine & operator= (const ScratchReplayMachine &) = delete;

    void          SetMachine  (const MachineConfig & config, const std::wstring & name);
    HRESULT       Build       ();
    HRESULT       MountDisks  (const std::vector<ReplayDisk> & disks);

    MachineHost * GetMachine  () const { return m_machine.get(); }
    Replayer    * GetReplayer () const { return m_replayer.get(); }

private:
    //  The medium each drive bay holds, slot-major, zero for none.
    using BayIds = std::array<uint64_t, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;

    void          Release     ();

    //  What the next build is made from, and how many times it has changed.
    std::mutex                        m_configLock;
    MachineConfig                     m_config;
    std::wstring                      m_name;
    uint64_t                          m_configGeneration = 0;

    //  The replaying thread's own.
    MachineBuildServices              m_services;
    std::unique_ptr<MachineHost>      m_machine;
    std::unique_ptr<MachineBuilder>   m_builder;
    KeyframeStore                     m_noKeyframes;
    std::unique_ptr<Replayer>         m_replayer;
    BayIds                            m_mounted          = {};
    uint64_t                          m_builtGeneration  = 0;
};
