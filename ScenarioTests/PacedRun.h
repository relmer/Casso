#pragma once

#include "Pch.h"

#include "TestMachine.h"
#include "Devices/Network/W5100.h"
#include "Machines/Apple2/Common/UthernetCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PacedRun
//
//  Emulated Apples with an Uthernet II, run as fast as the PC allows except
//  while one is waiting on the PC. A slice in which a guest polled an empty
//  receive buffer takes at least its emulated time in wall time, so the
//  guest's timeouts, which count cycles, measure real time while the PC
//  works on a reply. Booting, disk access and the guest's computation are not
//  slowed.
//
////////////////////////////////////////////////////////////////////////////////

class PacedRun
{
public:
    static constexpr uint64_t  kSliceCycles = 10'230;   // 10 ms at 1.023 MHz
    static constexpr DWORD     kSliceMs     = 10;

    //  One slice of each machine; true when a guest polled an empty buffer.
    static bool  Step (std::span<TestMachine * const> machines)
    {
        bool       waited = false;
        ULONGLONG  start  = GetTickCount64();



        for (TestMachine * machine : machines)
        {
            UthernetCard  * card   = machine->GetRefs().uthernet;
            uint64_t        before = (card != nullptr) ? card->GetChip().GetEmptyReceivePolls() : 0;

            machine->RunCycles (kSliceCycles);

            waited = waited || (card != nullptr && card->GetChip().GetEmptyReceivePolls() != before);
        }

        if (waited && GetTickCount64() - start < kSliceMs)
        {
            Sleep ((DWORD) (kSliceMs - (GetTickCount64() - start)));
        }

        return waited;
    }

    //  Slices until `done` or `ms` of wall time pass.
    static void  RunUntil (std::span<TestMachine * const> machines, const std::function<bool()> & done, DWORD ms)
    {
        ULONGLONG  start = GetTickCount64();



        while (!done() && GetTickCount64() - start < ms)
        {
            Step (machines);
        }
    }
};