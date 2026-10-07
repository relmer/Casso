#pragma once

#include "Pch.h"

#include "Debugger/DebugCommand.h"
#include "Debugger/HeatAccessJump.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Shell/CpuManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICpuCommandTarget
//
//  What a command from the UI thread can ask the CPU thread to do.
//
//  Every method is one outcome: mount this, eject that, reset, run faster.
//  The shell implements it over the machine, the disk manager and the audio
//  mixers; a test implements it with a notebook and reads back what was asked
//  for. Neither side sees the command ids or the payload grammar, which
//  belong to the dispatcher alone.
//
////////////////////////////////////////////////////////////////////////////////

class ICpuCommandTarget
{
public:

    virtual ~ICpuCommandTarget () = default;

    virtual HRESULT  SwitchMachine            (const std::wstring & machineName)                = 0;
    virtual void     SoftReset                ()                                                = 0;
    virtual void     HoldAppleKeysThroughReset (bool openApple, bool closedApple)               = 0;
    virtual void     PowerCycle               ()                                                = 0;
    virtual void     StepInstruction          ()                                                = 0;
    virtual void     SaveTrace                ()                                                = 0;
    virtual void     RemountDisks             ()                                                = 0;
    virtual HRESULT  MountDisk                (int drive, const std::string & path)             = 0;
    virtual void     EjectDisk                (int drive)                                       = 0;
    virtual void     SetDriveUserWriteProtect (int drive, bool wp)                              = 0;
    virtual HRESULT  ToggleImageWriteProtect  (int drive)                                       = 0;
    virtual void     ResolvePendingChange     (int slot, int drive, int action,
                                               const std::string & savePath)                   = 0;
    virtual void     SetDriveAudioEnabled     (bool enabled)                                    = 0;
    virtual HRESULT  SetDriveAudioMechanism   (const std::wstring & mechanism)                  = 0;
    virtual void     SetDriveAudioVolumes     (float motor, float head, float door)             = 0;
    virtual void     SetDriveAudioPan         (int drive, float pan)                            = 0;
    virtual void     PlayDriveTestSound       (int drive, int kind)                             = 0;

    //  A debugger command line and the client its reply goes to, run on the
    //  thread that owns the machine, in `mode` when given and otherwise in the
    //  session's own.
    virtual void     RunDebugCommand          (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode) = 0;

    //  The user paused or resumed the machine, told to an attached debugger.
    virtual void     NotifyDebugPauseChanged  (bool paused)                                     = 0;

    //  The debugger window's requests: open and close the channel, pause a run,
    //  and move a pane. `view` is "code" or "memory"; an empty address means
    //  the code pane follows the PC again.
    virtual void     OpenDebugChannel         ()                                                = 0;
    virtual void     CloseDebugChannel        (bool isDetach)                                   = 0;
    virtual void     PauseDebugRun            ()                                                = 0;

    //  Runs the debugger window's queued actions, each directly.
    virtual void     RunDebugActions          ()                                                = 0;
    virtual void     SetDebugView             (const std::string & view, std::optional<Word> address) = 0;

    //  Where the trace pane reads from: an entry, or the newest when empty.
    virtual void     SetDebugTraceView        (std::optional<uint64_t> first)                   = 0;

    //  Whether the heat map pane is shown, which is whether the machine
    //  records its accesses.
    virtual void     SetDebugHeatMapShown     (bool shown)                                      { (void) shown; }

    //  The heat map pane's options, in HeatMapOptions' text, and its Reset,
    //  which zeroes what the map has counted.
    virtual void     SetDebugHeatMapOptions   (const std::string & text)                        { (void) text; }
    virtual void     ResetDebugHeatMap        ()                                                { }

    //  The heat map cell the mouse is over, none when it is over none; and a
    //  request about a cell's last write or read.
    virtual void     SetDebugHeatMapHover     (std::optional<Word> address)                     { (void) address; }
    virtual void     RunDebugHeatMapAccess    (const HeatAccessRequest & request)               { (void) request; }

    //  The CPU addresses whose reads before written the heat map leaves out,
    //  as first and last of each span.
    virtual void     SetDebugHeatMapIgnore    (const std::vector<std::pair<Word, Word>> & spans) { (void) spans; }

    //  Draws the picture again if what is marked on it has changed while
    //  the machine is stopped, when no frame runs to draw it.
    virtual void     RedrawDebugFrame         ()                                                { }

    //  A memory window's Go to, as typed, to resolve against the machine.
    virtual void     GoToDebugMemory          (int window, const std::string & text)           = 0;

    //  Scrolls a code view by instructions: down when positive.
    virtual void     ScrollDebugCode          (int lines, int view)                             = 0;

    //  A reverse execution command; argument is Seek's position or SeekCycle's cycle.
    virtual void     RunReverseCommand        (ReverseCommand command, uint64_t argument)       = 0;
    //  Tools > Options' recording switch and memory budget, applied at once.
    virtual void     ApplyReverseOptions      (bool isRecording, int budgetMb)                  { (void) isRecording; (void) budgetMb; }

    //  Behind live, the user agreed to discard the history recorded after where
    //  the machine stands, ahead of the change that asked.
    virtual void     DivergeHistory           ()                                                { }

    //  Saves the whole machine to a state file, or loads one.
    virtual void     SaveMachineState         (const std::filesystem::path & path)              = 0;
    virtual void     LoadMachineState         (const std::filesystem::path & path)              = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuCommandDispatcher
//
//  Turns a queued command into calls on the target, on the CPU thread.
//
//  This is the whole of the payload grammar: which id means which drive,
//  what "50,60,70" is a triple of, where the path starts in a change
//  resolution. A payload that does not parse asks for nothing, which is what
//  it did when this was a switch statement inside the shell -- the difference
//  is that now a test can say so.
//
////////////////////////////////////////////////////////////////////////////////

class CpuCommandDispatcher
{
public:

    static void  Dispatch (const EmulatorCommand & cmd, ICpuCommandTarget & target);

    //  A code view's name: `base` for the first, `base` with 2 to 4 after it
    //  for the others; index is 0 to 3.
    static bool  TryGetCodeView (const std::string & view, const std::string & base, int & index);

    //  Whether the command is a host input the input journal records, and
    //  the kind, drive or flags, and path to record for it.
    static bool  TryGetJournalInput (const EmulatorCommand & cmd, InputRecord & input);

    //  "on" or "off" after "heatmap": whether the heat map pane is shown.
    static bool  TryGetHeatMapShown (const std::string & where, bool & shown);

    //  "options <text>" after "heatmap": the options' text.
    static bool  TryGetHeatMapOptions (const std::string & where, std::string & text);

    //  "hover <hex>" or "hover none" after "heatmap": the cell under the mouse.
    static bool  TryGetHeatMapHover (const std::string & where, std::optional<Word> & address);

    //  "ignore " and HeatMapRangeSets' span words after "heatmap": the spans
    //  whose reads before written are left out.
    static bool  TryGetHeatMapIgnore (const std::string & where, std::vector<std::pair<Word, Word>> & spans);
    static void  SetHeatMapIgnore    (const std::string & where, ICpuCommandTarget & target);

    //  The payload of an IDM_DEBUG_REVERSE command, and back.
    static std::string  FormatReversePayload   (ReverseCommand command, uint64_t argument);
    static bool         TryParseReversePayload (const std::string & payload, ReverseCommand & command, uint64_t & argument);

    //  The payload of an IDM_DEBUG_REVERSE_OPTIONS command, and back.
    static std::string  FormatReverseOptionsPayload   (bool isRecording, int budgetMb);
    static bool         TryParseReverseOptionsPayload (const std::string & payload, bool & isRecording, int & budgetMb);

    //  A file path as a command payload, in UTF-8, and back.
    static std::string            PathToPayload (const std::filesystem::path & path);
    static std::filesystem::path  PayloadToPath (const std::string & payload);

    static constexpr uint16_t  kResetHoldsOpenApple   = 1;
    static constexpr uint16_t  kResetHoldsClosedApple = 2;

private:

    static void  DispatchResolveChange (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDriveVolumes  (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDrivePan      (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDriveTest     (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDebugView     (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchReverse       (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchReverseOptions (const std::string & payload, ICpuCommandTarget & target);
};
