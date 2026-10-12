#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

#include "resource.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dispatch
//
//  The reset paths read the disks back from the host first, so an image
//  regenerated outside the emulator (hack on a demo, rebuild the disk, hit
//  Reset) is what the boot ROM finds. A power cycle remounts AFTER, because
//  Disk2Controller::PowerCycle points each engine back at its empty internal
//  sentinel and the drives would otherwise come up empty.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::Dispatch (const EmulatorCommand & cmd, const CpuCommandTargets & targets)
{
    HRESULT  hr = S_OK;



    switch (cmd.id)
    {
        case IDM_FILE_OPEN:
            hr = targets.machine.SwitchMachine (TextEncoding::NarrowToWide (cmd.payload));
            if (FAILED (hr))
            {
                DEBUGMSG (L"SwitchMachine failed: 0x%08X\n", hr);
            }

            break;

        case IDM_MACHINE_RESET:
            // The payload names the Apple keys down when the reset was asked
            // for: "open", "closed", both, or nothing. They are held through
            // the reset so the firmware sees them however the host's key
            // state moves around the click.
            targets.machine.HoldAppleKeysThroughReset (cmd.payload.find ("open")   != std::string::npos,
                                                       cmd.payload.find ("closed") != std::string::npos);
            targets.disks.RemountDisks();
            targets.machine.SoftReset();
            break;

        case IDM_MACHINE_POWERCYCLE:
            targets.machine.PowerCycle();
            targets.disks.RemountDisks();
            break;

        case IDM_MACHINE_STEP:
            targets.machine.StepInstruction();
            break;

        case IDM_DEBUG_SAVE_TRACE:
            targets.machine.SaveTrace();
            break;

        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
            hr = targets.disks.MountDisk ((cmd.id == IDM_DISK_INSERT1) ? 0 : 1, cmd.payload);
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
            targets.disks.EjectDisk ((cmd.id == IDM_DISK_EJECT1) ? 0 : 1);
            break;

        case IDM_DISK_WRITEPROTECT1:
        case IDM_DISK_WRITEPROTECT2:
            targets.disks.SetDriveUserWriteProtect ((cmd.id == IDM_DISK_WRITEPROTECT1) ? 0 : 1,
                                                    !cmd.payload.empty() && cmd.payload[0] == '1');
            break;

        case IDM_DISK_WP1:
        case IDM_DISK_WP2:
            hr = targets.disks.ToggleImageWriteProtect ((cmd.id == IDM_DISK_WP1) ? 0 : 1);
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_DISK_RESOLVE_CHANGE:
            DispatchResolveChange (cmd.payload, targets.disks);
            break;

        case IDM_AUDIO_DRIVE_ENABLE:
        case IDM_AUDIO_DRIVE_DISABLE:
            targets.driveAudio.SetDriveAudioEnabled (cmd.id == IDM_AUDIO_DRIVE_ENABLE);
            break;

        case IDM_AUDIO_DRIVE_MECHANISM:
            // "shugart" or "alps", canonical lower-case from the settings
            // state; the mixer matches case-insensitively anyway.
            hr = targets.driveAudio.SetDriveAudioMechanism (TextEncoding::NarrowToWide (cmd.payload));
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_AUDIO_DRIVE_VOLUMES:
            DispatchDriveVolumes (cmd.payload, targets.driveAudio);
            break;

        case IDM_AUDIO_DRIVE_PAN:
            DispatchDrivePan (cmd.payload, targets.driveAudio);
            break;

        case IDM_AUDIO_DRIVE_TEST:
            DispatchDriveTest (cmd.payload, targets.driveAudio);
            break;

        case IDM_TAPE_INSERT:
            targets.tape.ControlTape (TapeCommand::Insert);
            break;

        case IDM_TAPE_EJECT:
            // "keep" unloads without forgetting, for a machine switch.
            targets.tape.ControlTape (cmd.payload == "keep" ? TapeCommand::Unload : TapeCommand::Eject);
            break;

        case IDM_TAPE_PLAY:
            targets.tape.ControlTape (TapeCommand::Play);
            break;

        case IDM_TAPE_STOP:
            targets.tape.ControlTape (TapeCommand::Stop);
            break;

        case IDM_TAPE_REWIND:
            targets.tape.ControlTape (TapeCommand::Rewind);
            break;

        case IDM_TAPE_RECORD:
            // "1" arms record, anything else releases it.
            targets.tape.ControlTape (cmd.payload == "1" ? TapeCommand::ArmRecord : TapeCommand::ReleaseRecord);
            break;

        case IDM_TAPE_SEEK:
            targets.tape.ControlTape (TapeCommand::Seek);
            break;

        case IDM_TAPE_FASTFORWARD:
            targets.tape.ControlTape (TapeCommand::FastForward);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchResolveChange
//
//  "<slot> <drive> <action> <path>", chosen on the UI thread and carried out
//  here. The path is last and takes the rest of the line, since it may
//  contain spaces.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchResolveChange (const std::string & payload, ICpuDiskCommands & disks)
{
    std::istringstream  reader (payload);
    int                 slot   = 0;
    int                 drive  = 0;
    int                 action = 0;
    std::string         savePath;



    reader >> slot >> drive >> action;

    if (reader.fail())
    {
        return;
    }

    std::getline (reader, savePath);

    while (!savePath.empty() && savePath.front() == ' ')
    {
        savePath.erase (savePath.begin());
    }

    disks.ResolvePendingChange (slot, drive, action, savePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchDriveVolumes
//
//  Percents on the wire, unit values at the target: "motor,head,door" in
//  0..100, "pan0,pan1" in -100..100, and "drive,kind" with kind 0 motor, 1
//  head, 2 door.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchDriveVolumes (const std::string & payload, ICpuDriveAudioCommands & driveAudio)
{
    int  motorPct = 0;
    int  headPct  = 0;
    int  doorPct  = 0;



    if (sscanf_s (payload.c_str(), "%d,%d,%d", &motorPct, &headPct, &doorPct) == 3)
    {
        driveAudio.SetDriveAudioVolumes ((float) motorPct / 100.0f,
                                         (float) headPct  / 100.0f,
                                         (float) doorPct  / 100.0f);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchDrivePan
//
//  "pan0,pan1" in -100..100, hard left to hard right.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchDrivePan (const std::string & payload, ICpuDriveAudioCommands & driveAudio)
{
    int  pan0 = 0;
    int  pan1 = 0;



    if (sscanf_s (payload.c_str(), "%d,%d", &pan0, &pan1) == 2)
    {
        driveAudio.SetDriveAudioPan (0, (float) pan0 / 100.0f);
        driveAudio.SetDriveAudioPan (1, (float) pan1 / 100.0f);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchDriveTest
//
//  "drive,kind" with kind 0 motor, 1 head, 2 door: one sound auditioned
//  at the current settings.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchDriveTest (const std::string & payload, ICpuDriveAudioCommands & driveAudio)
{
    int  drive = 0;
    int  kind  = 0;



    if (sscanf_s (payload.c_str(), "%d,%d", &drive, &kind) == 2)
    {
        driveAudio.PlayDriveTestSound (drive, kind);
    }
}
