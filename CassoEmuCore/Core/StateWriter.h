#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateWriter
//
//  Builds a machine state blob: a run of tagged, versioned sections holding
//  fixed-width little-endian values. See IMachineState.h for the pattern every
//  stateful part follows.
//
//  A section on the stream is its tag (4 bytes), its version (2 bytes) and the
//  byte count of its payload (4 bytes), then the payload. BeginSection writes
//  the header with a zero size and EndSection patches the size in, so sections
//  nest freely.
//
////////////////////////////////////////////////////////////////////////////////

class StateWriter
{
public:
    static constexpr size_t   kSectionHeaderSize = sizeof (uint32_t) + sizeof (uint16_t) + sizeof (uint32_t);

    void                      BeginSection (uint32_t tag, uint16_t version);
    HRESULT                   EndSection   ();

    void                      WriteByte    (Byte value);
    void                      WriteBool    (bool value);
    void                      WriteWord    (Word value);
    void                      WriteUInt32  (uint32_t value);
    void                      WriteUInt64  (uint64_t value);
    void                      WriteBytes   (const Byte * data, size_t count);

    const std::vector<Byte> & GetBytes        () const { return m_bytes; }
    bool                      HasOpenSection  () const { return !m_openSections.empty(); }

private:
    void                      WriteLittleEndian (uint64_t value, size_t byteCount);

    std::vector<Byte>         m_bytes;
    std::vector<size_t>       m_openSections;   // stream offset of each open section's payload
};
