#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateSegment
//
//  A run of bytes a sharing StateWriter kept by reference instead of copying:
//  it belongs at offset in the writer's own bytes, ahead of whatever was
//  written there after it. source is the live buffer the run stands for,
//  which holds the same bytes at the moment of the save unless a write to it
//  was missed; MachineHost::CheckSharedSave compares the two right after the
//  save, and nothing may read through it once the machine runs on.
//
////////////////////////////////////////////////////////////////////////////////

struct StateSegment
{
    size_t                                      offset = 0;
    std::shared_ptr<const std::vector<Byte>>    bytes;
    const Byte                                * source = nullptr;
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
//  The writer keeps its own byte count apart from its buffer's size: a reused
//  buffer keeps the length of the save it last held, so a save of the same
//  machine writes over it without growing or zero-filling it first, and the
//  buffer is cut to the count when it is handed out.
//
////////////////////////////////////////////////////////////////////////////////

class StateWriter
{
public:
    static constexpr size_t   kSectionHeaderSize = sizeof (uint32_t) + sizeof (uint16_t) + sizeof (uint32_t);

    void                      BeginSection (uint32_t tag, uint16_t version);
    HRESULT                   EndSection   ();

    void                      WriteByte    (Byte value)     { *GetRoom (sizeof (value)) = value; }
    void                      WriteBool    (bool value)     { *GetRoom (sizeof (Byte)) = value ? 1 : 0; }
    void                      WriteWord    (Word value)     { WriteLittleEndian (value, sizeof (value)); }
    void                      WriteUInt32  (uint32_t value) { WriteLittleEndian (value, sizeof (value)); }
    void                      WriteUInt64  (uint64_t value) { WriteLittleEndian (value, sizeof (value)); }
    void                      WriteBytes   (const Byte * data, size_t count);
    void                      WriteShared  (const std::shared_ptr<const std::vector<Byte>> & bytes, const Byte * source);

    const std::vector<Byte> & GetBytes        () const;
    std::vector<Byte>         TakeBytes       ();
    std::vector<StateSegment> TakeSegments    ()       { return std::move (m_segments); }
    void                      Reuse           (std::vector<Byte> && buffer);
    void                      Reuse           (std::vector<Byte> && buffer, std::vector<StateSegment> && segments);
    bool                      HasOpenSection  () const { return !m_openSections.empty(); }
    void                      SetSharing      (bool isSharing) { m_isSharing = isSharing; }
    bool                      IsSharing       () const { return m_isSharing; }

    void                      FlattenInto     (std::vector<Byte> & outBytes) const;
    static void               Flatten         (const std::vector<Byte> & own, const std::vector<StateSegment> & segments, std::vector<Byte> & outBytes);

    // The runs kept so far, for a check made right after the save.
    const std::vector<StateSegment> & GetSegments () const { return m_segments; }

private:
    static constexpr size_t   kSegmentReserve = 64;

    struct OpenSection
    {
        size_t  sizeOffset   = 0;   // where in m_bytes the size field is
        size_t  payloadStart = 0;   // stream offset of the payload, segments counted
    };

    //  The next count bytes of the stream, to be written through the pointer.
    Byte *                    GetRoom           (size_t count)
    {
        Byte  * room = nullptr;



        if (m_size + count > m_bytes.size())
        {
            Grow (count);
        }

        room    = m_bytes.data() + m_size;
        m_size += count;

        return room;
    }

    void                      WriteLittleEndian (uint64_t value, size_t byteCount)
    {
        Byte    * out = GetRoom (byteCount);
        size_t    i   = 0;



        for (i = 0; i < byteCount; i++)
        {
            out[i] = static_cast<Byte> (value >> (i * CHAR_BIT));
        }
    }

    void                      Grow              (size_t count);
    size_t                    GetStreamOffset   () const { return m_size + m_sharedBytes; }

    mutable std::vector<Byte>  m_bytes;                 // cut to m_size when handed out
    std::vector<StateSegment>  m_segments;
    std::vector<OpenSection>   m_openSections;
    size_t                     m_size        = 0;       // bytes of m_bytes the stream holds
    size_t                     m_sharedBytes = 0;
    bool                       m_isSharing   = false;
};