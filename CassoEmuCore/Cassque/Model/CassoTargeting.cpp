#include "Pch.h"

#include "Cassque/Model/CassoTargeting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoTargeting::Choose
//
//  The owner when it is alive, else the most recently active running
//  emulator, else a launch with the default machine's own drive count.
//
////////////////////////////////////////////////////////////////////////////////

CassoTarget CassoTargeting::Choose (
    HWND                   owner,
    bool                   ownerAlive,
    std::span<const HWND>  running,
    const MachineConfig  & defaultMachine)
{
    CassoTarget  target;



    target.machineName = defaultMachine.name;
    target.driveCount  = defaultMachine.AttachedDiskIiDriveCount();

    if (owner != nullptr && ownerAlive)
    {
        target.kind = CassoTarget::Kind::Owner;
        target.hwnd = owner;
    }
    else if (!running.empty())
    {
        target.kind = CassoTarget::Kind::Running;
        target.hwnd = running[0];
    }
    else
    {
        target.kind = CassoTarget::Kind::Launch;
        target.hwnd = nullptr;
    }

    return target;
}
