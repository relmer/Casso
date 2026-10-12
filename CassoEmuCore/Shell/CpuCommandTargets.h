#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeCommand
//
//  The tape-deck operations run on the CPU thread. Insert takes the tape the
//  UI thread has already read and decoded.
//
////////////////////////////////////////////////////////////////////////////////

enum class TapeCommand
{
    Insert,
    Eject,
    Play,
    Stop,
    Rewind,
    ArmRecord,
    ReleaseRecord,
    Unload,     // out of the deck but still remembered: a machine switch
    Seek,       // to the position TapeManager::Seek last stored
    FastForward,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICpuMachineCommands
//
//  What a command from the UI thread can ask of the machine as a whole: a
//  switch, a reset, a power cycle, one instruction, the trace. The shell
//  implements it.
//
////////////////////////////////////////////////////////////////////////////////

class ICpuMachineCommands
{
public:

    virtual ~ICpuMachineCommands () = default;

    virtual HRESULT  SwitchMachine             (const std::wstring & machineName)  = 0;
    virtual void     SoftReset                 ()                                  = 0;
    virtual void     HoldAppleKeysThroughReset (bool openApple, bool closedApple)  = 0;
    virtual void     PowerCycle                ()                                  = 0;
    virtual void     StepInstruction           ()                                  = 0;
    virtual void     SaveTrace                 ()                                  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICpuDiskCommands
//
//  What a command can ask of the Disk II drives. ShellDisks implements it.
//
////////////////////////////////////////////////////////////////////////////////

class ICpuDiskCommands
{
public:

    virtual ~ICpuDiskCommands () = default;

    virtual void     RemountDisks             ()                                     = 0;
    virtual HRESULT  MountDisk                (int drive, const std::string & path)  = 0;
    virtual void     EjectDisk                (int drive)                            = 0;
    virtual void     SetDriveUserWriteProtect (int drive, bool wp)                   = 0;
    virtual HRESULT  ToggleImageWriteProtect  (int drive)                            = 0;
    virtual void     ResolvePendingChange     (int slot, int drive, int action,
                                               const std::string & savePath)        = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICpuDriveAudioCommands
//
//  What a command can ask of the drive sounds. ShellAudio implements it.
//
////////////////////////////////////////////////////////////////////////////////

class ICpuDriveAudioCommands
{
public:

    virtual ~ICpuDriveAudioCommands () = default;

    virtual void     SetDriveAudioEnabled   (bool enabled)                         = 0;
    virtual HRESULT  SetDriveAudioMechanism (const std::wstring & mechanism)       = 0;
    virtual void     SetDriveAudioVolumes   (float motor, float head, float door)  = 0;
    virtual void     SetDriveAudioPan       (int drive, float pan)                 = 0;
    virtual void     PlayDriveTestSound     (int drive, int kind)                  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICpuTapeCommands
//
//  What a command can ask of the cassette recorder. ShellTapeDeck implements
//  it.
//
////////////////////////////////////////////////////////////////////////////////

class ICpuTapeCommands
{
public:

    virtual ~ICpuTapeCommands () = default;

    virtual void  ControlTape (TapeCommand command) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuCommandTargets
//
//  Everything a queued command can be carried out against. The shell, the
//  drives, the drive sounds and the recorder in the app; one notebook that
//  implements all four in a test, which then reads back what was asked for
//  and in what order. None of them sees the command ids or the payload
//  grammar, which belong to the dispatcher alone.
//
////////////////////////////////////////////////////////////////////////////////

struct CpuCommandTargets
{
    ICpuMachineCommands     & machine;
    ICpuDiskCommands        & disks;
    ICpuDriveAudioCommands  & driveAudio;
    ICpuTapeCommands        & tape;
};
