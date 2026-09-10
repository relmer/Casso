#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

#include "resource.h"





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

void CpuCommandDispatcher::Dispatch (const EmulatorCommand & cmd, ICpuCommandTarget & target)
{
    HRESULT  hr = S_OK;



    switch (cmd.id)
    {
        case IDM_FILE_OPEN:
            hr = target.SwitchMachine (std::wstring (cmd.payload.begin(), cmd.payload.end()));
            if (FAILED (hr))
            {
                DEBUGMSG (L"SwitchMachine failed: 0x%08X\n", hr);
            }

            break;

        case IDM_MACHINE_RESET:
            target.RemountDisks();
            target.SoftReset();
            break;

        case IDM_MACHINE_POWERCYCLE:
            target.PowerCycle();
            target.RemountDisks();
            break;

        case IDM_MACHINE_STEP:
            target.StepInstruction();
            break;

        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
            hr = target.MountDisk ((cmd.id == IDM_DISK_INSERT1) ? 0 : 1, cmd.payload);
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
            target.EjectDisk ((cmd.id == IDM_DISK_EJECT1) ? 0 : 1);
            break;

        case IDM_DISK_WRITEPROTECT1:
        case IDM_DISK_WRITEPROTECT2:
            target.SetDriveUserWriteProtect ((cmd.id == IDM_DISK_WRITEPROTECT1) ? 0 : 1,
                                             !cmd.payload.empty() && cmd.payload[0] == '1');
            break;

        case IDM_DISK_WP1:
        case IDM_DISK_WP2:
            hr = target.ToggleImageWriteProtect ((cmd.id == IDM_DISK_WP1) ? 0 : 1);
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_DISK_SALVAGE1:
        case IDM_DISK_SALVAGE2:
            target.RunSalvageFlow ((cmd.id == IDM_DISK_SALVAGE1) ? 0 : 1);
            break;

        case IDM_DISK_RESOLVE_CHANGE:
            DispatchResolveChange (cmd.payload, target);
            break;

        case IDM_AUDIO_DRIVE_ENABLE:
        case IDM_AUDIO_DRIVE_DISABLE:
            target.SetDriveAudioEnabled (cmd.id == IDM_AUDIO_DRIVE_ENABLE);
            break;

        case IDM_AUDIO_DRIVE_MECHANISM:
            // "shugart" or "alps", canonical lower-case from the settings
            // state; the mixer matches case-insensitively anyway.
            hr = target.SetDriveAudioMechanism (std::wstring (cmd.payload.begin(), cmd.payload.end()));
            IGNORE_RETURN_VALUE (hr, S_OK);
            break;

        case IDM_AUDIO_DRIVE_VOLUMES:
            DispatchDriveVolumes (cmd.payload, target);
            break;

        case IDM_AUDIO_DRIVE_PAN:
            DispatchDrivePan (cmd.payload, target);
            break;

        case IDM_AUDIO_DRIVE_TEST:
            DispatchDriveTest (cmd.payload, target);
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

void CpuCommandDispatcher::DispatchResolveChange (const std::string & payload, ICpuCommandTarget & target)
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

    target.ResolvePendingChange (slot, drive, action, savePath);
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

void CpuCommandDispatcher::DispatchDriveVolumes (const std::string & payload, ICpuCommandTarget & target)
{
    int  motorPct = 0;
    int  headPct  = 0;
    int  doorPct  = 0;



    if (sscanf_s (payload.c_str(), "%d,%d,%d", &motorPct, &headPct, &doorPct) == 3)
    {
        target.SetDriveAudioVolumes ((float) motorPct / 100.0f,
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

void CpuCommandDispatcher::DispatchDrivePan (const std::string & payload, ICpuCommandTarget & target)
{
    int  pan0 = 0;
    int  pan1 = 0;



    if (sscanf_s (payload.c_str(), "%d,%d", &pan0, &pan1) == 2)
    {
        target.SetDriveAudioPan (0, (float) pan0 / 100.0f);
        target.SetDriveAudioPan (1, (float) pan1 / 100.0f);
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

void CpuCommandDispatcher::DispatchDriveTest (const std::string & payload, ICpuCommandTarget & target)
{
    int  drive = 0;
    int  kind  = 0;



    if (sscanf_s (payload.c_str(), "%d,%d", &drive, &kind) == 2)
    {
        target.PlayDriveTestSound (drive, kind);
    }
}
