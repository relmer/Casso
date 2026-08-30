#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartupTrace  (TEMPORARY INVESTIGATION SCAFFOLD -- not for merge)
//
//  QPC timeline of the startup path. Stamp() is a counter read plus a
//  push_back; Dump() writes the timeline to the path in CASSO_STARTUP_TRACE
//  and does nothing when that variable is unset.
//
////////////////////////////////////////////////////////////////////////////////

namespace StartupTrace
{
    struct Mark { const char * label; long long qpc; };

    inline std::vector<Mark> & Marks()
    {
        static std::vector<Mark> s_marks;
        return s_marks;
    }

    inline void Stamp (const char * label)
    {
        LARGE_INTEGER now = {};
        QueryPerformanceCounter (&now);
        Marks().push_back (Mark { label, now.QuadPart });
    }

    inline void Dump()
    {
        wchar_t  path[512] = {};

        if (GetEnvironmentVariableW (L"CASSO_STARTUP_TRACE", path, 512) == 0)
        {
            return;
        }

        LARGE_INTEGER  freq = {};
        QueryPerformanceFrequency (&freq);

        std::ofstream  out (path);

        if (!out.good() || Marks().empty())
        {
            return;
        }

        long long  base = Marks().front().qpc;
        long long  prev = base;

        out << "elapsed_ms  delta_ms  label\n";

        for (const Mark & m : Marks())
        {
            double  elapsed = (double) (m.qpc - base) * 1000.0 / (double) freq.QuadPart;
            double  delta   = (double) (m.qpc - prev) * 1000.0 / (double) freq.QuadPart;

            out << std::fixed << std::setprecision (1)
                << std::setw (10) << elapsed << "  "
                << std::setw (8)  << delta   << "  "
                << m.label << "\n";

            prev = m.qpc;
        }
    }
}
