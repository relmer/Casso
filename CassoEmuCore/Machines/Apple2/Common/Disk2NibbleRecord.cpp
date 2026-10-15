#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleRecord::Clear
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleRecord::Clear()
{
    m_entries        = {};
    m_count          = 0;
    m_pendingNewDisk = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleRecord::SaveState
//
//  The count, the drives whose disk changed since their last entry, then
//  the entries in the ring, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleRecord::SaveState (StateWriter & writer) const
{
    HRESULT   hr    = S_OK;
    uint64_t  index = 0;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteUInt64 (m_count);
    writer.WriteByte   (m_pendingNewDisk);

    for (index = m_count - GetSize(); index < m_count; index++)
    {
        const Entry  & entry = GetEntry (index);



        writer.WriteByte (entry.nibble);
        writer.WriteByte (entry.flags);
        writer.WriteWord (entry.address);
    }

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleRecord::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleRecord::LoadState (StateReader & reader)
{
    constexpr Byte     kDriveBits = 0x03;
    HRESULT            hr         = S_OK;
    uint16_t           version    = 0;
    Disk2NibbleRecord  loaded;
    uint64_t           index      = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt64 (loaded.m_count);
    reader.ReadByte   (loaded.m_pendingNewDisk);

    for (index = loaded.m_count - loaded.GetSize(); index < loaded.m_count; index++)
    {
        Entry  & entry = loaded.m_entries[index % kCapacity];



        reader.ReadByte (entry.nibble);
        reader.ReadByte (entry.flags);
        reader.ReadWord (entry.address);
    }

    hr = reader.EndSection();
    CHR (hr);

    CBREx ((loaded.m_pendingNewDisk & ~kDriveBits) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    *this = loaded;

Error:
    return hr;
}
