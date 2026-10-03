#include "Pch.h"

#include "StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Reuse
//
//  Starts the stream over in buffer, keeping its length, so a writer that
//  saves the machine many times allocates once and writes over the last
//  save's bytes instead of zero-filling them again. The sharing setting
//  stays.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Reuse (std::vector<Byte> && buffer)
{
    m_bytes = std::move (buffer);
    m_segments.clear();
    m_openSections.clear();

    m_size        = 0;
    m_sharedBytes = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reuse (with a segment list)
//
//  As Reuse, and the segment list of an earlier save as well, emptied, so a
//  sharing save keeps that list's capacity instead of reserving a new one.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Reuse (
    std::vector<Byte>          && buffer,
    std::vector<StateSegment>  && segments)
{
    Reuse (std::move (buffer));

    m_segments = std::move (segments);
    m_segments.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBytes
//
//  The stream's own bytes. The buffer is cut to the byte count first; cutting
//  frees nothing, so a later write grows it back without reallocating.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<Byte> & StateWriter::GetBytes() const
{
    m_bytes.resize (m_size);

    return m_bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeBytes
//
//  Hands the stream's own bytes over and starts an empty stream.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> StateWriter::TakeBytes()
{
    std::vector<Byte>  bytes;



    m_bytes.resize (m_size);

    bytes  = std::move (m_bytes);
    m_size = 0;

    m_bytes.clear();

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Grow
//
//  Lengthens the buffer to hold count more bytes than the stream does. Its
//  capacity at least doubles, so a fresh buffer reallocates a handful of
//  times over a save, and only the bytes about to be written are filled.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Grow (size_t count)
{
    size_t  needed = m_size + count;



    if (needed > m_bytes.capacity())
    {
        m_bytes.reserve (std::max (needed, m_bytes.capacity() * 2));
    }

    m_bytes.resize (needed);
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

    section.sizeOffset   = m_size - sizeof (uint32_t);
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
//  WriteBytes
//
//  A raw run of bytes, such as a RAM bank. The count is not written; the
//  reader must know it, and a section size check catches a disagreement.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteBytes (const Byte * data, size_t count)
{
    if (count != 0)
    {
        memcpy (GetRoom (count), data, count);
    }
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

    // Room for a disk's tracks at once, so a save does not regrow the list.
    if (m_segments.capacity() == 0)
    {
        m_segments.reserve (kSegmentReserve);
    }

    segment.offset = m_size;
    segment.bytes  = bytes;

    m_sharedBytes += bytes->size();
    m_segments.push_back (std::move (segment));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlattenInto
//
//  The whole blob this writer holds so far, segments spliced in, written
//  over outBytes, which keeps its capacity.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::FlattenInto (std::vector<Byte> & outBytes) const
{
    Flatten (GetBytes(), m_segments, outBytes);
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





