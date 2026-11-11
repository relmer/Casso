#include "Pch.h"

#include "Debugger/DisassemblyLineList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyLineList::Build
//
//  The known starts first: a debug file line counts only where its bytes are
//  memory's now, so an overwritten part of a program decodes as it is.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyLineList::Build (const Inputs & inputs)
{
    static constexpr uint32_t  kBytes  = (uint32_t) DebugMemoryImage::kBytes;
    const DebugMemoryImage   & image   = *inputs.image;
    std::vector<uint8_t>       known (kBytes, 0);
    std::vector<uint8_t>       debug (kBytes, 0);
    uint32_t                   address = 0;



    m_lines.clear();
    m_lines.reserve (kBytes / 2);
    m_version++;

    for (const ProgramCheck::Entry & entry : inputs.debugLines)
    {
        bool  isMatch = !entry.bytes.empty() && (uint32_t) entry.address + entry.bytes.size() <= kBytes;



        for (size_t each = 0; each < entry.bytes.size() && isMatch; each++)
        {
            isMatch = image.bytes[(size_t) entry.address + each] == entry.bytes[each];
        }

        if (isMatch && !entry.mnemonic.empty())
        {
            known[entry.address] = 1;
            debug[entry.address] = 1;
        }
    }

    for (Word start : inputs.starts)
    {
        known[start] = 1;
    }

    for (uint32_t at = 0; at < kBytes; at++)
    {
        known[at] = known[at] | (image.fetched[at] ? 1 : 0);
    }

    while (address < kBytes)
    {
        Line  line;
        bool  isMarked = false;
        int   dataLen  = inputs.dataLine ? inputs.dataLine ((Word) address, isMarked) : 0;



        line.address = (Word) address;

        if (dataLen > 0)
        {
            line.isData = true;
            line.length = (uint8_t) std::min<uint32_t> ((uint32_t) dataLen, kBytes - address);
            line.source = isMarked ? Source::Marked : Source::DebugFile;
        }
        else
        {
            uint32_t  length = std::max<uint32_t> (1, inputs.lengths[image.bytes[address]]);



            length      = std::min (length, kBytes - address);
            line.source = debug[address] ? Source::DebugFile : known[address] ? Source::Fetched : Source::Decoded;

            //  A decode running over a known start stops short of it, as data.
            for (uint32_t inside = 1; inside < length; inside++)
            {
                if (known[address + inside] && !known[address])
                {
                    length      = inside;
                    line.isData = true;
                    break;
                }
            }

            line.length = (uint8_t) length;
        }

        m_lines.push_back (line);
        address += line.length;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyLineList::FindLine
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyLineList::FindLine (Word address) const
{
    auto  after = std::ranges::upper_bound (m_lines, address, {}, &Line::address);



    if (m_lines.empty())
    {
        return -1;
    }

    return (after == m_lines.begin()) ? 0 : (int) (after - m_lines.begin()) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyLineList::StepLines
//
////////////////////////////////////////////////////////////////////////////////

Word DisassemblyLineList::StepLines (Word address, int count) const
{
    int  index = FindLine (address);



    if (index < 0)
    {
        return address;
    }

    index = std::clamp (index + count, 0, (int) m_lines.size() - 1);
    return m_lines[(size_t) index].address;
}
