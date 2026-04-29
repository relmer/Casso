#include "Pch.h"

#include "MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBus
//
////////////////////////////////////////////////////////////////////////////////

MemoryBus::MemoryBus ()
    : m_floatingBusValue (0xFF)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadByte (Word address)
{
    MemoryDevice * device = FindDevice (address);

    if (device != nullptr)
    {
        Byte value = device->Read (address);
        m_floatingBusValue = value;
        return value;
    }

    // Unmapped I/O in $C000-$CFFF returns floating bus value
    if (address >= 0xC000 && address <= 0xCFFF)
    {
        return m_floatingBusValue;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::WriteByte (Word address, Byte value)
{
    MemoryDevice * device = FindDevice (address);

    if (device != nullptr)
    {
        device->Write (address, value);
    }

    m_floatingBusValue = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDevice
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::AddDevice (MemoryDevice * device)
{
    BusEntry entry;

    entry.start  = device->GetStart ();
    entry.end    = device->GetEnd ();
    entry.device = device;

    // Insert sorted by start address
    auto it = std::lower_bound (
        m_entries.begin (),
        m_entries.end (),
        entry,
        [] (const BusEntry & a, const BusEntry & b)
        {
            return a.start < b.start;
        });

    m_entries.insert (it, entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveDevice
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::RemoveDevice (MemoryDevice * device)
{
    auto it = std::remove_if (
        m_entries.begin (),
        m_entries.end (),
        [device] (const BusEntry & entry)
        {
            return entry.device == device;
        });

    m_entries.erase (it, m_entries.end ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Validate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBus::Validate () const
{
    HRESULT hr = S_OK;

    for (size_t i = 0; i + 1 < m_entries.size (); i++)
    {
        for (size_t j = i + 1; j < m_entries.size (); j++)
        {
            const BusEntry & a = m_entries[i];
            const BusEntry & b = m_entries[j];

            if (a.start <= b.end && b.start <= a.end)
            {
                DEBUGMSG (L"Address conflict: device at $%04X-$%04X overlaps device at $%04X-$%04X\n",
                    a.start, a.end, b.start, b.end);
                hr = E_FAIL;
                goto Error;
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::Reset ()
{
    for (auto & entry : m_entries)
    {
        entry.device->Reset ();
    }

    m_floatingBusValue = 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDevice
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * MemoryBus::FindDevice (Word address) const
{
    for (const auto & entry : m_entries)
    {
        if (address >= entry.start && address <= entry.end)
        {
            return entry.device;
        }
    }

    return nullptr;
}
