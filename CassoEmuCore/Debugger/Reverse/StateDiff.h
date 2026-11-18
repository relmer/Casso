#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateDiff
//
//  Where two saved machine states first differ, by section: the tags of the
//  sections that enclose the first differing byte, outermost first, and the
//  byte's offset in the innermost one's payload, as "MOCK > AY01 at 12 of
//  340". A section's payload is searched for sections inside it when it is
//  made of nothing else, so the path has a device's sections inside the
//  machine's. For a divergence of history, which device the replay got
//  wrong.
//
////////////////////////////////////////////////////////////////////////////////

class StateDiff
{
public:
    //  Empty when the two are the same.
    static std::string  Describe (const std::vector<Byte> & expected, const std::vector<Byte> & actual);

private:
    struct Section
    {
        uint32_t  tag     = 0;
        size_t    payload = 0;     // where its payload starts
        size_t    size    = 0;     // the payload's size
    };

    //  The bytes at [first, first + size) split into sections, when they are
    //  made of sections and nothing else.
    static bool         TrySplit     (const std::vector<Byte> & bytes, size_t first, size_t size, std::vector<Section> & outSections);
    static std::string  GetTagText   (uint32_t tag);
    static void         DescribeIn   (const std::vector<Byte> & expected, size_t expectedFirst, size_t expectedSize,
                                      const std::vector<Byte> & actual,   size_t actualFirst,   size_t actualSize,
                                      std::string & path);
};
