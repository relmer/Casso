#include "Pch.h"

#include "Shell/CpuCommandDispatcher.h"

#include "Debugger/DebugCommandPayload.h"
#include "Debugger/HeatMapRangeSets.h"
#include "resource.h"
#include "Core/TextEncoding.h"





//  The words of an IDM_DEBUG_REVERSE payload. Seek is followed by its
//  position in decimal, and SeekCycle by its cycle.
static constexpr std::pair<ReverseCommand, const char *>  s_kReverseWords[] =
{
    { ReverseCommand::StepBack,        "back"       },
    { ReverseCommand::StepBackOver,    "back-over"  },
    { ReverseCommand::StepBackOut,     "back-out"   },
    { ReverseCommand::ReverseContinue, "continue"   },
    { ReverseCommand::StepForward,     "forward"    },
    { ReverseCommand::Seek,            "seek"       },
    { ReverseCommand::SeekCycle,       "seek-cycle" },
    { ReverseCommand::ScrubCycle,      "scrub-cycle" },
    { ReverseCommand::GoLive,          "live"       },
};





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
            hr = target.SwitchMachine (TextEncoding::NarrowToWide (cmd.payload));
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

        case IDM_DEBUG_SAVE_TRACE:
            target.SaveTrace();
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
            hr = target.SetDriveAudioMechanism (TextEncoding::NarrowToWide (cmd.payload));
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
                target.RunDebugCommand (decoded.clientId, decoded.line, decoded.mode);
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
            target.CloseDebugChannel (cmd.payload == "detach");
            break;

        case IDM_DEBUG_PAUSE:
            target.PauseDebugRun();
            break;

        case IDM_DEBUG_ACTION:
            target.RunDebugActions();
            break;

        case IDM_DEBUG_VIEW:
            DispatchDebugView (cmd.payload, target);
            break;

        case IDM_DEBUG_REVERSE:
            DispatchReverse (cmd.payload, target);
            break;

        case IDM_DEBUG_REVERSE_OPTIONS:
            DispatchReverseOptions (cmd.payload, target);
            break;

        case IDM_DEBUG_DIVERGE:
            target.DivergeHistory();
            break;

        case IDM_FILE_SAVE_STATE:
            target.SaveMachineState (PayloadToPath (cmd.payload));
            break;

        case IDM_FILE_LOAD_STATE:
            target.LoadMachineState (PayloadToPath (cmd.payload));
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetJournalInput
//
//  The commands that change what the machine computes: reset, power cycle,
//  and a disk mount, eject or write-protect change. Each is recorded with the
//  drive in value, and a reset with the Apple keys it holds in detail, so a
//  replay can make the same call. Everything else -- audio, the debugger,
//  a machine switch, which replaces the whole machine -- is not an input.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetJournalInput (const EmulatorCommand & cmd, InputRecord & input)
{
    bool  isInput = true;



    input = InputRecord();

    switch (cmd.id)
    {
        case IDM_MACHINE_RESET:
            input.kind = InputKind::Reset;

            if (cmd.payload.find ("open") != std::string::npos)
            {
                input.detail |= kResetHoldsOpenApple;
            }

            if (cmd.payload.find ("closed") != std::string::npos)
            {
                input.detail |= kResetHoldsClosedApple;
            }

            break;

        case IDM_MACHINE_POWERCYCLE:
            input.kind = InputKind::PowerCycle;
            break;

        case IDM_DISK_INSERT1:
        case IDM_DISK_INSERT2:
            input.kind    = InputKind::DiskMount;
            input.value   = (cmd.id == IDM_DISK_INSERT1) ? 0 : 1;
            input.payload = cmd.payload;
            break;

        case IDM_DISK_EJECT1:
        case IDM_DISK_EJECT2:
            input.kind  = InputKind::DiskEject;
            input.value = (cmd.id == IDM_DISK_EJECT1) ? 0 : 1;
            break;

        case IDM_DISK_WRITEPROTECT1:
        case IDM_DISK_WRITEPROTECT2:
            input.kind   = InputKind::DriveWriteProtect;
            input.value  = (cmd.id == IDM_DISK_WRITEPROTECT1) ? 0 : 1;
            input.detail = (!cmd.payload.empty() && cmd.payload[0] == '1') ? 1 : 0;
            break;

        case IDM_DISK_WP1:
        case IDM_DISK_WP2:
            input.kind  = InputKind::ImageWriteProtect;
            input.value = (cmd.id == IDM_DISK_WP1) ? 0 : 1;
            break;

        default:
            isInput = false;
            break;
    }

    return isInput;
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
//  TryGetHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetHeatMapShown (const std::string & where, bool & shown)
{
    if (where != "on" && where != "off")
    {
        return false;
    }

    shown = (where == "on");
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetHeatMapOptions (const std::string & where, std::string & text)
{
    static constexpr std::string_view  kPrefix = "options ";



    if (!where.starts_with (kPrefix))
    {
        return false;
    }

    text = where.substr (kPrefix.size());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetHeatMapHover
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetHeatMapHover (
    const std::string     & where,
    std::optional<Word>   & address)
{
    static constexpr std::string_view  kPrefix    = "hover ";
    constexpr size_t                   kMaxDigits = 4;
    std::string                        digits;



    if (!where.starts_with (kPrefix))
    {
        return false;
    }

    digits = where.substr (kPrefix.size());

    if (digits == "none")
    {
        address.reset();
        return true;
    }

    if (digits.empty() || digits.size() > kMaxDigits || digits.find_first_not_of ("0123456789abcdefABCDEF") != std::string::npos)
    {
        return false;
    }

    address = (Word) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetHeatMapIgnore
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryGetHeatMapIgnore (
    const std::string                    & where,
    std::vector<std::pair<Word, Word>>   & spans)
{
    static constexpr std::string_view  kPrefix = "ignore ";



    spans.clear();

    if (!where.starts_with (kPrefix))
    {
        return false;
    }

    return HeatMapRangeSets::TryParseSpanWords (where.substr (kPrefix.size()), spans);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHeatMapIgnore
//
//  "ignore" and its spans after "heatmap" go to the target; any other words
//  ask for nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::SetHeatMapIgnore (const std::string & where, ICpuCommandTarget & target)
{
    std::vector<std::pair<Word, Word>>  spans;



    if (TryGetHeatMapIgnore (where, spans))
    {
        target.SetDebugHeatMapIgnore (spans);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchDebugView
//
//  "code <hex>", "code pc", "memory <hex>" for the first memory window, and
//  "memory2" to "memory4" with a hex address or "close" for the others, which
//  is passed on as no address. "lines <hex>" is how many lines the code pane
//  has room for. "trace <decimal>" and "trace end" place the trace pane.
//  "goto <window> <text>" is a memory window's Go to, as typed. "heatmap on"
//  and "heatmap off" say whether the heat map pane is shown, "heatmap options
//  <text>" sets its options and "heatmap reset" zeroes its counts; "heatmap
//  hover" gives the cell under the mouse, and the words HeatAccessJump makes
//  ask about a cell's last access; "heatmap ignore" gives the spans whose
//  reads before written are left out. "beam" asks for
//  the stopped picture to be drawn again with the beam mark turned on or off.
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
    bool                     isShown    = false;
    std::string              options;
    std::optional<Word>      hover;
    HeatAccessRequest        access;



    if (view == "heatmap")
    {
        if (TryGetHeatMapShown (where, isShown))
        {
            target.SetDebugHeatMapShown (isShown);
        }
        else if (TryGetHeatMapOptions (where, options))
        {
            target.SetDebugHeatMapOptions (options);
        }
        else if (where == "reset")
        {
            target.ResetDebugHeatMap();
        }
        else if (TryGetHeatMapHover (where, hover))
        {
            target.SetDebugHeatMapHover (hover);
        }
        else if (HeatAccessJump::TryParseWords (where, access))
        {
            target.RunDebugHeatMapAccess (access);
        }
        else
        {
            SetHeatMapIgnore (where, target);
        }

        return;
    }

    if (view == "beam")
    {
        target.RedrawDebugFrame();
        return;
    }

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

    //  "codetop[N] <hex>" opens a code view with that address on its top line,
    //  as it was when Casso last closed, rather than centered on it.
    isCode = TryGetCodeView (view, "code", index) || TryGetCodeView (view, "lines", index) || TryGetCodeView (view, "codetop", index);

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





////////////////////////////////////////////////////////////////////////////////
//
//  FormatReversePayload
//
////////////////////////////////////////////////////////////////////////////////

std::string CpuCommandDispatcher::FormatReversePayload (
    ReverseCommand  command,
    uint64_t        argument)
{
    std::string  payload;



    for (const auto & entry : s_kReverseWords)
    {
        if (entry.first == command)
        {
            payload = entry.second;
        }
    }

    if (command == ReverseCommand::Seek || command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle)
    {
        payload += std::format (" {}", argument);
    }

    return payload;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseReversePayload
//
//  A word from the table, and for a seek a decimal position or cycle after
//  one space; anything else asks for nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryParseReversePayload (
    const std::string  & payload,
    ReverseCommand     & command,
    uint64_t           & argument)
{
    size_t                  space    = payload.find (' ');
    std::string             word     = payload.substr (0, space);
    std::string             rest     = (space == std::string::npos) ? std::string() : payload.substr (space + 1);
    bool                    isFound  = false;
    bool                    isSeek   = false;
    std::from_chars_result  parsed   = {};



    argument = 0;

    for (const auto & entry : s_kReverseWords)
    {
        if (word == entry.second)
        {
            command = entry.first;
            isFound = true;
        }
    }

    if (!isFound)
    {
        return false;
    }

    isSeek = command == ReverseCommand::Seek || command == ReverseCommand::SeekCycle || command == ReverseCommand::ScrubCycle;

    if (isSeek != !rest.empty())
    {
        return false;
    }

    if (!isSeek)
    {
        return true;
    }

    parsed = std::from_chars (rest.data(), rest.data() + rest.size(), argument);

    return parsed.ec == std::errc() && parsed.ptr == rest.data() + rest.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchReverse
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchReverse (const std::string & payload, ICpuCommandTarget & target)
{
    ReverseCommand  command  = ReverseCommand::StepBack;
    uint64_t        argument = 0;



    if (TryParseReversePayload (payload, command, argument))
    {
        target.RunReverseCommand (command, argument);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatReverseOptionsPayload
//
////////////////////////////////////////////////////////////////////////////////

std::string CpuCommandDispatcher::FormatReverseOptionsPayload (
    bool  isRecording,
    int   budgetMb)
{
    return std::format ("{} {}", isRecording ? "on" : "off", budgetMb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseReverseOptionsPayload
//
//  "on" or "off", one space, and a positive decimal budget in MB; anything
//  else asks for nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuCommandDispatcher::TryParseReverseOptionsPayload (
    const std::string  & payload,
    bool               & isRecording,
    int                & budgetMb)
{
    size_t                  space  = payload.find (' ');
    std::string             word   = payload.substr (0, space);
    std::string             rest   = (space == std::string::npos) ? std::string() : payload.substr (space + 1);
    int                     budget = 0;
    std::from_chars_result  parsed = {};



    if ((word != "on" && word != "off") || rest.empty())
    {
        return false;
    }

    parsed = std::from_chars (rest.data(), rest.data() + rest.size(), budget);

    if (parsed.ec != std::errc() || parsed.ptr != rest.data() + rest.size() || budget <= 0)
    {
        return false;
    }

    isRecording = word == "on";
    budgetMb    = budget;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DispatchReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

void CpuCommandDispatcher::DispatchReverseOptions (const std::string & payload, ICpuCommandTarget & target)
{
    bool  isRecording = false;
    int   budgetMb    = 0;



    if (TryParseReverseOptionsPayload (payload, isRecording, budgetMb))
    {
        target.ApplyReverseOptions (isRecording, budgetMb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PathToPayload
//
//  UTF-8, so a path the active code page cannot hold survives the queue.
//
////////////////////////////////////////////////////////////////////////////////

std::string CpuCommandDispatcher::PathToPayload (const std::filesystem::path & path)
{
    std::u8string  text = path.u8string();



    return std::string (text.begin(), text.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PayloadToPath
//
////////////////////////////////////////////////////////////////////////////////

std::filesystem::path CpuCommandDispatcher::PayloadToPath (const std::string & payload)
{
    std::u8string  text (payload.begin(), payload.end());



    return std::filesystem::path (text);
}




