#include "Pch.h"

#include "Debugger/Reverse/StateDiff.h"





//  A section's header: its tag, its version and its payload's size, as
//  StateWriter::BeginSection writes them.
static constexpr size_t  s_kHeaderBytes = sizeof (uint32_t) + sizeof (uint16_t) + sizeof (uint32_t);

//  The most bytes of fields a payload's sections are looked for after.
static constexpr size_t  s_kMaxHeaderBytes = 256;





////////////////////////////////////////////////////////////////////////////////
//
//  StateDiff::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string StateDiff::Describe (const std::vector<Byte> & expected, const std::vector<Byte> & actual)
{
    std::string  path;



    if (expected == actual)
    {
        return std::string();
    }

    DescribeIn (expected, 0, expected.size(), actual, 0, actual.size(), path);

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateDiff::DescribeIn
//
//  The first section whose bytes differ, or whose tag or size does, is
//  searched in turn; bytes that aren't made of sections are compared as
//  they are.
//
////////////////////////////////////////////////////////////////////////////////

void StateDiff::DescribeIn (
    const std::vector<Byte>  & expected,
    size_t                     expectedFirst,
    size_t                     expectedSize,
    const std::vector<Byte>  & actual,
    size_t                     actualFirst,
    size_t                     actualSize,
    std::string              & path)
{
    std::vector<Section>  expectedSections;
    std::vector<Section>  actualSections;
    size_t                i      = 0;
    size_t                common = std::min (expectedSize, actualSize);
    size_t                header = 0;



    //  A payload can start with a header of its own fields before its
    //  sections, as the machine's does; the header is the bytes before the
    //  first offset from which the rest splits into sections.
    for (header = 0; header < std::min (common, s_kMaxHeaderBytes); header++)
    {
        if (TrySplit (expected, expectedFirst + header, expectedSize - header, expectedSections) &&
            TrySplit (actual,   actualFirst   + header, actualSize   - header, actualSections))
        {
            break;
        }

        expectedSections.clear();
    }

    if (header > 0 && !expectedSections.empty() &&
        !std::equal (expected.begin() + expectedFirst, expected.begin() + expectedFirst + header, actual.begin() + actualFirst))
    {
        path += std::format (" header of {} bytes", header);
        expectedSections.clear();
    }

    if (!expectedSections.empty())
    {
        for (i = 0; i < std::min (expectedSections.size(), actualSections.size()); i++)
        {
            const Section  & e = expectedSections[i];
            const Section  & a = actualSections[i];

            if (e.tag != a.tag)
            {
                path += std::format ("{}section {} is {} here, {} recorded", path.empty() ? "" : " > ", i, GetTagText (a.tag), GetTagText (e.tag));
                return;
            }

            if (e.size == a.size && std::equal (expected.begin() + e.payload, expected.begin() + e.payload + e.size, actual.begin() + a.payload))
            {
                continue;
            }

            path += (path.empty() ? "" : " > ") + GetTagText (e.tag);

            DescribeIn (expected, e.payload, e.size, actual, a.payload, a.size, path);
            return;
        }

        path += std::format ("{}{} sections here, {} recorded", path.empty() ? "" : " > ", actualSections.size(), expectedSections.size());
        return;
    }

    for (i = 0; i < common; i++)
    {
        if (expected[expectedFirst + i] != actual[actualFirst + i])
        {
            path += std::format (" at {} of {}", i, expectedSize);
            return;
        }
    }

    path += std::format (" {} bytes here, {} recorded", actualSize, expectedSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateDiff::TrySplit
//
//  Every section must fit, and the last must end where the bytes do.
//
////////////////////////////////////////////////////////////////////////////////

bool StateDiff::TrySplit (const std::vector<Byte> & bytes, size_t first, size_t size, std::vector<Section> & outSections)
{
    size_t  at  = first;
    size_t  end = first + size;



    outSections.clear();

    while (at < end)
    {
        Section   section;
        uint32_t  payload = 0;

        if (end - at < s_kHeaderBytes)
        {
            return false;
        }

        memcpy (&section.tag, &bytes[at], sizeof (uint32_t));
        memcpy (&payload, &bytes[at + sizeof (uint32_t) + sizeof (uint16_t)], sizeof (uint32_t));

        section.payload = at + s_kHeaderBytes;
        section.size    = payload;

        if (section.size > end - section.payload)
        {
            return false;
        }

        outSections.push_back (section);
        at = section.payload + section.size;
    }

    return !outSections.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateDiff::GetTagText
//
//  Its four characters when they are all printable, as the tags are; its
//  hex value otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::string StateDiff::GetTagText (uint32_t tag)
{
    std::string  text;



    for (int shift = 0; shift < 32; shift += 8)
    {
        char  c = (char) ((tag >> shift) & 0xFF);

        if (c < 0x20 || c > 0x7E)
        {
            return std::format ("${:08X}", tag);
        }

        text += c;
    }

    return text;
}
