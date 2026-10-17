#pragma once

#include "Debugger/DebugCommand.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"

struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerAction
//
//  What a control in the debugger window does, as the command the session
//  runs directly, with no command text in between (FR-135). `echo` is the
//  equivalent command the console shows behind the prompt of `echoMode`, or
//  of the session's mode when that is empty. It is display only: nothing
//  parses it.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerAction;

struct BreakpointStep
{
    //  Actions: the actions run as one undo step. Import: the breakpoint
    //  lines of the file at `path`, as one step. Undo and Redo take the
    //  newest step back or forward.
    enum class Kind
    {
        Actions,
        Import,
        Undo,
        Redo,
    };

    Kind                         kind = Kind::Actions;
    std::vector<DebuggerAction>  actions;
    std::string                  path;
};

struct DebuggerAction
{
    DebugCommand                   command;
    std::string                    echo;
    std::optional<CommandMode>     echoMode;

    //  What the user typed for the command's value, evaluated in the
    //  session's mode when the action runs: into `a1` for AddWatch and
    //  SetRegister, and as the values for EnterBytes and EnterWords.
    std::string                    operand;

    //  A breakpoint definition as BP, BPM or another setting command takes
    //  it in AppleWin's words, parsed once when the action runs, in place of
    //  `command`.
    std::string                    definition;

    //  A breakpoints pane action, which runs in place of `command` and goes
    //  on the pane's undo list (FR-120).
    std::optional<BreakpointStep>  breakpointStep;

    //  The window putting back what it had open, not a choice the user
    //  made: the command runs and the console shows neither echo nor reply.
    bool                           quiet = false;

    //  For a memory window's undo or redo: the bytes memory must contain from
    //  the command's address, or nothing is written.
    std::vector<Byte>              required;
};

//  Builds an action in the session's mode, for a pane that does not know it.
using DebuggerActionBuilder = std::function<DebuggerAction (CommandMode mode)>;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions
//
//  The actions the window's controls, keys and menus take. Each builds the
//  command the matching typed line parses to, and its echo in the session's
//  mode, `mode`.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerActions
{
public:
    static DebuggerAction  GetRun              (CommandMode mode);
    static DebuggerAction  GetStepInto         (CommandMode mode);
    static DebuggerAction  GetStepOver         (CommandMode mode);
    static DebuggerAction  GetStepOut          (CommandMode mode);

    //  BASICSTEP, BASICLINE or BASICOUT, by its verb.
    static DebuggerAction  GetBasicStep        (DebugVerb verb, CommandMode mode);

    //  T-, P-, GU-, G- or LIVE: the verb is one of the reverse verbs.
    static DebuggerAction  GetReverse          (DebugVerb verb, CommandMode mode);

    //  Runs to the address. Its echo is Casso's G, whatever the mode, since
    //  GSSquared's g takes no address.
    static DebuggerAction  GetRunToCursor      (Word address);
    static DebuggerAction  GetRunFrame         ();

    //  Sets a breakpoint at the address, or clears the execution breakpoint
    //  there by its id, so a click never clears a different breakpoint that
    //  happens to cover the same address.
    static DebuggerAction  GetToggleBreakpoint (const DebuggerViewSnapshot & snapshot, Word address, CommandMode mode);

    static DebuggerAction  GetEnableBreakpoint (int id, bool enable, CommandMode mode);
    static DebuggerAction  GetClearBreakpoint  (int id, CommandMode mode);
    static DebuggerAction  GetClearWatch       (int id, CommandMode mode);
    static DebuggerAction  GetBreakpointStep   (BreakpointStep step);
    static DebuggerAction  GetPanel            (const std::string & id, bool open, CommandMode mode);
    static DebuggerAction  GetPoke             (Word address, Byte value, CommandMode mode);
    static DebuggerAction  GetTraceToggle      (bool isOn, CommandMode mode);
    static DebuggerAction  GetSetMode          (CommandMode target, CommandMode mode);
    static DebuggerAction  GetSetRegister      (const std::string & name, Word value, CommandMode mode);
    static DebuggerAction  GetPatch            (Word address, std::span<const Byte> bytes, CommandMode mode, std::span<const Byte> required = {});
    static DebuggerAction  GetCallStackMode    (const std::string & mechanism, CommandMode mode);
    static DebuggerAction  GetSaveHistory      (const std::string & path, CommandMode mode);

    //  BLOAD and BSAVE: a file loaded in a format (BinaryImageReader's word,
    //  or empty for the format the file's contents indicate), at an address
    //  and for a length when given; and a range saved as raw bytes. A length
    //  runs from the address, which is required with it, and ends within
    //  memory.
    static DebuggerAction  GetLoadBinary       (const std::string & path, const std::string & format, std::optional<Word> address,
                                                std::optional<uint32_t> length, CommandMode mode);
    static DebuggerAction  GetSaveBinary       (const std::string & path, Word address, uint32_t length, CommandMode mode);

    //  RCC, which starts the trip count again; and PUSH, POP and PPOP, the
    //  Stack pane's push and pops.
    static DebuggerAction  GetResetCycles      (CommandMode mode);
    static DebuggerAction  GetPushByte         (Byte value, CommandMode mode);
    static DebuggerAction  GetPopByte          (CommandMode mode);
    static DebuggerAction  GetPopWord          (CommandMode mode);

    //  WE and WD, by the watch's id; ZP, ZPC, ZPE and ZPD for the zero-page
    //  pointers, by slot.
    static DebuggerAction  GetEnableWatch            (int id, bool enable, CommandMode mode);
    static DebuggerAction  GetAddZeroPagePointer     (Byte address, CommandMode mode);
    static DebuggerAction  GetClearZeroPagePointer   (int slot, CommandMode mode);
    static DebuggerAction  GetEnableZeroPagePointer  (int slot, bool enable, CommandMode mode);

    //  A Memory pane's S over all memory, F, M and MC. `items` are S's, as
    //  typed, which the session parses as it runs.
    static DebuggerAction  GetSearchMemory   (const std::string & items, CommandMode mode);
    static DebuggerAction  GetFillMemory     (Word first, Word last, std::span<const Byte> bytes, CommandMode mode);
    static DebuggerAction  GetMoveMemory     (Word destination, Word first, Word last, CommandMode mode);
    static DebuggerAction  GetCompareMemory  (Word other, Word first, Word last, CommandMode mode);

    //  The code pane's menu: a range marked as data of a directive's kind
    //  (DB, DW, DA, ASC or DF) or as code again, a symbol, a routine never
    //  stepped into, a bookmark added or removed, the stopwatch between two
    //  addresses, and a routine run as JSR runs it.
    static DebuggerAction  GetMarkData       (const std::string & directive, Word first, Word last, CommandMode mode);
    static DebuggerAction  GetClearData      (Word first, Word last, CommandMode mode);
    static DebuggerAction  GetAddSymbol      (const std::string & name, Word address, CommandMode mode);
    static DebuggerAction  GetSkipRoutine    (const std::string & name, Word address, CommandMode mode);
    static DebuggerAction  GetAddBookmark    (Word address, CommandMode mode);
    static DebuggerAction  GetClearBookmark  (int id, CommandMode mode);
    static DebuggerAction  GetStopwatch      (Word start, Word stop, CommandMode mode);
    static DebuggerAction  GetCallSubroutine (Word address, CommandMode mode);

    //  SRC ON or OFF, SYM LOAD, BPSAVE, and PROJECT with a subcommand such as
    //  OPEN or SAVEAS, each on a file the user picked.
    static DebuggerAction  GetSourceStepping   (bool isSource, CommandMode mode);

    //  A watch edit's action as a debug project saves it: the verb of an
    //  AddWatch, ClearWatch, EnterBytes, EnterWords or SetRegister action and
    //  its fields, apart at dots, with any text among them as hex digits. The
    //  text of an action of another kind is empty, and TryFromText returns
    //  false for text that does not parse.
    static std::string     ToText      (const DebuggerAction & action);
    static bool            TryFromText (const std::string & text, DebuggerAction & action);
    static DebuggerAction  GetLoadSymbols      (const std::string & path, CommandMode mode);
    static DebuggerAction  GetSaveBreakpoints  (const std::string & path, CommandMode mode);
    static DebuggerAction  GetProject          (const std::string & subcommand, const std::string & path, CommandMode mode);
    static DebuggerAction  GetAddWatch         (Word address, CommandMode mode);
    static DebuggerAction  GetSourceBreakpoint (const std::string & fileName, int line, CommandMode mode);

    //  The breakpoints pane's changes, where `*` is every breakpoint.
    static DebuggerAction  GetEnableAllBreakpoints (bool enable, CommandMode mode);
    static DebuggerAction  GetClearAllBreakpoints  (CommandMode mode);
    static DebuggerAction  GetEditBreakpoint       (int id, const std::string & definition, CommandMode mode);
    static DebuggerAction  GetChangeBreakpoint     (int id, bool enabled, bool temporary, bool stops, CommandMode mode);
    static DebuggerAction  GetDefineBreakpoint     (const std::string & definition, CommandMode mode);

    //  BPNAME # "text"; an empty text returns the breakpoint to its default
    //  name.
    static DebuggerAction  GetNameBreakpoint       (int id, const std::string & name, CommandMode mode);

    //  The watches pane's edits. A string value is what the user typed, which
    //  the session evaluates in its mode when the action runs.
    static DebuggerAction  GetAddWatch             (const std::string & expression, CommandMode mode);
    static DebuggerAction  GetEnterWord            (Word address, const std::string & value, CommandMode mode);
    static DebuggerAction  GetEnterWord            (Word address, Word value, CommandMode mode);
    static DebuggerAction  GetEnterByte            (Word address, const std::string & value, CommandMode mode);
    static DebuggerAction  GetEnterByte            (Word address, Byte value, CommandMode mode);
    static DebuggerAction  GetSetRegister          (const std::string & name, const std::string & value, CommandMode mode);

    //  The action a keyboard-scheme action takes, which is the action its
    //  button takes. The cursor actions use the selected code line (toggling
    //  falls back to the PC's line); Pause has none, since it is the channel's
    //  pause rather than a command. A step, forward or back, takes nothing
    //  while the snapshot shows the machine running.
    static std::optional<DebuggerAction>  GetForKey (DebuggerKeySchemes::Action   action,
                                                     const DebuggerViewSnapshot * snapshot,
                                                     int                          selectedRow,
                                                     CommandMode                  mode);

    //  Step into, over and out, run to cursor, run one frame and the step
    //  back keys: the actions that need a paused machine.
    static bool  IsStepAction (DebuggerKeySchemes::Action action);

private:
    static DebuggerAction  Make         (DebugVerb verb, const std::string & appleWinLine, CommandMode mode);
    static void            SetRangeFrom (DebugCommand & command, Word other, Word first, Word last);
};
