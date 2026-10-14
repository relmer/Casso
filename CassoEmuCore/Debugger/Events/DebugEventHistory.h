#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventJob.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/ReplayDiskCopier.h"

class KeyframeStore;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventHistory
//
//  The keyframe store and input journal of the running machine as event
//  rebuilds use them: the stretches between its keyframes, and a job copied
//  out of history for some of them. The live stretch, from the newest
//  keyframe to the machine, is not one of them; its events are in the event
//  cache.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class DebugEventHistory : public IDebugEventHistory
{
public:
    explicit DebugEventHistory (MachineHost & machine);

    void      Attach       (KeyframeStore * keyframes) { m_keyframes = keyframes; }

    // IDebugEventHistory
    void      GetStretches (std::vector<DebugEventBounds> & outStretches) const override;
    HRESULT   BuildJob     (const std::vector<DebugEventBounds> & stretches, DebugEventJob & outJob) override;

private:
    HRESULT   AddStretch   (const DebugEventBounds & bounds, DebugEventJob & ioJob);

    MachineHost        & m_machine;
    KeyframeStore      * m_keyframes  = nullptr;
    KeyframeUnpacker     m_noUnpacker;                 // empty, so every copy includes its whole snapshot
    ReplayDiskCopier     m_diskCopier;
    uint64_t             m_generation = 0;
};
