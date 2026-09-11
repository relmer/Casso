#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoTarget
//
//  The emulator an insert goes to: the one that launched the browser, a
//  running one, or a new one; and what is known about its machine.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoTarget
{
    enum class Kind { Owner, Running, Launch };

    Kind         kind        = Kind::Launch;
    HWND         hwnd        = nullptr;
    std::string  machineName;
    int          driveCount  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoTargeting
//
//  Which emulator receives a disk, decided from facts the caller supplies.
//
//  PURE ON PURPOSE. Whether a window is alive and which windows are running
//  are questions for the shell, which asks the desktop; this only ranks the
//  answers, so the ranking can be tested with a list of made-up handles.
//
//  The drive count and machine name are the default machine's until the
//  target answers a describe request; the caller overwrites them from the
//  reply when one arrives.
//
////////////////////////////////////////////////////////////////////////////////

class CassoTargeting
{
public:
    //  `running` is ordered most recently active first.
    static CassoTarget  Choose (HWND                   owner,
                                bool                   ownerAlive,
                                std::span<const HWND>  running,
                                const MachineConfig  & defaultMachine);
};
