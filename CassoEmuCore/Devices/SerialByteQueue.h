#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialByteQueue
//
//  Bytes an attachment has sent toward a serial chip that the chip has not
//  yet received. Any thread may push: a socket or COM-port reader runs on its
//  own thread. Only the emulation thread pops, one byte per character time.
//
//  The count is atomic so the emulation thread's per-instruction idle test is
//  one load rather than a lock.
//
////////////////////////////////////////////////////////////////////////////////

class SerialByteQueue
{
public:
    void    Push     (const Byte * bytes, size_t count);
    bool    TryPop   (Byte & outValue);
    void    Clear    ();

    //  The bytes waiting, oldest first, for a machine state.
    void    CopyTo   (std::vector<Byte> & outBytes) const;
    size_t  GetCount () const { return m_count.load (memory_order_acquire); }

private:
    mutable mutex    m_lock;
    deque<Byte>      m_bytes;
    atomic<size_t>   m_count = 0;
};
