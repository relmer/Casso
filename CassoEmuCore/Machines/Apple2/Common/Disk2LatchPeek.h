#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Disk2NibbleEngine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2LatchPeek
//
//  The data latch a read would return, from a copy of the drive run on from
//  the last access, so a peek moves nothing. The copy is stored for the next
//  peek, so a peek runs only the cycles since the one before it: the drive
//  as the copy was taken, the copy as run on, the drive and access it was
//  taken at, and how far it has been run on. The debugger peeks every frame,
//  and running a copy from the last access each time cost more with every
//  frame after the guest stopped reading the disk.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2LatchPeek
{
public:
    Byte  Peek (const Disk2NibbleEngine & engine, int drive, uint64_t sync, uint64_t delta);

private:
    Disk2NibbleEngine  m_base;
    Disk2NibbleEngine  m_engine;
    int                m_drive = -1;
    uint64_t           m_sync  = 0;
    uint64_t           m_delta = 0;
};
