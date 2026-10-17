#pragma once

#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCaptures
//
//  Reads the serial logs in UnitTest/Fixtures/CassoLink/. A run is the bytes
//  one side sent before the other replied: '>' from the Apple, '<' to it.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkRun
{
    char               direction = 0;
    uint64_t           cycle     = 0;
    std::vector<Byte>  bytes;
};


class CassoLinkCaptures
{
public:
    static std::vector<CassoLinkRun>  LoadRuns (const std::string & fixture)
    {
        FixtureProvider            fixtures;
        std::vector<uint8_t>       text;
        std::vector<CassoLinkRun>  runs;
        std::istringstream         lines;
        std::string                line;

        AssertSucceeded (fixtures.OpenFixture ("CassoLink/" + fixture, text));
        lines.str (std::string (text.begin(), text.end()));

        while (std::getline (lines, line))
        {
            std::istringstream  fields (line);
            std::string         cycle;
            std::string         port;
            std::string         mark;
            std::string         hex;

            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            fields >> cycle >> port >> mark;

            if (runs.empty() || runs.back().direction != mark[0])
            {
                runs.push_back ({ mark[0], std::stoull (cycle), {} });
            }

            while (fields >> hex)
            {
                runs.back().bytes.push_back ((Byte) std::stoul (hex, nullptr, 16));
            }
        }

        return runs;
    }

    static std::vector<Byte>  LoadDirection (const std::string & fixture, char direction)
    {
        std::vector<Byte>  bytes;

        for (const CassoLinkRun & run : LoadRuns (fixture))
        {
            if (run.direction == direction)
            {
                bytes.insert (bytes.end(), run.bytes.begin(), run.bytes.end());
            }
        }

        return bytes;
    }

    //  The image in an ADTPro packet stream's image data, in block order; see
    //  CassoLinkHost::ToFileOrder for the .dsk file.
    static std::vector<Byte>  RebuildImage (const std::string & fixture, char direction, size_t blocks)
    {
        std::vector<Byte>  stream = LoadDirection (fixture, direction);
        std::vector<Byte>  image (blocks * 512, 0);
        size_t             pos    = 0;

        while (pos < stream.size())
        {
            CassoLinkPacket  packet;
            size_t           consumed = 0;
            bool             complete = false;
            HRESULT          hr       = stream[pos] == CassoLinkCodec::kStart ? CassoLinkCodec::Decode (std::span<const Byte> (stream).subspan (pos), consumed, packet, complete) : E_FAIL;

            hr = (hr == S_OK && !complete) ? E_FAIL : hr;

            if (hr == S_OK && packet.command == CassoLinkCodec::kImageData)
            {
                size_t  start = (size_t) packet.payload[0] | ((size_t) packet.payload[1] << 8);

                std::copy (packet.payload.begin() + 2, packet.payload.end(), image.begin() + (ptrdiff_t) (start * 512));
            }

            pos += (hr == S_OK) ? consumed : 1;
        }

        return image;
    }
};
