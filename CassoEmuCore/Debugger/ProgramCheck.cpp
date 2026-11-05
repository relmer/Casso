#include "Pch.h"

#include "Debugger/ProgramCheck.h"
#include "Core/TextEncoding.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::GetDebugFileEntries
//
//  A line with more than one span, or a span longer than an instruction, is
//  a macro's or data's, and a line whose text has no mnemonic is a
//  directive's, so none of them is an entry.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ProgramCheck::Entry> ProgramCheck::GetDebugFileEntries (const DebugFile & file, const std::map<int, std::vector<std::string>> & sources)
{
    std::vector<Entry>                   entries;
    std::map<int, const DebugSpan *>     spans;
    std::map<int, const DebugSegment *>  segments;



    for (const DebugSpan & span : file.spans)
    {
        spans[span.id] = &span;
    }

    for (const DebugSegment & segment : file.segments)
    {
        segments[segment.id] = &segment;
    }

    for (const DebugLine & line : file.lines)
    {
        auto      span    = (line.spans.size() == 1) ? spans.find (line.spans.front()) : spans.end();
        auto      source  = sources.find (line.file);
        auto      segment = segments.end();
        uint32_t  address = 0;
        Entry     entry;

        if (line.type != DebugLineType::Asm || span == spans.end() || span->second->size == 0)
        {
            continue;
        }

        segment = segments.find (span->second->segment);
        address = (segment != segments.end()) ? segment->second->start + span->second->start : 0x10000;

        if (address > 0xFFFF)
        {
            continue;
        }

        entry.address = (Word) address;

        if (!span->second->bytes.empty())
        {
            entry.bytes.assign (span->second->bytes.begin(), span->second->bytes.begin() + (ptrdiff_t) std::min (span->second->bytes.size(), kLabelBytes));
        }
        else if (source != sources.end() && line.line >= 1 && (size_t) line.line <= source->second.size() &&
                 span->second->size <= Disassembler::kMaxInstructionBytes)
        {
            entry.mnemonic = GetMnemonic (source->second[(size_t) line.line - 1]);
            entry.size     = span->second->size;
        }

        if (!entry.bytes.empty() || !entry.mnemonic.empty())
        {
            entries.push_back (std::move (entry));
        }
    }

    return Sample (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::GetLabelEntries
//
//  Zero page and the stack are a program's variables, I/O changes as it is
//  read, and ROM is the same under every program, so only the RAM between
//  them differs from one program to another.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ProgramCheck::Entry> ProgramCheck::GetLabelEntries (const std::vector<Word> & labels, const PeekFn & peek)
{
    std::vector<Entry>  entries;
    std::set<Word>      seen;



    for (Word label : labels)
    {
        Entry  entry;
        Byte   value = 0;

        if (!IsRamLabel (label) || !seen.insert (label).second)
        {
            continue;
        }

        entry.address = label;

        for (uint32_t at = label; at <= kRamLast && entry.bytes.size() < kLabelBytes && peek ((Word) at, value); at++)
        {
            entry.bytes.push_back (value);
        }

        if (!entry.bytes.empty())
        {
            entries.push_back (std::move (entry));
        }
    }

    return Sample (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::GetMnemonic
//
////////////////////////////////////////////////////////////////////////////////

std::string ProgramCheck::GetMnemonic (const std::string & line)
{
    std::wstring  wide = TextEncoding::NarrowToWide (line);
    std::string   mnemonic;



    for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (wide))
    {
        if (run.token == SourceSyntax::Token::Mnemonic)
        {
            mnemonic = line.substr ((size_t) run.start, (size_t) run.length);
            break;
        }
    }

    return ToUpper (mnemonic);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::IsMatch
//
////////////////////////////////////////////////////////////////////////////////

bool ProgramCheck::IsMatch (const std::vector<Entry> & entries, const PeekFn & peek, const Microcode * instructionSet)
{
    std::optional<Disassembler>  disassembler;
    size_t                       matched      = 0;



    if (instructionSet != nullptr)
    {
        disassembler.emplace (instructionSet);
    }

    for (const Entry & entry : entries)
    {
        matched += IsEntryMatch (entry, peek, disassembler.has_value() ? &*disassembler : nullptr) ? 1 : 0;
    }

    return matched * 4 >= entries.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::IsEntryMatch
//
////////////////////////////////////////////////////////////////////////////////

bool ProgramCheck::IsEntryMatch (const Entry & entry, const PeekFn & peek, const Disassembler * disassembler)
{
    Byte                     bytes[Disassembler::kMaxInstructionBytes] = {};
    size_t                   count                                     = 0;
    Byte                     value                                     = 0;
    DisassembledInstruction  instruction;
    HRESULT                  hr                                        = S_OK;



    if (!entry.bytes.empty())
    {
        for (size_t i = 0; i < entry.bytes.size(); i++)
        {
            if (!peek ((Word) (entry.address + i), value) || value != entry.bytes[i])
            {
                return false;
            }
        }

        return true;
    }

    if (disassembler == nullptr)
    {
        return true;
    }

    while (count < Disassembler::kMaxInstructionBytes && peek ((Word) (entry.address + count), bytes[count]))
    {
        count++;
    }

    hr = disassembler->DisassembleOne (entry.address, std::span<const Byte> (bytes, count), instruction);

    return SUCCEEDED (hr) && instruction.isDefined && instruction.bytes.size() == entry.size && ToUpper (instruction.mnemonic) == entry.mnemonic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::Sample
//
//  A long program is checked at evenly spaced entries, so a stop costs the
//  same for a program of any length.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ProgramCheck::Entry> ProgramCheck::Sample (std::vector<Entry> entries)
{
    std::vector<Entry>  sampled;



    if (entries.size() <= kMaxEntries)
    {
        return entries;
    }

    for (size_t i = 0; i < kMaxEntries; i++)
    {
        sampled.push_back (std::move (entries[i * entries.size() / kMaxEntries]));
    }

    return sampled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramCheck::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string ProgramCheck::ToUpper (std::string text)
{
    std::ranges::transform (text, text.begin(), [] (char c) { return (char) std::toupper ((unsigned char) c); });
    return text;
}
