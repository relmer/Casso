#pragma once

#include "Debugger/Reply.h"

class DebugSession;
class Microcode;
struct DebugCommand;
struct Expression;





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers
//
//  The breaks on an instruction rather than an address: BRK on a BRK or an
//  invalid opcode, BRKOP on an opcode or a mnemonic, BRKINT on an interrupt,
//  and BRKUNINIT on a read of memory before it was written.
//
////////////////////////////////////////////////////////////////////////////////

class BrkHandlers
{
public:
    static constexpr Byte  kBrkOpcode = 0x00;

    static void  SetBrk       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetOpcode    (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetInterrupt (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetUnwritten (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  Whether a BRK definition's text turns the break on.
    static bool  IsBrkOn      (const std::string & text);

private:
    static constexpr int  kOpcodeCount = 256;
    static constexpr int  kBrkSelector = 0;
    static constexpr int  kAllSelector = -1;
    static constexpr int  kMaxLength   = 3;

    static bool  TryParseBrkArguments (const std::string & text, int & selector, std::optional<bool> & isOn, std::string & error);
    static void  AddInvalidOpcodes    (DebugSession & session, int length, BreakpointListData & added);
    static void  RemoveInvalidOpcodes (DebugSession & session, int length);
    static void  RemoveBrkOpcodes     (DebugSession & session);
    static bool  IsInvalidOfLength    (DebugSession & session, Byte opcode, int length);
    static int   GetInvalidLength     (const Microcode * set, Byte opcode);
    static void  ReportBrk            (DebugSession & session, Reply & reply);
    static void  AddOpcodeEntry       (DebugSession & session, Byte opcode, const Expression & condition, BreakpointListData & list);
    static bool  TryFindOpcodes       (DebugSession & session, const std::string & mnemonic, std::vector<Byte> & opcodes);
    static bool  HasInterrupt         (DebugSession & session);
    static void  ReportUnwritten      (DebugSession & session, Reply & reply);
};
