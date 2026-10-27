#include "Pch.h"

#include "DeploySave.h"

#include "resource.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Shell/CpuManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RequestLiveReturn
//
//  Window thread: posts IDM_DEPLOY_GO_LIVE with a new serial and waits,
//  without pumping messages, until the CPU thread reports that serial's
//  outcome or timeoutMs passes. At the deadline the serial stops being
//  awaited, so a Go live that starts later does nothing, and the answer is
//  TimedOut.
//
////////////////////////////////////////////////////////////////////////////////

LiveReturnOutcome DeploySave::RequestLiveReturn (
    CpuManager  & cpu,
    DWORD         timeoutMs)
{
    uint64_t                      serial  = 0;
    LiveReturnOutcome             outcome = LiveReturnOutcome::TimedOut;
    std::unique_lock<std::mutex>  lock (m_mutex);



    serial          = ++m_nextSerial;
    m_awaitedSerial = serial;
    m_lastSerial    = serial;
    m_lastOutcome.reset();

    lock.unlock();
    cpu.PostCommand (IDM_DEPLOY_GO_LIVE, FormatSerial (serial));
    lock.lock();

    if (m_changed.wait_for (lock, std::chrono::milliseconds (timeoutMs), [this] { return m_lastOutcome.has_value(); }))
    {
        outcome = *m_lastOutcome;
    }

    m_awaitedSerial = 0;

    return outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAwaited
//
//  CPU thread, before a Go live moves the machine: whether this serial is
//  the one a request waits for.
//
////////////////////////////////////////////////////////////////////////////////

bool DeploySave::IsAwaited (uint64_t serial) const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return serial != 0 && serial == m_awaitedSerial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompleteLiveReturn
//
//  CPU thread: stores the outcome for its serial, whether or not a request
//  waits for it, and notifies the waiting thread. An outcome for an older
//  serial than the newest request's changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DeploySave::CompleteLiveReturn (
    uint64_t           serial,
    LiveReturnOutcome  outcome)
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    if (serial == m_lastSerial)
    {
        m_lastOutcome = outcome;
        m_changed.notify_all();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLastOutcome
//
//  Window thread, once the CPU thread is frozen: the newest request's
//  outcome, a late one included, or TimedOut while none has arrived. A Go
//  live already running when the freeze was requested has finished by then,
//  because the freeze takes effect only between passes.
//
////////////////////////////////////////////////////////////////////////////////

LiveReturnOutcome DeploySave::GetLastOutcome() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_lastOutcome.value_or (LiveReturnOutcome::TimedOut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush
//
//  Window thread: freezes the CPU thread, calls Claim on the disk store's
//  token and saves every disk as quit does. The replay flag is cleared first, because a
//  bay flush skips the file while it is set, and its value goes into
//  result.wasReplaying so Resume can set it back. Returns with the CPU thread
//  frozen and the token on the calling thread. When the freeze does not take
//  effect, the token is left alone and the failure is returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeploySave::Flush (
    CpuManager         & cpu,
    DiskImageStore     & store,
    DWORD                timeoutMs,
    DeployFlushResult  & result)
{
    HRESULT  hr = S_OK;



    result = DeployFlushResult();

    hr = cpu.HoldForDeploy (timeoutMs);
    CHR (hr);

    store.GetThreadOwnership().Claim();

    result.wasReplaying  = store.IsReplaying();
    result.wasBehindLive = store.IsFlushHeld() || result.wasReplaying;

    if (result.wasReplaying)
    {
        store.SetReplaying (false);
    }

    hr = store.FlushAllForShutdown();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Resume
//
//  Window thread, for a deploy that did not go ahead: when the calling thread
//  has the store's token, because Flush froze the CPU thread and called Claim,
//  sets the replay flag back and releases the token; then ends the freeze. After a
//  freeze that never took effect it touches no store state.
//
////////////////////////////////////////////////////////////////////////////////

void DeploySave::Resume (
    CpuManager               & cpu,
    DiskImageStore           & store,
    const DeployFlushResult  & result)
{
    if (store.GetThreadOwnership().IsHeldByCurrentThread())
    {
        store.SetReplaying (result.wasReplaying);
        store.GetThreadOwnership().Release();
    }

    cpu.ReleaseDeployHold();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToLiveReturnOutcome
//
//  A Go live's reverse outcome as a live return's. No outcome, as after a
//  failed Execute, is NotReached; HistoryCut is HistoryCut; Moved with the
//  machine live is ReachedLiveEnd; anything else is NotReached.
//
////////////////////////////////////////////////////////////////////////////////

LiveReturnOutcome DeploySave::ToLiveReturnOutcome (
    const std::optional<ReverseOutcome>  & outcome,
    bool                                   isBehindLive)
{
    LiveReturnOutcome  result = LiveReturnOutcome::NotReached;



    if (outcome == ReverseOutcome::HistoryCut)
    {
        result = LiveReturnOutcome::HistoryCut;
    }
    else if (outcome == ReverseOutcome::Moved && !isBehindLive)
    {
        result = LiveReturnOutcome::ReachedLiveEnd;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSavedBehindLive
//
//  Whether the disks were saved somewhere other than the live end, which is
//  when the user is told: the CPU thread was behind live once frozen, or the
//  return to live cut the history. A NotBehindLive answer, and a TimedOut one
//  whose Go live finished before the freeze, mean the disks were saved at the
//  live end.
//
////////////////////////////////////////////////////////////////////////////////

bool DeploySave::IsSavedBehindLive (
    LiveReturnOutcome          outcome,
    const DeployFlushResult  & result)
{
    return result.wasBehindLive || outcome == LiveReturnOutcome::HistoryCut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSerial
//
////////////////////////////////////////////////////////////////////////////////

std::string DeploySave::FormatSerial (uint64_t serial)
{
    return std::to_string (serial);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseSerial
//
//  The whole payload is the decimal serial; anything else parses to nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DeploySave::TryParseSerial (
    const std::string  & payload,
    uint64_t           & serial)
{
    const char             * first  = payload.data();
    const char             * last   = payload.data() + payload.size();
    std::from_chars_result   parsed = std::from_chars (first, last, serial);



    return !payload.empty() && parsed.ec == std::errc() && parsed.ptr == last;
}
