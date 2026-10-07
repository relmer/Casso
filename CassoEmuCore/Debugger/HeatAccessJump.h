#pragma once

#include "Pch.h"

#include "Debugger/AccessHeatMap.h"
#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessRequest
//
//  A request about the instruction that last wrote or read an address in a
//  bank: show it in the disassembly, or take the machine back through
//  history to just after it ran.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatAccessRequest
{
    bool                  isRewind = false;
    bool                  isWrite  = true;
    HeatMapOptions::Bank  bank     = HeatMapOptions::Bank::Cpu;
    Word                  address  = 0;

    bool operator== (const HeatAccessRequest & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessInfo
//
//  The instruction that last wrote or read an address, as a tip shows it:
//  its PC, the symbol there if one is known, the instruction as the code
//  there disassembles now, and the cycle it began on; or that it is older
//  than the history kept.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatAccessInfo
{
    bool         has         = false;
    bool         isTooOld    = false;
    Word         pc          = 0;
    uint64_t     cycle       = 0;
    std::string  label;
    std::string  instruction;

    bool operator== (const HeatAccessInfo & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessHover
//
//  The last writer and reader of the cell the mouse is over, in the bank
//  the map shows.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatAccessHover
{
    Word                  address = 0;
    HeatMapOptions::Bank  bank    = HeatMapOptions::Bank::Cpu;
    HeatAccessInfo        writer;
    HeatAccessInfo        reader;

    bool operator== (const HeatAccessHover & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessPlan
//
//  What a request comes to: the PC to show, the position to seek to, or a
//  message saying why neither.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatAccessPlan
{
    enum class Kind
    {
        ShowCode,
        Seek,
        Report,
    };

    Kind         kind     = Kind::Report;
    Word         pc       = 0;
    uint64_t     position = 0;
    std::string  message;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump
//
//  Turns a request into a plan, and a request into the words the debugger
//  window sends the machine for it and back.
//
//  A seek lands just after the instruction that made the access: the byte
//  written holds its new value there, and the instruction is the one the
//  disassembly shows just above the PC, where a step back returns to it.
//
////////////////////////////////////////////////////////////////////////////////

class HeatAccessJump
{
public:
    //  What the map and history say of the access; whether history is being
    //  recorded, and the oldest position it holds.
    static HeatAccessPlan  Plan (const HeatAccessRequest  & request,
                                 HeatAccessState            state,
                                 const HeatLastAccess     & access,
                                 bool                       isRecording,
                                 uint64_t                   oldestPosition);

    //  "rewind write main 2000", "code read cpu D000"; and back, false for
    //  anything else.
    static std::string     FormatWords   (const HeatAccessRequest & request);
    static bool            TryParseWords (const std::string & words, HeatAccessRequest & outRequest);

    //  1234567 as "1,234,567".
    static std::string     GroupDigits   (uint64_t value);

    //  "Last written by $6A12 DRAWROW: STA ($06),Y at cycle 1,234,567",
    //  "Not written since counting started", or "Last written before the
    //  history kept".
    static std::wstring    Describe      (bool isWrite, const HeatAccessInfo & info);
};
