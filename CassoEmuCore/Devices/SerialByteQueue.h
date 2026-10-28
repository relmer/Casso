#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialByteQueue
//
//  Bytes an attachment has sent toward a serial chip that the chip has not
//  yet received, each with the receive errors it arrived with (the 6551's
//  parity, framing and overrun status bits). Any thread may push: a byte
//  source endpoint pushes from the thread that starts it. Only the emulation
//  thread pops, one byte per character time.
//
//  The count is atomic so the emulation thread's per-instruction idle test is
//  one load rather than a lock.
//
////////////////////////////////////////////////////////////////////////////////

class SerialByteQueue
{
public:
    struct Entry
    {
        Byte  value  = 0;
        Byte  errors = 0;
    };

    void    Push      (const Byte * bytes, size_t count);
    void    PushEntry (Byte value, Byte errors);
    bool    TryPop    (Byte & outValue);
    bool    TryPop    (Byte & outValue, Byte & outErrors);
    void    Clear     ();

    //  The entries waiting, oldest first, for a machine state.
    void    CopyTo    (std::vector<Entry> & outEntries) const;
    size_t  GetCount  () const { return m_count.load (memory_order_acquire); }

private:
    mutable mutex    m_lock;
    deque<Entry>     m_entries;
    atomic<size_t>   m_count = 0;
};