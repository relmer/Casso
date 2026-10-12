#pragma once

#include "Pch.h"

#include "Shell/CpuCommandTargets.h"
#include "Shell/CpuManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuCommandDispatcher
//
//  Turns a queued command into calls on its targets, on the CPU thread.
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

    static void  Dispatch (const EmulatorCommand & cmd, const CpuCommandTargets & targets);

private:

    static void  DispatchResolveChange (const std::string & payload, ICpuDiskCommands & disks);
    static void  DispatchDriveVolumes  (const std::string & payload, ICpuDriveAudioCommands & driveAudio);
    static void  DispatchDrivePan      (const std::string & payload, ICpuDriveAudioCommands & driveAudio);
    static void  DispatchDriveTest     (const std::string & payload, ICpuDriveAudioCommands & driveAudio);
};
