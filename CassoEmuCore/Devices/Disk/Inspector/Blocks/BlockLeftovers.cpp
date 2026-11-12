#include "Pch.h"

#include "Cpu.h"
#include "Disassembler.h"
#include "Devices/Disk/Inspector/Blocks/BlockLeftovers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers::Classify
//
////////////////////////////////////////////////////////////////////////////////

LeftoverKind BlockLeftovers::Classify (BlockBytes bytes)
{
    size_t  nonZero = (size_t) std::count_if (bytes.begin(), bytes.end(), [] (Byte b) { return b != 0; });



    if (nonZero < kMinBytes)
    {
        return LeftoverKind::None;
    }

    if (IsText (bytes, nonZero))
    {
        return LeftoverKind::Text;
    }

    return IsCode (bytes) ? LeftoverKind::Code : LeftoverKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers::IsText
//
//  At least 90 percent of the bytes that aren't zero. A block of one byte
//  repeated, such as a fill, isn't text.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockLeftovers::IsText (BlockBytes bytes, size_t nonZero)
{
    constexpr Byte    kLowBits        = 0x7F;
    constexpr Byte    kFirstPrintable = 0x20;
    constexpr Byte    kLastPrintable  = 0x7E;
    constexpr Byte    kReturn         = 0x0D;
    constexpr Byte    kLineFeed       = 0x0A;
    constexpr Byte    kTab            = 0x09;
    constexpr size_t  kPercent        = 100;
    constexpr size_t  kTextPercent    = 90;
    constexpr size_t  kMinDistinct    = 8;



    size_t          text     = 0;
    std::set<Byte>  distinct;
    Byte            low      = 0;



    for (Byte b : bytes)
    {
        low = (Byte) (b & kLowBits);

        if (b == 0)
        {
            continue;
        }

        distinct.insert (b);

        if ((low >= kFirstPrintable && low <= kLastPrintable) || low == kReturn || low == kLineFeed || low == kTab)
        {
            text++;
        }
    }

    return distinct.size() >= kMinDistinct && text * kPercent >= nonZero * kTextPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers::IsCode
//
//  At most 5 percent of the instructions undefined, and enough of the
//  common ones: JSR, JMP and RTS, and loads and stores.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockLeftovers::IsCode (BlockBytes bytes)
{
    constexpr size_t  kPercent       = 100;
    constexpr size_t  kMaxUndefined  = 5;
    constexpr size_t  kMinFlow       = 3;
    constexpr size_t  kMinMoves      = 8;
    constexpr size_t  kMaxBreaks     = 20;



    static Cpu                     cpu;
    vector<DisassembledLine>       lines;
    size_t                         undefined = 0;
    size_t                         flow      = 0;
    size_t                         moves     = 0;
    size_t                         breaks    = 0;



    Disassembler::Disassemble (bytes, 0, cpu.GetInstructionSet(), lines);

    if (lines.empty())
    {
        return false;
    }

    for (const DisassembledLine & line : lines)
    {
        if (!line.isDefined)
        {
            undefined++;
        }
        else if (line.mnemonic == "JSR" || line.mnemonic == "JMP" || line.mnemonic == "RTS")
        {
            flow++;
        }
        else if (line.mnemonic.starts_with ("LD") || line.mnemonic.starts_with ("ST"))
        {
            moves++;
        }
        else if (line.mnemonic == "BRK")
        {
            breaks++;
        }
    }

    return undefined * kPercent <= lines.size() * kMaxUndefined
        && breaks * kPercent <= lines.size() * kMaxBreaks
        && flow >= kMinFlow
        && moves >= kMinMoves;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers::FindRuns
//
////////////////////////////////////////////////////////////////////////////////

vector<LeftoverRun> BlockLeftovers::FindRuns (const vector<LeftoverKind> & kinds)
{
    vector<LeftoverRun>  runs;
    uint32_t             block = 0;



    for (block = 0; block < (uint32_t) kinds.size(); block++)
    {
        if (kinds[block] == LeftoverKind::None)
        {
            continue;
        }

        if (!runs.empty() && runs.back().kind == kinds[block] && runs.back().first + runs.back().count == block)
        {
            runs.back().count++;
            continue;
        }

        runs.push_back ({ block, 1, kinds[block] });
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers::GetKindName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockLeftovers::GetKindName (LeftoverKind kind)
{
    switch (kind)
    {
        case LeftoverKind::Text: return L"text";
        case LeftoverKind::Code: return L"6502 code";
        case LeftoverKind::None: break;
    }

    return L"nothing";
}
