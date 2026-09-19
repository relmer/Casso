#pragma once

#include "Pch.h"

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
    //  thread that owns the machine.
    virtual void     RunDebugCommand          (uint32_t clientId, const std::string & line)     = 0;

    //  The user paused or resumed the machine, told to an attached debugger.
    virtual void     NotifyDebugPauseChanged  (bool paused)                                     = 0;

    //  The debugger window's requests: open and close the channel, pause a run,
    //  and move a pane. `view` is "code" or "memory"; an empty address means
    //  the code pane follows the PC again.
    virtual void     OpenDebugChannel         ()                                                = 0;
    virtual void     CloseDebugChannel        ()                                                = 0;
    virtual void     PauseDebugRun            ()                                                = 0;
    virtual void     SetDebugView             (const std::string & view, std::optional<Word> address) = 0;

    //  Where the trace pane reads from: an entry, or the newest when empty.
    virtual void     SetDebugTraceView        (std::optional<uint64_t> first)                   = 0;
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

private:

    static void  DispatchResolveChange (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDriveVolumes  (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDrivePan      (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDriveTest     (const std::string & payload, ICpuCommandTarget & target);
    static void  DispatchDebugView     (const std::string & payload, ICpuCommandTarget & target);
};
