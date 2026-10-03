#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateSegment
//
//  A run of bytes a sharing StateWriter kept by reference instead of copying:
//  it belongs at offset in the writer's own bytes, ahead of whatever was
//  written there after it.
//
////////////////////////////////////////////////////////////////////////////////

struct StateSegment
{
    size_t                                    offset = 0;
    std::shared_ptr<const std::vector<Byte>>  bytes;
};





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
//  A sharing writer keeps the runs given to WriteShared as segments rather
//  than copying them, so a part whose large buffers seldom change can hand
//  the same immutable buffer to many saves. The blob is then the writer's own
//  bytes with the segments spliced in (Flatten); section sizes count them.
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
    void                      WriteShared  (const std::shared_ptr<const std::vector<Byte>> & bytes);

    const std::vector<Byte> & GetBytes        () const { return m_bytes; }
    std::vector<Byte>         TakeBytes       ()       { return std::move (m_bytes); }
    std::vector<StateSegment> TakeSegments    ()       { return std::move (m_segments); }
    void                      Reuse           (std::vector<Byte> && buffer);
    bool                      HasOpenSection  () const { return !m_openSections.empty(); }
    void                      SetSharing      (bool isSharing) { m_isSharing = isSharing; }
    bool                      IsSharing       () const { return m_isSharing; }

    static void               Flatten         (const std::vector<Byte> & own, const std::vector<StateSegment> & segments, std::vector<Byte> & outBytes);

private:
    struct OpenSection
    {
        size_t  sizeOffset   = 0;   // where in m_bytes the size field is
        size_t  payloadStart = 0;   // stream offset of the payload, segments counted
    };

    void                      WriteLittleEndian (uint64_t value, size_t byteCount);
    size_t                    GetStreamOffset   () const { return m_bytes.size() + m_sharedBytes; }

    std::vector<Byte>          m_bytes;
    std::vector<StateSegment>  m_segments;
    std::vector<OpenSection>   m_openSections;
    size_t                     m_sharedBytes = 0;
    bool                       m_isSharing   = false;
};