#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinition
//
//  A breakpoint as the debugger window's controls define it: its type, and
//  the text the user typed into each field that type has. A disk
//  breakpoint's event is an index into DiskBreakSpecText::GetEventWords and
//  its drive is 0 for either. `isOff` turns off a type that is a switch:
//  BRK, an interrupt, a BASIC error, an unwritten read, a video line or a
//  beam position.
//
//  AppleWinParser::TryMakeBreakpoint evaluates the fields into the
//  DebugCommand the session runs, with no command text in between.
//
////////////////////////////////////////////////////////////////////////////////

struct BreakpointDefinition
{
    enum class Type { Execute, Read, Write, ReadWrite, Value, Condition, Opcode, Brk, Interrupt, Disk, BasicLine, BasicVariable, BasicError,
                      UnwrittenRead, VideoLine, Beam, SourceLine };

    Type         type     = Type::Execute;
    bool         isBefore = false;
    bool         isOff    = false;
    std::string  address;
    std::string  value;
    std::string  condition;
    int          event    = 0;
    int          drive    = 0;
    std::string  arguments;
    std::string  file;

    static BreakpointDefinition  MakeExecute (const std::string & address);
    static BreakpointDefinition  MakeSwitch  (Type type, bool isOn);
};
