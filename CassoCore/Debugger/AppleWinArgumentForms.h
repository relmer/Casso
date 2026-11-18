#pragma once

#include "Pch.h"

#include "Debugger/AppleWinArguments.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms
//
//  The argument forms several command families and dialects share: an
//  expression, a range, a source line, a list of values, a count, a
//  decimal, an id or *, and search items.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinArgumentForms
{
public:
    using Tokens = AppleWinArguments::Tokens;

    // How a list of values is written: bytes only, words only, or bytes
    // where a value above $FF becomes two bytes, as MEB takes them.
    enum class ValueWidth
    {
        Bytes,
        Words,
        BytesOrWords,
    };

    //  One expression as a word; a value outside $0000-$FFFF is an error.
    static bool         TryEvaluate           (const std::string & text, const IDebugExpressionContext & context, Word & value, std::string & error);
    static std::string  GetTextAfterFirstWord (const AppleWinArguments & args);
    static bool         TryParseRange         (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool         TryParseSourceLine    (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command);
    static bool         TryParseValues        (const Tokens & tokens, size_t first, ValueWidth width, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
    static bool         TryParseCount         (const std::string & text, uint32_t & value, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool         TryParseDecimal       (const std::string & text, uint64_t & value, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool         IsNumberOrRange       (const std::string & text, NumberSyntax syntax);
    static bool         TryParseIdOrAll       (const Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax = NumberSyntax::AppleWin);
    static bool         TryParseSearchItems   (const std::string & items, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);

private:
    static bool         TryParseSearchWord    (const std::string & word, const IDebugExpressionContext & context, DebugCommand & command, std::string & error);
};
