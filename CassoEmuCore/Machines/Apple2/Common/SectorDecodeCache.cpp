#include "Pch.h"

#include "Machines/Apple2/Common/SectorDecodeCache.h"





std::mutex                           SectorDecodeCache::s_lock;
std::deque<SectorDecodeCache::Entry> SectorDecodeCache::s_entries;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeCache::Find
//
//  A hit moves to the front.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorDecodeCache::Find (const std::vector<Byte> & fileBytes, HRESULT & outHr, std::vector<Byte> & outSectors, SectorDecodeReport & outReport)
{
    uint64_t                     hash  = Hash (fileBytes);
    std::lock_guard<std::mutex>  lock (s_lock);
    bool                         found = false;



    for (auto each = s_entries.begin(); each != s_entries.end(); ++each)
    {
        if (each->hash != hash || each->size != fileBytes.size())
        {
            continue;
        }

        outHr      = each->hr;
        outSectors = each->sectors;
        outReport  = each->report;
        found      = true;

        if (each != s_entries.begin())
        {
            Entry  hit = std::move (*each);

            s_entries.erase (each);
            s_entries.push_front (std::move (hit));
        }

        break;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeCache::Store
//
//  At the front, dropping the least recently used past kEntries.
//
////////////////////////////////////////////////////////////////////////////////

void SectorDecodeCache::Store (const std::vector<Byte> & fileBytes, HRESULT hr, const std::vector<Byte> & sectors, const SectorDecodeReport & report)
{
    Entry                        entry;
    std::lock_guard<std::mutex>  lock (s_lock);



    entry.hash    = Hash (fileBytes);
    entry.size    = fileBytes.size();
    entry.hr      = hr;
    entry.sectors = sectors;
    entry.report  = report;

    s_entries.push_front (std::move (entry));

    while (s_entries.size() > kEntries)
    {
        s_entries.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeCache::Hash
//
//  FNV-1a over the whole file, eight bytes at a time.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t SectorDecodeCache::Hash (const std::vector<Byte> & bytes)
{
    static constexpr uint64_t  s_kOffset = 0xcbf29ce484222325ull;
    static constexpr uint64_t  s_kPrime  = 0x00000100000001b3ull;
    uint64_t                   hash      = s_kOffset;
    size_t                     whole     = bytes.size() / sizeof (uint64_t);
    uint64_t                   word      = 0;



    for (size_t i = 0; i < whole; i++)
    {
        std::memcpy (&word, bytes.data() + i * sizeof (uint64_t), sizeof (word));
        hash = (hash ^ word) * s_kPrime;
    }

    for (size_t i = whole * sizeof (uint64_t); i < bytes.size(); i++)
    {
        hash = (hash ^ bytes[i]) * s_kPrime;
    }

    return hash;
}
