#include "Pch.h"

#include "Seams/IProcessProbe.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ProcessProbe::TryGetStartTime
//
//  A process that has exited, while another program has its handle open,
//  has an exit code other than STILL_ACTIVE, and is not running.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ProcessProbe::TryGetStartTime (DWORD pid, uint64_t & outStarted)
{
    HANDLE    process  = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    FILETIME  created  = {};
    FILETIME  exited   = {};
    FILETIME  kernel   = {};
    FILETIME  user     = {};
    DWORD     exitCode = 0;
    bool      running  = false;



    if (process != nullptr)
    {
        running = GetExitCodeProcess (process, &exitCode) && exitCode == STILL_ACTIVE
               && GetProcessTimes (process, &created, &exited, &kernel, &user);

        CloseHandle (process);
    }

    outStarted = ((uint64_t) created.dwHighDateTime << 32) | created.dwLowDateTime;

    return running;
}
