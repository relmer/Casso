#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/Reverse/ReverseController.h"

class IReverseStopTest;
class MachineHost;
struct TraceRecord;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseHost
//
//  Reverse execution as the running emulator holds it: one controller over
//  the machine, recording from when a machine is built until it is torn
//  down, paused while the user has chosen Maximum speed, and moved through
//  history by commands. Everything here runs on the CPU thread.
//
//  While the machine is behind live, the host input gate is held, so input
//  from the UI and controller threads cannot reach a recorded position or a
//  replay in progress; it is released, and the live callback called, once
//  the machine is live again, by a command, a debugger edit or running on.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseHost
{
public:
    explicit ReverseHost (MachineHost & machine);

    ReverseHost             (const ReverseHost &) = delete;
    ReverseHost & operator= (const ReverseHost &) = delete;

    HRESULT  StartRecording (const ReverseSettings & settings);
    void     StopRecording  ();
    HRESULT  OnFrame        (bool isUserMaximumSpeed);
    void     SyncInputGate  ();
    HRESULT  Execute        (ReverseCommand command, uint64_t argument, IReverseStopTest * stopTest, ReverseResult & result);

    void     SetLiveCallback (std::function<void()> onLive) { m_onLive = std::move (onLive); }

    bool     IsRecording    () const { return m_controller.IsRecording(); }
    bool     IsBehindLive   () const { return m_controller.IsInHistory(); }

    //  How far behind live the machine stands; the outcome is left empty.
    HistoryStatus  GetStatus () const;

    void           GetRecentTrace (size_t count, std::vector<TraceRecord> & outEntries) const;

    ReverseController        & GetController()       { return m_controller; }
    const ReverseController  & GetController() const { return m_controller; }

    static ReverseSettings  MakeSettings (int budgetMb, int intervalFrames);

private:
    HRESULT  Move (ReverseCommand command, uint64_t argument, IReverseStopTest * stopTest, ReverseResult & result);

    MachineHost            & m_machine;
    ReverseController        m_controller;
    std::function<void()>    m_onLive;
};
