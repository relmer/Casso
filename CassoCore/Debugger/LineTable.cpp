#include "Pch.h"

#include "Debugger/LineTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable::Clear
//
////////////////////////////////////////////////////////////////////////////////

void LineTable::Clear()
{
    m_byAddress.clear();
    m_byLine.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable::Build
//
//  A span's first address is its segment's start plus its own. Positions at an
//  address are sorted by depth, so the invocation comes before the body.
//
////////////////////////////////////////////////////////////////////////////////

void LineTable::Build (const DebugFile & file)
{
    std::map<int, uint32_t>                 segmentStart;
    std::map<int, std::pair<Word, Word>>    spanRange;



    Clear();
    m_byAddress.resize (kAddressSpace);

    for (const DebugSegment & segment : file.segments)
    {
        segmentStart[segment.id] = segment.start;
    }

    for (const DebugSpan & span : file.spans)
    {
        uint32_t  first = segmentStart[span.segment] + span.start;

        if (span.size > 0 && first + span.size - 1 < kAddressSpace)
        {
            spanRange[span.id] = { (Word) first, (Word) (first + span.size - 1) };
        }
    }

    for (const DebugLine & line : file.lines)
    {
        SourcePosition  position = { line.file, line.line, line.type, line.depth };

        for (int span : line.spans)
        {
            auto  found = spanRange.find (span);

            if (found == spanRange.end())
            {
                continue;
            }

            m_byLine[{ line.file, line.line }].push_back (found->second);

            for (uint32_t address = found->second.first; address <= found->second.second; address++)
            {
                m_byAddress[address].push_back (position);
            }
        }
    }

    for (std::vector<SourcePosition> & positions : m_byAddress)
    {
        std::stable_sort (positions.begin(), positions.end(),
                          [] (const SourcePosition & a, const SourcePosition & b) { return a.depth < b.depth; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable::GetPositionsAt
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<SourcePosition> & LineTable::GetPositionsAt (Word address) const
{
    static const std::vector<SourcePosition>  s_none;



    return m_byAddress.empty() ? s_none : m_byAddress[address];
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable::GetRanges
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<Word, Word>> LineTable::GetRanges (int file, int line) const
{
    auto  found = m_byLine.find ({ file, line });



    return (found != m_byLine.end()) ? found->second : std::vector<std::pair<Word, Word>>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LineTable::GetNextLineWithCode
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> LineTable::GetNextLineWithCode (int file, int line) const
{
    auto  found = m_byLine.lower_bound ({ file, line });



    if (found == m_byLine.end() || found->first.first != file)
    {
        return std::nullopt;
    }

    return found->first.second;
}
