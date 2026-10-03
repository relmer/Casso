#include "Pch.h"

#include "StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Reuse
//
//  Starts the stream over in buffer, keeping its capacity, so a writer that
//  saves the machine many times allocates once. The sharing setting stays.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Reuse (std::vector<Byte> && buffer)
{
    m_bytes = std::move (buffer);
    m_bytes.clear();
    m_segments.clear();
    m_openSections.clear();

    m_sharedBytes = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginSection
//
//  Writes the section header with a placeholder size and opens the section.
//  Every BeginSection is matched by an EndSection.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::BeginSection (uint32_t tag, uint16_t version)
{
    OpenSection  section;



    WriteUInt32 (tag);
    WriteWord   (version);
    WriteUInt32 (0);

    section.sizeOffset   = m_bytes.size() - sizeof (uint32_t);
    section.payloadStart = GetStreamOffset();

    m_openSections.push_back (section);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndSection
//
//  Closes the innermost open section and patches its payload size into the
//  header. Fails when no section is open, or when the payload would not fit
//  the 32-bit size field.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StateWriter::EndSection()
{
    HRESULT      hr          = S_OK;
    bool         isOpen      = !m_openSections.empty();
    OpenSection  section;
    size_t       payloadSize = 0;
    size_t       i           = 0;



    CBRAEx (isOpen, E_UNEXPECTED);

    section = m_openSections.back();
    m_openSections.pop_back();

    payloadSize = GetStreamOffset() - section.payloadStart;
    CBRAEx (payloadSize <= UINT32_MAX, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

    for (i = 0; i < sizeof (uint32_t); i++)
    {
        m_bytes[section.sizeOffset + i] = static_cast<Byte> (payloadSize >> (i * CHAR_BIT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteByte (Byte value)
{
    m_bytes.push_back (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBool
//
//  One byte, 0 or 1.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteBool (bool value)
{
    m_bytes.push_back (value ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteWord (Word value)
{
    WriteLittleEndian (value, sizeof (Word));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteUInt32
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteUInt32 (uint32_t value)
{
    WriteLittleEndian (value, sizeof (uint32_t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteUInt64
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteUInt64 (uint64_t value)
{
    WriteLittleEndian (value, sizeof (uint64_t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBytes
//
//  A raw run of bytes, such as a RAM bank. The count is not written; the
//  reader must know it, and a section size check catches a disagreement.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteBytes (const Byte * data, size_t count)
{
    m_bytes.insert (m_bytes.end(), data, data + count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteShared
//
//  A raw run of bytes, as WriteBytes, that a sharing writer keeps by
//  reference. The buffer must not change while any save holding it lives.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteShared (const std::shared_ptr<const std::vector<Byte>> & bytes)
{
    StateSegment  segment;



    if (!m_isSharing)
    {
        WriteBytes (bytes->data(), bytes->size());
        return;
    }

    segment.offset = m_bytes.size();
    segment.bytes  = bytes;

    m_sharedBytes += bytes->size();
    m_segments.push_back (std::move (segment));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flatten
//
//  The blob a sharing writer stands for: its own bytes with each segment
//  spliced in at its offset, in order.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Flatten (
    const std::vector<Byte>          & own,
    const std::vector<StateSegment>  & segments,
    std::vector<Byte>                & outBytes)
{
    size_t  total = own.size();
    size_t  taken = 0;



    for (const StateSegment & segment : segments)
    {
        total += segment.bytes->size();
    }

    outBytes.clear();
    outBytes.reserve (total);

    for (const StateSegment & segment : segments)
    {
        outBytes.insert (outBytes.end(), own.begin() + static_cast<ptrdiff_t> (taken), own.begin() + static_cast<ptrdiff_t> (segment.offset));
        outBytes.insert (outBytes.end(), segment.bytes->begin(), segment.bytes->end());

        taken = segment.offset;
    }

    outBytes.insert (outBytes.end(), own.begin() + static_cast<ptrdiff_t> (taken), own.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteLittleEndian (uint64_t value, size_t byteCount)
{
    size_t  i = 0;



    for (i = 0; i < byteCount; i++)
    {
        m_bytes.push_back (static_cast<Byte> (value >> (i * CHAR_BIT)));
    }
}
