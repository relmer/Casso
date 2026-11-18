#pragma once

#include "Debugger/Reverse/DeferredHistoryEdits.h"
#include "Debugger/Reverse/DivergenceGate.h"
#include "Debugger/Reverse/HeldInputWatch.h"

class Apple2eKeyboard;
class CpuManager;
class DebugViewModel;
class IDebuggerHost;
class IShellDebugContext;
class MachineHost;
class ShellHistory;
struct DebugCommand;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryDivergence
//
//  Behind live, a change to the machine first puts up a question: discard
//  the history after the machine's position? A command is checked on the UI
//  thread before it is queued, and a debugger edit is deferred on the CPU
//  thread and run on a yes. Input the guest reads -- keys, a paste, the game
//  port, the guest mouse -- is deferred without a question, and the lines it
//  would change are published to the watch the CPU thread attaches to the
//  devices. The first read the deferred input would change stops the replay,
//  restores the machine to before that read, and puts up the question; a yes
//  makes the input live there, a no drops it, and either way a replay that
//  was running continues. The //c's 80/40 switch puts up the question at
//  once.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryDivergence
{
public:
    // A guest mouse press that asked behind live and was let go of before
    // the machine was live is held down this long once live, so the guest
    // sees a click rather than nothing. The timer is on the emulator's window.
    static constexpr UINT  kClickHoldMs = 100;

    HistoryDivergence (IDebuggerHost      & host,
                       MachineHost        & machine,
                       CpuManager         & cpuManager,
                       ShellHistory       & history,
                       IShellDebugContext & context,
                       DebugViewModel     & viewModel);

    //  The input gate's state on the UI thread: behind live.
    bool  IsBehindLiveForUi      ();

    //  Commands and debugger edits behind live.
    bool  AllowCommand           (WORD id, const std::string & payload);
    bool  AskToDiverge           ();
    void  Diverge                ();
    bool  GuardEdit              (const DebugCommand & command);
    void  OnConfirmDiverge       ();

    //  Input deferred behind live, and the read that stops the replay for it.
    void  HoldInput              (HeldInput input);
    void  PublishHeldInput       ();
    void  StopForHeldInputRead   ();
    void  OnHeldInputRead        ();
    void  ResumeAfterHeldInput   ();
    void  OnConfirmInputDiverge  ();

    //  The deferred input made live, in order, once the machine is live.
    void  ApplyHeldInputs              (const std::vector<HeldInput> & inputs);
    void  ApplyHeldMouseTarget         (uint32_t target);
    void  PressHeldMouse               ();
    void  ReleaseMouseAfterClick       ();
    void  ToggleHeldEightyColumnSwitch ();
    void  ToggleEightyColumnSwitch     (Apple2eKeyboard * iieKbd);

    //  The gate input passes behind live. A players' change made behind live
    //  is owed to the Joyport once live; so is the length the paste buffer had
    //  before a paste was deferred.
    DivergenceGate  & GetGate                  ()               { return m_divergenceGate; }
    HeldInputWatch  & GetHeldInputWatch        ()               { return m_heldInputWatch; }
    void              OweJoyportSync           ()               { m_isJoyportSyncOwed = true; }
    bool              TakeJoyportSyncOwed      ();
    size_t            GetPasteLengthBeforeHold () const         { return m_pasteLengthBeforeHold; }
    void              SetPasteLengthBeforeHold (size_t length)  { m_pasteLengthBeforeHold = length; }

private:
    IDebuggerHost         & m_host;
    MachineHost           & m_machine;
    CpuManager            & m_cpuManager;
    ShellHistory          & m_history;
    IShellDebugContext    & m_context;
    DebugViewModel        & m_viewModel;
    DivergenceGate          m_divergenceGate;
    HeldInputWatch          m_heldInputWatch;                        // the deferred input's lines, published here, taken by the CPU thread
    std::atomic<bool>       m_isResumeOwedAfterHeldRead = false;     // a replay stopped at a read of deferred input was running
    size_t                  m_pasteLengthBeforeHold     = 0;         // the paste buffer's length before a paste deferred behind live
    bool                    m_isJoyportSyncOwed         = false;     // a players' change made behind live, for the Joyport once live
    bool                    m_isAskingToDiverge         = false;     // the question is up; a change made under it is dropped
    DeferredHistoryEdits    m_deferredEdits;
};
