#pragma once

#include "Cpu.h"
#include "Debugger/CallStack.h"
#include "Debugger/Reply.h"

class IDebugTarget;
class SymbolTable;
class WatchpointTable;
struct DebugSessionView;





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper
//
//  A debug session's call-stack record (FR-068): started at PC, fed the
//  opcodes that can change it by the CPU and the stack writes by the
//  watchpoint table, and settled between instructions before it is read.
//  A step out's frame is set from it, and turning recording off while that
//  step runs takes effect when the step stops.
//
////////////////////////////////////////////////////////////////////////////////

class CallRecordKeeper : public IOpcodeWatcher
{
public:
    CallRecordKeeper (IDebugTarget & target, WatchpointTable & watchpoints);

    void                           SetRecording       (bool isOn);
    bool                           IsRecording        () const { return m_recorder.IsActive(); }
    void                           Restart            ();
    void                           MarkUnrebuilt      ();
    uint64_t                       GetGeneration      () const { return m_generation; }
    std::optional<CallStackBreak>  GetBottom          () const { return m_recorder.GetBottom(); }
    CallRecord                     GetRecord          ();
    void                           Adopt              (const CallRecord & record);
    void                           SetRebuildProgress (std::optional<float> progress) { m_rebuildProgress = progress; }
    CallStackMechanism             GetMechanism       () const                        { return m_mechanism; }
    void                           SetMechanism       (CallStackMechanism mechanism)  { m_mechanism = mechanism; }

    //  The chain to PC, with each frame's target looked up in the symbol
    //  tables; with entries, calls to other addresses are dropped.
    CallStackData                  GetCallStack       (const SymbolTable & symbols, const std::set<Word> * entries);
    bool                           HasNoCallSince     (uint64_t cycle);
    void                           GetCallerLinks     (std::vector<CallerLink> & outLinks);
    void                           Settle             ();

    //  A step out's frame on the record, or a notice when the record has no
    //  frame for it.
    void                           SetStepOutFrame    (RunRequest & request, std::string & outNotice);

    //  The PC at the last stop: a PC that differs at a step out was moved.
    void                           NoteStopPc         (Word pc) { m_stopPc = pc; }

    void                           OnMachineChanged   ();
    void                           OnReset            (bool isPowerCycle);
    void                           OnStopped          ();

    void                           TakeView           (DebugSessionView & out) const;
    void                           InstallView        (const DebugSessionView & view);

    // IOpcodeWatcher: the record's instructions, from the CPU.
    void                           OnWatchedFetch     (Word pc, Byte sp, Byte opcode) override;

private:
    Byte                           PeekByte            (Word address) const;
    Word                           FindStoreInProgress () const;

    IDebugTarget             & m_target;
    WatchpointTable          & m_watchpoints;
    CallStackRecorder          m_recorder;
    std::array<bool, 0x100>    m_opcodes   = {};
    CallStackMechanism         m_mechanism = CallStackMechanism::Hybrid;

    //  Counts the record's starts, ends and resets; and how far along a
    //  rebuild of it from history is, while one runs.
    uint64_t                   m_generation = 0;
    std::optional<float>       m_rebuildProgress;

    bool                       m_isStepOutOnRecord      = false;
    bool                       m_isRecordingEndDeferred = false;
    std::optional<Word>        m_stopPc;
};
