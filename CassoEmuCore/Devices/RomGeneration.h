#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RomGeneration
//
//  A count every ROM image change adds one to: a ROM device made, a debugger
//  patch, or a $Cxxx or //c bank image replaced, which a //c bank switch also
//  does. A hash over the ROMs taken at one count still holds while the count
//  is unchanged. The count is shared by every machine, so a change in one
//  only makes another hash its ROMs again.
//
////////////////////////////////////////////////////////////////////////////////

class RomGeneration
{
public:
    static uint64_t  Get  () noexcept { return s_count.load (std::memory_order_relaxed); }
    static void      Bump () noexcept { s_count.fetch_add (1, std::memory_order_relaxed); }

private:
    static inline std::atomic<uint64_t>  s_count = 1;
};