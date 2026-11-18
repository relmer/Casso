#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"

struct BreakpointDefinition;
struct DiskBreakSpec;
struct DiskMarkPattern;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinBreakpointParser
//
//  The arguments of the AppleWin dialect's breakpoint commands: execution
//  and memory breakpoints and their IF clauses, value, register and free
//  conditions, opcode, beam and frame breakpoints, BPDISK, and the
//  breakpoints the debugger window's controls define.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinBreakpointParser
{
public:
    using Tokens = AppleWinArguments::Tokens;

    static bool         TryParseBreakpointArguments (const AppleWinArguments & source, DebugCommand & command, std::string & error);
    static bool         TryParseNameArguments       (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool         TryParseIfClause            (Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool         TryParseCondition           (const std::string & subject, const Tokens & tokens, size_t first, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool         TryParseFreeCondition       (const std::string & text, DebugCommand & command, std::string & error, NumberSyntax syntax);
    static bool         TryParseOpcodes             (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static void         NoteSymbol                  (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command);
    static bool         TryMakeBreakpoint           (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);

private:
    static bool         TryParseValueBreakpoint     (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool         TryParseRegisterCondition   (const Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax);
    static bool         TryParseWatchpointArguments (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool         TryParseBeamArguments       (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool         TryParseFrameArguments      (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static std::string  CutIfClause                 (const std::string & text);
    static bool         IsMnemonicToken             (const std::string & token);

    //  The window's breakpoint definitions (AppleWinBreakpointParserDefinition.cpp).
    static bool         TryMakeExecute              (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool         TryMakeSwitchOff            (const BreakpointDefinition & definition, DebugCommand & command, std::string & error);
    static bool         TryMakeDiskSpec             (const BreakpointDefinition & definition, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool         IsLiveRange                 (const DebugCommand & command, std::string & error);

    //  BPDISK (AppleWinBreakpointParserDisk.cpp).
    static bool         TryParseDiskArguments       (const AppleWinArguments & args, DebugCommand & command, std::string & error);
    static bool         TryParseDiskEventArguments  (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec);
    static bool         TryParseDiskNumber          (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, std::optional<Byte> & outValue);
    static bool         TryParseDiskFields          (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec, int count);
    static bool         TryParseDiskRange           (const Tokens & tokens, size_t & at, const IDebugExpressionContext & context, DiskBreakSpec & spec, bool takesHalf);
    static bool         TryParseDiskCommand         (const Tokens & tokens, size_t & at, DiskBreakSpec & spec);
    static bool         TryParseDiskOptions         (const Tokens & tokens, size_t & at, DiskBreakSpec & spec);
    static bool         TryParseDiskMarks           (const Tokens & tokens, size_t & at, std::optional<DiskMarkPattern> & outMarks);
    static bool         IsDiskOptionWord            (const std::string & token);
    static bool         TryEvaluateQuietly          (const std::string & text, const IDebugExpressionContext & context, Word & value);
};
