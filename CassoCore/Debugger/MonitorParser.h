#pragma once

#include "Debugger/DebugCommand.h"
#include "Debugger/MonitorState.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorParser
//
//  One Apple II Monitor line to the commands it means. A line can hold
//  several, as the Monitor allows (`300.30F 400.40F`), so parsing yields a
//  list; the state carries across lines.
//
//  The command characters are declared here as data, because a test decodes
//  the command table of every Apple II ROM Casso ships and asserts that every
//  entry is one of them (FR-027). The union of every ROM's commands works on
//  every machine (FR-016): no single ROM has all of them, and which two the
//  ][+ and //e dropped is an accident of their firmware, not something a user
//  of Casso should have to know.
//
////////////////////////////////////////////////////////////////////////////////

class MonitorParser
{
public:
    // Every character the Monitor treats as a command, as a 7-bit code; a
    // control character is its $00-$1F code, and the rest are uppercase.
    static std::span<const Byte>  GetCommandCharacters ();
    static bool                   IsCommandCharacter   (Byte character);
};
