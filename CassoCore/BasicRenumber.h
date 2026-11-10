#pragma once

#include "Pch.h"

#include "BasicProgram.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicRenumber
//
//  A tokenized Applesoft or Integer BASIC program with its lines renumbered
//  from a start by a step, and every line reference rewritten to match.
//
//  A reference to a line the program does not have is left as written. An
//  Integer BASIC GOTO or GOSUB with an expression is left as written too,
//  because its target is computed when the program runs.
//
////////////////////////////////////////////////////////////////////////////////

class BasicRenumber
{
public:
    static HRESULT  Renumber (const std::vector<Byte>  & programBytes,
                              uint32_t                   start,
                              uint32_t                   step,
                              std::vector<Byte>        & outBytes,
                              std::string              & outReason);

    static constexpr uint32_t  kDefaultStart = 10;
    static constexpr uint32_t  kDefaultStep  = 10;

private:
    static void  RewriteLine (BasicProgram::Dialect                   dialect,
                              const std::map<uint32_t, uint32_t>    & numbers,
                              std::vector<Byte>                     & ioBody);
};
