#include "Pch.h"

#include "FileTypes/PackBytes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PackBytes::Unpack
//
//  A run cut short by the end of the input expands as far as it goes.
//
////////////////////////////////////////////////////////////////////////////////

size_t PackBytes::Unpack (std::span<const Byte> in, size_t limit, std::vector<Byte> & outBytes)
{
    size_t  at = 0;



    while (at < in.size() && outBytes.size() < limit)
    {
        Byte    flag  = in[at++];
        size_t  count = (size_t) (flag & 0x3F) + 1;

        switch (flag >> 6)
        {
            case 0:
                for (size_t i = 0; i < count && at < in.size(); i++)
                {
                    outBytes.push_back (in[at++]);
                }

                break;

            case 1:
                if (at < in.size())
                {
                    outBytes.insert (outBytes.end(), count, in[at++]);
                }

                break;

            case 2:
                if (at + 4 <= in.size())
                {
                    for (size_t i = 0; i < count; i++)
                    {
                        outBytes.insert (outBytes.end(), in.begin() + (ptrdiff_t) at, in.begin() + (ptrdiff_t) at + 4);
                    }
                }

                at += 4;
                break;

            default:
                if (at < in.size())
                {
                    outBytes.insert (outBytes.end(), count * 4, in[at++]);
                }

                break;
        }
    }

    if (outBytes.size() > limit)
    {
        outBytes.resize (limit);
    }

    return std::min (at, in.size());
}
