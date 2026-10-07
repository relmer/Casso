#pragma once

#include "Pch.h"

#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TestZipBuilder
//
//  Builds a zip of stored (method 0) entries in memory, laid out as PKWARE's
//  APPNOTE describes: local headers and data, the central directory, then
//  the end record. Each entry's header fields can be overridden afterward
//  to make a corrupt archive.
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class TestZipBuilder
{
public:
    struct Entry
    {
        std::string         path;
        std::vector<Byte>   data;
        std::uint16_t       flags    = 0;
        std::uint16_t       method   = 0;
        std::uint32_t       crc32    = 0;
        bool                isCrcSet = false;
    };

    std::vector<Entry>  entries;

    Entry & Add (const std::string & path, const std::string & content)
    {
        Entry  entry;

        entry.path = path;
        entry.data.assign (content.begin(), content.end());
        entries.push_back (std::move (entry));

        return entries.back();
    }

    std::vector<Byte> Build() const
    {
        std::vector<Byte>           out;
        std::vector<std::uint32_t>  offsets;
        std::uint32_t               cdStart = 0;
        std::uint32_t               cdSize  = 0;
        std::uint32_t               crc     = 0;

        for (const Entry & entry : entries)
        {
            crc = entry.isCrcSet ? entry.crc32 : ZipArchive::ComputeCrc32 (entry.data);
            offsets.push_back ((std::uint32_t) out.size());

            Put32 (out, 0x04034B50);
            Put16 (out, 20);
            Put16 (out, entry.flags);
            Put16 (out, entry.method);
            Put32 (out, 0);
            Put32 (out, crc);
            Put32 (out, (std::uint32_t) entry.data.size());
            Put32 (out, (std::uint32_t) entry.data.size());
            Put16 (out, (std::uint16_t) entry.path.size());
            Put16 (out, 0);
            out.insert (out.end(), entry.path.begin(), entry.path.end());
            out.insert (out.end(), entry.data.begin(), entry.data.end());
        }

        cdStart = (std::uint32_t) out.size();

        for (size_t i = 0; i < entries.size(); i++)
        {
            const Entry  & entry = entries[i];

            crc = entry.isCrcSet ? entry.crc32 : ZipArchive::ComputeCrc32 (entry.data);

            Put32 (out, 0x02014B50);
            Put16 (out, 20);
            Put16 (out, 20);
            Put16 (out, entry.flags);
            Put16 (out, entry.method);
            Put32 (out, 0);
            Put32 (out, crc);
            Put32 (out, (std::uint32_t) entry.data.size());
            Put32 (out, (std::uint32_t) entry.data.size());
            Put16 (out, (std::uint16_t) entry.path.size());
            Put16 (out, 0);
            Put16 (out, 0);
            Put16 (out, 0);
            Put16 (out, 0);
            Put32 (out, 0);
            Put32 (out, offsets[i]);
            out.insert (out.end(), entry.path.begin(), entry.path.end());
        }

        cdSize = (std::uint32_t) out.size() - cdStart;

        Put32 (out, 0x06054B50);
        Put16 (out, 0);
        Put16 (out, 0);
        Put16 (out, (std::uint16_t) entries.size());
        Put16 (out, (std::uint16_t) entries.size());
        Put32 (out, cdSize);
        Put32 (out, cdStart);
        Put16 (out, 0);

        return out;
    }

private:
    static void Put16 (std::vector<Byte> & out, std::uint16_t value)
    {
        out.push_back ((Byte) value);
        out.push_back ((Byte) (value >> 8));
    }

    static void Put32 (std::vector<Byte> & out, std::uint32_t value)
    {
        Put16 (out, (std::uint16_t) value);
        Put16 (out, (std::uint16_t) (value >> 16));
    }
};
