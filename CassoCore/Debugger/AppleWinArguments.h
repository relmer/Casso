#pragma once

#include "Pch.h"

struct AppleWinCommand;
class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArguments
//
//  A command's words, as each argument parser takes them: the table
//  entry, the words after the command, the text after the command as
//  typed, and the context that evaluates expressions.
//
////////////////////////////////////////////////////////////////////////////////

struct AppleWinArguments
{
    using Tokens = std::vector<std::string>;

    const AppleWinCommand         * entry;
    Tokens                          tokens;
    std::string                     rest;
    const IDebugExpressionContext * context;
};
