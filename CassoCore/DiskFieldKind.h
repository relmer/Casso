#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldKind
//
//  Which of the two standard Disk II sector formats a field belongs to.
//  Sixteen is DOS 3.3 and ProDOS (address prologue D5 AA 96, 6-and-2 data);
//  Thirteen is DOS 3.2 and earlier (address prologue D5 AA B5, 5-and-3 data).
//
//  In CassoCore because the debugger's disk breakpoints and the disk inspector
//  both speak it, and the breakpoint parser lives here.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskFieldKind
{
    Sixteen,
    Thirteen,
};
