#pragma once

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
//  Behind live, a change to the machine first forks history, with no
//  question: the history after the machine's position becomes a branch left
//  behind. A command is checked on the UI thread before it is queued, and
//  the fork is queued ahead of it; a debugger edit forks on the CPU thread
//  before it is made. Input the guest reads -- keys, a paste, the game port,
//  the guest mouse -- is deferred, and the lines it would change are
//  published to the watch the CPU thread attaches to the devices; while the
//  machine is paused, the replay runs, so the guest reads it. The first read
//  the deferred input would change stops the replay, restores the machine to
//  before that read, and forks there; the input goes in live, and a replay
//  that was running continues. The //c's 80/40 switch forks at once.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryDivergence
{
public:
    // A guest mouse press deferred behind live and let go of before
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
    void  Diverge                ();
    bool  GuardEdit              (const DebugCommand & command);

    //  Input deferred behind live, and the read that stops the replay for it.
    void  HoldInput              (HeldInput input);
    void  RunForHeldInput        ();
    void  PublishHeldInput       ();
    void  StopForHeldInputRead   ();
    void  OnHeldInputRead        ();
    void  ResumeAfterHeldInput   ();
    void  ForkForHeldInput       ();

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
};
