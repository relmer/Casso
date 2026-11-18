#pragma once

#include "Debugger/DebugCommand.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines
//
//  The command line each control of the debugger window sends, in the words
//  of the console's mode, and the keys that go to the command box instead of
//  the key scheme. Plain text in and out, with no session in it.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerCommandLines
{
public:
    //  The mode run to cursor runs in, whatever the console's: GSSquared's g
    //  takes no address, so the window sends Casso's own G in every dialect.
    static constexpr CommandMode  kRunToCursorMode = CommandMode::Casso;

    //  The command a control stands for.
    static std::string  GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address);
    static std::string  GetPokeLine             (Word address, Byte value);
    static bool         IsCodeBreakpointAt      (const DebuggerViewSnapshot::BreakpointLine & bp, Word address);
    static bool         IsDiskRoutineAt         (const DebuggerViewSnapshot::BreakpointLine & bp, Word address);
    static std::string  GetStepLine             ()             { return "T"; }
    static std::string  GetStepOverLine         ()             { return "P"; }
    static std::string  GetStepOutLine          ()             { return "RTS"; }
    static std::string  GetRunLine              ()             { return "G"; }
    static std::string  GetRunToCursorLine      (Word address);
    static std::string  GetTraceToggleLine      (bool isOn) { return isOn ? "HISTORY OFF" : "HISTORY ON"; }

    //  PANEL to open or close a device panel, as an AppleWin line, which
    //  GetModeLine gives in the words of the current mode.
    static std::string  GetPanelLine            (const std::string & id, bool open);

    //  The command line a keyboard-scheme action sends, which is the line its
    //  button sends. The cursor actions use the selected code line (toggling
    //  falls back to the PC's line); Pause has no line, since it is the
    //  channel's pause rather than a command.
    static std::optional<std::string>  GetActionLine (DebuggerKeySchemes::Action   action,
                                                      const DebuggerViewSnapshot * snapshot,
                                                      int                          selectedRow);

    //  A control's AppleWin line in the words of the mode the session is in,
    //  where the mode has them: GSSquared's `s`, `o`, `r`, `g`, `bp`, `nobp`
    //  and `addr:` deposit; WinDbg's `t`, `p`, `gu`, `g`, `bp`, `bc` and `eb`.
    //  Any other line, and any line in another mode, is returned as it is.
    static std::string  GetModeLine   (const std::string & line, CommandMode mode);
    static std::string  GetWinDbgLine (const std::string & name, const std::string & rest, const std::string & line);
    static bool         IsHexBytes    (const std::string & text);

    //  In GSSquared mode with the command line empty, Space and F10 step and
    //  Return resumes, as GSSquared's window does, whatever the key scheme;
    //  otherwise nothing, and the key goes where it would have.
    static std::optional<DebuggerKeySchemes::Action>  GetConsoleKeyAction (CommandMode mode,
                                                                           WPARAM      vk,
                                                                           bool        ctrl,
                                                                           bool        alt,
                                                                           bool        shift,
                                                                           bool        isLineEmpty);

    //  While the line assembler is taking lines, Return and Space go to the
    //  command box whatever the scheme, so an empty line can end it.
    static bool  DoesAssemblerKeepKey (const DebuggerViewSnapshot * snapshot, WPARAM vk, bool ctrl, bool alt);

    //  In Monitor mode Return on an empty command line is a line of its own:
    //  it shows the next row of bytes, so the command line keeps it from the
    //  key scheme.
    static bool  DoesConsoleKeepKey (CommandMode mode, WPARAM vk, bool ctrl, bool alt);

    //  The R or W a line has with no file name, which the window prompts for,
    //  and the line with the chosen name added.
    static std::optional<DebugVerb>  GetMissingFileVerb  (const std::string & line, CommandMode mode, bool isAssembling);
    static std::string               GetLineWithFileName (const std::string & line, const std::string & path);
};
