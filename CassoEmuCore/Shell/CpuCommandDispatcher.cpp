#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

#include "Debugger/DebugCommandPayload.h"
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
            // The payload names the Apple keys down when the reset was asked
            // for: "open", "closed", both, or nothing. They are held through
            // the reset so the firmware sees them however the host's key
            // state moves around the click.
            target.HoldAppleKeysThroughReset (cmd.payload.find ("open")   != std::string::npos,
                                              cmd.payload.find ("closed") != std::string::npos);
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

        case IDM_DEBUG_COMMAND:
        {
            //  A payload that cannot be read names no client to answer, so it
            //  is dropped rather than run with its reply sent nowhere.
            DebugCommandPayload  decoded;

            if (DebugCommandPayload::TryDecode (cmd.payload, decoded))
            {
                target.RunDebugCommand (decoded.clientId, decoded.line);
            }

            break;
        }

        case IDM_DEBUG_PAUSE_CHANGED:
            target.NotifyDebugPauseChanged (cmd.payload == "1");
            break;

        case IDM_DEBUG_OPEN:
            target.OpenDebugChannel();
            break;

        case IDM_DEBUG_CLOSE:
            target.CloseDebugChannel();
            break;

        case IDM_DEBUG_PAUSE:
            target.PauseDebugRun();
            break;

        case IDM_DEBUG_VIEW:
            DispatchDebugView (cmd.payload, target);
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





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetCodeView
//
//  `base` names the first code view and `base` followed by 2 to 4 the others;
//  index is 0 to 3.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetCodeView (const std::string & view, const std::string & base, int & index)
{
    if (view == base)
    {
        index = 0;
        return true;
    }

    if (view.size() == base.size() + 1 && view.starts_with (base) && view.back() >= '2' && view.back() <= '4')
    {
        index = view.back() - '1';
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchDebugView
//
//  "code <hex>", "code pc", "memory <hex>" for the first memory window, and
//  "memory2" to "memory4" with a hex address or "close" for the others, which
//  is passed on as no address. "lines <hex>" is how many lines the code pane
//  has room for. "trace <decimal>" and "trace end" place the trace pane.
//  "goto <window> <text>" is a memory window's Go to, as typed.
//  Anything else asks for nothing: a pane moved to an address nobody meant is
//  worse than a pane left where it was.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchDebugView (const std::string & payload, ICpuCommandTarget & target)
{
    static constexpr size_t  kMaxEntryDigits = 18;
    size_t                   space           = payload.find (' ');
    std::string              view            = payload.substr (0, space);
    std::string              where           = (space == std::string::npos) ? std::string() : payload.substr (space + 1);
    bool                     isExtraWin = view == "memory2" || view == "memory3" || view == "memory4";
    unsigned int             value      = 0;
    size_t                   used       = 0;
    int                      index      = 0;
    bool                     isCode     = false;



    if (view == "trace")
    {
        if (where == "end")
        {
            target.SetDebugTraceView (std::nullopt);
        }
        else if (!where.empty() && where.size() <= kMaxEntryDigits && where.find_first_not_of ("0123456789") == std::string::npos)
        {
            target.SetDebugTraceView (std::stoull (where));
        }

        return;
    }

    //  "codescroll[N] <signed decimal>": instructions to scroll a code view.
    if (TryGetCodeView (view, "codescroll", index))
    {
        if (!where.empty() && where.size() <= 6 && where.find_first_not_of ("-0123456789") == std::string::npos && where.find ('-', 1) == std::string::npos)
        {
            target.ScrollDebugCode (std::stoi (where), index);
        }

        return;
    }

    //  "follow <N>" hands following the PC to code view N; "codeclose <N>"
    //  closes view N, which is never the first.
    if ((view == "follow" || view == "codeclose") && where.size() == 1 && where[0] >= (view == "follow" ? '1' : '2') && where[0] <= '4')
    {
        target.SetDebugView (view, (Word) (where[0] - '1'));
        return;
    }

    //  "goto <window> <text>": the text is whatever was typed, resolved later.
    if (view == "goto")
    {
        space = where.find (' ');

        if (space == 1 && where[0] >= '1' && where[0] <= '4')
        {
            target.GoToDebugMemory (where[0] - '0', where.substr (2));
        }

        return;
    }

    isCode = TryGetCodeView (view, "code", index) || TryGetCodeView (view, "lines", index);

    if (view != "memory" && !isCode && !isExtraWin)
    {
        return;
    }

    if ((view.starts_with ("code") && where == "pc") || (isExtraWin && where == "close"))
    {
        target.SetDebugView (view, std::nullopt);
        return;
    }

    if (where.empty() || where.size() > 4 || where.find_first_not_of ("0123456789abcdefABCDEF") != std::string::npos)
    {
        return;
    }

    value = std::stoul (where, &used, 16);
    target.SetDebugView (view, (Word) value);
}
