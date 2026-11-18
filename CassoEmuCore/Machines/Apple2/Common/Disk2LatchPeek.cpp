#include "Pch.h"

#include "Machines/Apple2/Common/Disk2LatchPeek.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2LatchPeek::Peek
//
//  The latch `delta` cycles after the access at `sync`. The copy is taken
//  again when the drive or the access it was taken at differs, when the
//  drive has moved in any way since (IsSameReadState), or when the peek is
//  for an earlier cycle than the copy has been run on to.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2LatchPeek::Peek (const Disk2NibbleEngine & engine, int drive, uint64_t sync, uint64_t delta)
{
    bool  isSame = m_drive == drive && m_sync == sync && delta >= m_delta && engine.IsSameReadState (m_base);



    if (!isSame)
    {
        m_base   = engine.MakePeekCopy();
        m_engine = m_base;
        m_drive  = drive;
        m_sync   = sync;
        m_delta  = 0;
    }

    m_engine.AdvancePeekCopy (static_cast<uint32_t> (delta - m_delta));
    m_delta = delta;

    return m_engine.PeekReadLatch();
}
