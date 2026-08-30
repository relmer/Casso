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
        wchar_t   path[512] = {};
        FILE    * file      = nullptr;

        if (GetEnvironmentVariableW (L"CASSO_STARTUP_TRACE", path, 512) == 0 || Marks().empty())
        {
            return;
        }

        LARGE_INTEGER   freq = {};
        QueryPerformanceFrequency (&freq);

        // Plain stdio rather than a stream: this header is included from both
        // Dxui and Casso, whose precompiled headers do not agree on which of
        // <fstream>/<iomanip> they carry, and scaffolding is a poor reason to
        // widen either one.
        if (_wfopen_s (&file, path, L"w") != 0 || file == nullptr)
        {
            return;
        }

        fprintf (file, "elapsed_ms  delta_ms  label\n");

        long long   base = Marks().front().qpc;
        long long   prev = base;

        for (const Mark & mark : Marks())
        {
            double   elapsed = (double) (mark.qpc - base) * 1000.0 / (double) freq.QuadPart;
            double   delta   = (double) (mark.qpc - prev) * 1000.0 / (double) freq.QuadPart;

            fprintf (file, "%10.1f  %8.1f  %s\n", elapsed, delta, mark.label);

            prev = mark.qpc;
        }

        fclose (file);
    }
}
