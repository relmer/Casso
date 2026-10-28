#pragma once

#include "Pch.h"

#include "Debugger/Reverse/ReverseOutcome.h"


class CpuManager;
class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  LiveReturnOutcome
//
//  Where a return to the live end before an update install left the machine.
//
////////////////////////////////////////////////////////////////////////////////

enum class LiveReturnOutcome
{
    NotBehindLive,      // the machine was live; nothing ran
    ReachedLiveEnd,
    HistoryCut,         // the replay diverged: live at the last good keyframe, the history after it dropped
    NotReached,         // behind live after the return: a gap, a failed Execute, or a later command moved it back
    TimedOut,           // no outcome for this request before the deadline
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeployFlushResult
//
//  What DeploySave::Flush found once the CPU thread was held.
//
////////////////////////////////////////////////////////////////////////////////

struct DeployFlushResult
{
    bool  wasBehindLive = false;    // SetFlushHold was on or the replay flag was set
    bool  wasReplaying  = false;    // the replay flag's value before the save cleared it
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeploySave
//
//  An update installed while the machine is behind live saves the disks at
//  the live end. The window thread posts a request that the CPU thread return
//  to live and waits for the outcome, bounded; then it holds the CPU thread and saves
//  every disk from the window thread.
//
//  RequestLiveReturn and GetLastOutcome run on the window thread, IsAwaited
//  and CompleteLiveReturn on the CPU thread, and the mutex guards the serials
//  and the outcome between them. A request that timed out is no longer
//  awaited, so a Go live that the CPU thread runs after the deadline does
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class DeploySave
{
public:
    LiveReturnOutcome  RequestLiveReturn  (CpuManager & cpu, DWORD timeoutMs);
    bool               IsAwaited          (uint64_t serial) const;
    void               CompleteLiveReturn (uint64_t serial, LiveReturnOutcome outcome);
    LiveReturnOutcome  GetLastOutcome     () const;

    static HRESULT            Flush               (CpuManager & cpu, DiskImageStore & store, DWORD timeoutMs, DeployFlushResult & result);
    static void               Resume              (CpuManager & cpu, DiskImageStore & store, const DeployFlushResult & result);
    static LiveReturnOutcome  ToLiveReturnOutcome (const std::optional<ReverseOutcome> & outcome, bool isBehindLive);
    static bool               IsSavedBehindLive   (LiveReturnOutcome outcome, const DeployFlushResult & result);

    //  The IDM_DEPLOY_GO_LIVE payload, and back.
    static std::string  FormatSerial   (uint64_t serial);
    static bool         TryParseSerial (const std::string & payload, uint64_t & serial);

private:
    mutable std::mutex                 m_mutex;
    std::condition_variable            m_changed;
    uint64_t                           m_nextSerial    = 0;
    uint64_t                           m_awaitedSerial = 0;    // 0 when no request waits
    uint64_t                           m_lastSerial    = 0;    // the newest request's
    std::optional<LiveReturnOutcome>   m_lastOutcome;          // m_lastSerial's, once it arrives
};
