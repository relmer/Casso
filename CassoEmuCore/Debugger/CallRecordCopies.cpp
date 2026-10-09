#include "Pch.h"

#include "Debugger/CallRecordCopies.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Pack
//
//  A first byte of flags, the cycle the record dates from, the frames, the
//  breaks, and the last return when there is one. Equal records pack to
//  equal bytes.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Pack (
    const CallRecord   & record,
    std::vector<Byte>  & outBytes)
{
    Byte  flags = 0;



    outBytes.clear();

    if (record.isActive)
    {
        flags |= kIsActive;
    }

    if (record.lastReturn.has_value())
    {
        flags |= kHasLastReturn;
    }

    outBytes.push_back (flags);
    WriteNumber (outBytes, record.startCycle);

    WriteNumber (outBytes, record.frames.size());

    for (const CallStackFrame & frame : record.frames)
    {
        WriteFrame (outBytes, frame);
    }

    WriteNumber (outBytes, record.breaks.size());

    for (const CallStackRecorder::Break & each : record.breaks)
    {
        outBytes.push_back ((Byte) each.info.kind);
        WriteWord (outBytes, each.info.pc);
        outBytes.push_back (each.info.opcode);
        WriteNumber (outBytes, each.depth);
    }

    if (record.lastReturn.has_value())
    {
        WriteFrame (outBytes, *record.lastReturn);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Unpack
//
//  What Pack wrote, every byte of it; anything short, left over or out of
//  range is not a packed record.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::Unpack (
    std::span<const Byte>   bytes,
    CallRecord            & outRecord)
{
    HRESULT  hr      = S_OK;
    Reader   reader  = { bytes, 0 };
    Byte     flags   = 0;
    size_t   count   = 0;
    size_t   i       = 0;
    bool     isWhole = false;



    outRecord = CallRecord();

    hr = ReadByte (reader, flags);
    CHR (hr);

    outRecord.isActive = (flags & kIsActive) != 0;

    hr = ReadNumber (reader, outRecord.startCycle);
    CHR (hr);

    hr = ReadCount (reader, count);
    CHR (hr);

    outRecord.frames.resize (count);

    for (i = 0; i < count; i++)
    {
        hr = ReadFrame (reader, outRecord.frames[i]);
        CHR (hr);
    }

    hr = ReadCount (reader, count);
    CHR (hr);

    outRecord.breaks.resize (count);

    for (i = 0; i < count; i++)
    {
        hr = ReadBreak (reader, outRecord.breaks[i]);
        CHR (hr);
    }

    if ((flags & kHasLastReturn) != 0)
    {
        outRecord.lastReturn.emplace();

        hr = ReadFrame (reader, *outRecord.lastReturn);
        CHR (hr);
    }

    isWhole = reader.at == reader.bytes.size();
    CBREx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::SetBudget
//
//  A smaller budget lets the oldest copies go now.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::SetBudget (size_t budgetBytes)
{
    m_budget = budgetBytes;

    Trim();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Clear
//
//  Every copy goes, and the memory with them.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Clear()
{
    m_entries = std::vector<Entry>();
    m_arena   = std::vector<Byte>();
    m_first   = 0;
    m_garbage = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Set
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Set (
    uint64_t                position,
    std::span<const Byte>   packed)
{
    bool  isAdded = Insert (position, packed, true);



    IGNORE_RETURN_VALUE (isAdded, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::TryAdd
//
////////////////////////////////////////////////////////////////////////////////

bool CallRecordCopies::TryAdd (
    uint64_t                position,
    std::span<const Byte>   packed)
{
    return Insert (position, packed, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Drop
//
//  The copy at position, when one is kept there. The oldest goes without
//  moving the rest of the index.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Drop (uint64_t position)
{
    size_t  index = FindAtOrAfter (position);



    if (index == m_entries.size() || m_entries[index].position != position)
    {
        return;
    }

    Release (m_entries[index]);

    if (index == m_first)
    {
        m_first++;
    }
    else
    {
        m_entries.erase (m_entries.begin() + (ptrdiff_t) index);
    }

    Trim();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::TryFindAtOrBefore
//
////////////////////////////////////////////////////////////////////////////////

bool CallRecordCopies::TryFindAtOrBefore (
    uint64_t    position,
    uint64_t  & outPosition) const
{
    size_t  index   = FindAtOrAfter (position);
    bool    isFound = false;



    if (index < m_entries.size() && m_entries[index].position == position)
    {
        isFound = true;
    }
    else if (index > m_first)
    {
        index--;
        isFound = true;
    }

    if (isFound)
    {
        outPosition = m_entries[index].position;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::GetPacked
//
//  A view into the arena, good until the copies next change.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> CallRecordCopies::GetPacked (uint64_t position) const
{
    size_t                 index = FindAtOrAfter (position);
    std::span<const Byte>  packed;



    if (index < m_entries.size() && m_entries[index].position == position)
    {
        packed = std::span<const Byte> (m_arena.data() + m_entries[index].offset + kHeaderBytes,
                                        ReadField (m_entries[index].offset + kFieldBytes));
    }

    return packed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::GetByteCount
//
////////////////////////////////////////////////////////////////////////////////

size_t CallRecordCopies::GetByteCount() const
{
    return m_entries.capacity() * sizeof (Entry) + m_arena.capacity();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Insert
//
//  In position order, its bytes shared with the copy just before it when
//  they match; then the budget is kept.
//
////////////////////////////////////////////////////////////////////////////////

bool CallRecordCopies::Insert (
    uint64_t                position,
    std::span<const Byte>   packed,
    bool                    isReplacing)
{
    size_t  index  = FindAtOrAfter (position);
    size_t  offset = 0;



    if (index < m_entries.size() && m_entries[index].position == position)
    {
        if (!isReplacing)
        {
            return false;
        }

        Release (m_entries[index]);
        m_entries.erase (m_entries.begin() + (ptrdiff_t) index);
    }

    if (index > m_first && HasBytes (m_entries[index - 1].offset, packed))
    {
        offset = m_entries[index - 1].offset;
        WriteField (offset, ReadField (offset) + 1);
    }
    else
    {
        offset = Append (packed);
    }

    m_entries.insert (m_entries.begin() + (ptrdiff_t) index, Entry { position, offset });

    Trim();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Append
//
//  A new block at the arena's end, used by one copy. An arena with no room
//  left is laid out again first when it holds bytes no copy uses; it grows
//  by doubling, but not past the budget unless the bytes need it.
//
////////////////////////////////////////////////////////////////////////////////

size_t CallRecordCopies::Append (std::span<const Byte> packed)
{
    size_t  offset = m_arena.size();
    size_t  needed = offset + kHeaderBytes + packed.size();



    if (needed > m_arena.capacity() && m_garbage > 0)
    {
        Compact();

        offset = m_arena.size();
        needed = offset + kHeaderBytes + packed.size();
    }

    if (needed > m_arena.capacity())
    {
        m_arena.reserve ((std::max) (needed, (std::min) (m_arena.capacity() * 2, m_budget)));
    }

    m_arena.resize (offset + kHeaderBytes);
    m_arena.insert (m_arena.end(), packed.begin(), packed.end());

    WriteField (offset,               1);
    WriteField (offset + kFieldBytes, (uint32_t) packed.size());

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Release
//
//  One copy fewer uses the entry's block; a block none uses is garbage.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Release (const Entry & entry)
{
    uint32_t  users = ReadField (entry.offset) - 1;



    WriteField (entry.offset, users);

    if (users == 0)
    {
        m_garbage += kHeaderBytes + ReadField (entry.offset + kFieldBytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Trim
//
//  The oldest copies go while the bytes in use are over the budget, and the
//  index sheds the copies dropped from its front once they are half of it.
//  The arena is laid out again once the bytes no copy uses are half of it,
//  and gives back what it holds past the budget once it uses less than half
//  of that.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Trim()
{
    bool  isOversized = false;



    while (m_first < m_entries.size() && GetUsedByteCount() > m_budget)
    {
        Release (m_entries[m_first]);
        m_first++;
    }

    if (m_first * 2 > m_entries.size())
    {
        m_entries.erase (m_entries.begin(), m_entries.begin() + (ptrdiff_t) m_first);
        m_first = 0;
    }

    if (m_garbage * 2 > m_arena.size())
    {
        Compact();
    }

    isOversized = m_arena.capacity() > m_budget && m_arena.size() * 2 < m_arena.capacity();

    if (isOversized)
    {
        m_arena.shrink_to_fit();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::Compact
//
//  In place: every block still in use moves toward the arena's start, in
//  the order the blocks lie in it, over the bytes no copy uses, and a block
//  shared by several copies moves once. The index keeps its order and the
//  arena its allocation.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::Compact()
{
    std::vector<size_t>                 inUse;
    std::unordered_map<size_t, size_t>  moved;
    size_t                              to    = 0;
    size_t                              size  = 0;
    size_t                              index = 0;



    for (index = m_first; index < m_entries.size(); index++)
    {
        inUse.push_back (m_entries[index].offset);
    }

    std::ranges::sort (inUse);
    inUse.erase (std::unique (inUse.begin(), inUse.end()), inUse.end());

    for (size_t from : inUse)
    {
        size = kHeaderBytes + ReadField (from + kFieldBytes);

        if (from != to)
        {
            std::copy (m_arena.begin() + (ptrdiff_t) from, m_arena.begin() + (ptrdiff_t) (from + size), m_arena.begin() + (ptrdiff_t) to);
        }

        moved.emplace (from, to);
        to += size;
    }

    for (index = m_first; index < m_entries.size(); index++)
    {
        m_entries[index].offset = moved[m_entries[index].offset];
    }

    m_arena.resize (to);
    m_garbage = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::FindAtOrAfter
//
//  The index of the first copy at or after position; the index's end when
//  there is none.
//
////////////////////////////////////////////////////////////////////////////////

size_t CallRecordCopies::FindAtOrAfter (uint64_t position) const
{
    auto  found = std::lower_bound (m_entries.begin() + (ptrdiff_t) m_first, m_entries.end(), position,
                                    [] (const Entry & entry, uint64_t value) { return entry.position < value; });



    return (size_t) (found - m_entries.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::HasBytes
//
////////////////////////////////////////////////////////////////////////////////

bool CallRecordCopies::HasBytes (
    size_t                  offset,
    std::span<const Byte>   packed) const
{
    size_t  size = ReadField (offset + kFieldBytes);



    return size == packed.size() && std::equal (packed.begin(), packed.end(), m_arena.begin() + (ptrdiff_t) (offset + kHeaderBytes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadField
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CallRecordCopies::ReadField (size_t at) const
{
    uint32_t  value = 0;
    size_t    i     = 0;



    for (i = 0; i < kFieldBytes; i++)
    {
        value |= (uint32_t) m_arena[at + i] << (i * kByteBits);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::WriteField
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::WriteField (
    size_t    at,
    uint32_t  value)
{
    size_t  i = 0;



    for (i = 0; i < kFieldBytes; i++)
    {
        m_arena[at + i] = (Byte) (value >> (i * kByteBits));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::GetUsedByteCount
//
//  The bytes the copies kept use: their index entries and the blocks in use.
//
////////////////////////////////////////////////////////////////////////////////

size_t CallRecordCopies::GetUsedByteCount() const
{
    return (m_entries.size() - m_first) * sizeof (Entry) + m_arena.size() - m_garbage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::WriteWord (
    std::vector<Byte>  & out,
    Word                 value)
{
    out.push_back ((Byte) value);
    out.push_back ((Byte) (value >> kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::WriteNumber
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::WriteNumber (
    std::vector<Byte>  & out,
    uint64_t             value)
{
    while (value > kNumberMask)
    {
        out.push_back ((Byte) ((value & kNumberMask) | kMoreBit));
        value >>= kNumberBits;
    }

    out.push_back ((Byte) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::WriteText
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::WriteText (
    std::vector<Byte>   & out,
    const std::string   & text)
{
    WriteNumber (out, text.size());
    out.insert (out.end(), text.begin(), text.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::WriteFrame
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordCopies::WriteFrame (
    std::vector<Byte>      & out,
    const CallStackFrame   & frame)
{
    Byte  flags = 0;



    if (frame.isVerified)
    {
        flags |= kIsVerified;
    }

    if (frame.isRewritten)
    {
        flags |= kIsRewritten;
    }

    if (frame.hasRisen)
    {
        flags |= kHasRisen;
    }

    WriteWord (out, frame.callSite);
    WriteWord (out, frame.target);

    out.push_back ((Byte) frame.kind);
    out.push_back ((Byte) frame.provenance);
    out.push_back (frame.stackLevel);

    WriteNumber (out, frame.cycle);

    out.push_back (flags);

    WriteText (out, frame.symbol);
    WriteText (out, frame.note);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadByte
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadByte (
    Reader  & reader,
    Byte    & outValue)
{
    HRESULT  hr     = S_OK;
    bool     isLeft = reader.at < reader.bytes.size();



    CBREx (isLeft, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outValue = reader.bytes[reader.at++];

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadWord (
    Reader  & reader,
    Word    & outValue)
{
    HRESULT  hr   = S_OK;
    Byte     low  = 0;
    Byte     high = 0;



    hr = ReadByte (reader, low);
    CHR (hr);

    hr = ReadByte (reader, high);
    CHR (hr);

    outValue = (Word) (low | (high << kByteBits));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadNumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadNumber (
    Reader    & reader,
    uint64_t  & outValue)
{
    HRESULT  hr        = S_OK;
    Byte     each      = kMoreBit;
    int      shift     = 0;
    bool     isInRange = true;



    outValue = 0;

    while ((each & kMoreBit) != 0)
    {
        isInRange = shift < kNumberLimit;
        CBREx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = ReadByte (reader, each);
        CHR (hr);

        outValue |= (uint64_t) (each & kNumberMask) << shift;
        shift    += kNumberBits;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadCount
//
//  A count of things each at least a byte long, so never more than the
//  bytes left.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadCount (
    Reader  & reader,
    size_t  & outCount)
{
    HRESULT   hr     = S_OK;
    uint64_t  count  = 0;
    bool      isRoom = false;



    hr = ReadNumber (reader, count);
    CHR (hr);

    isRoom = count <= reader.bytes.size() - reader.at;
    CBREx (isRoom, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outCount = (size_t) count;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadText
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadText (
    Reader        & reader,
    std::string   & outText)
{
    HRESULT  hr     = S_OK;
    size_t   length = 0;



    hr = ReadCount (reader, length);
    CHR (hr);

    outText.assign (reinterpret_cast<const char *> (reader.bytes.data() + reader.at), length);
    reader.at += length;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadFrame (
    Reader           & reader,
    CallStackFrame   & outFrame)
{
    HRESULT  hr            = S_OK;
    Byte     kind          = 0;
    Byte     provenance    = 0;
    Byte     flags         = 0;
    bool     isKnownKind   = false;
    bool     isKnownSource = false;



    hr = ReadWord (reader, outFrame.callSite);
    CHR (hr);

    hr = ReadWord (reader, outFrame.target);
    CHR (hr);

    hr = ReadByte (reader, kind);
    CHR (hr);

    hr = ReadByte (reader, provenance);
    CHR (hr);

    isKnownKind   = kind <= (Byte) CallFrameKind::Nmi;
    isKnownSource = provenance <= (Byte) CallProvenance::Guessed;
    CBREx (isKnownKind,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isKnownSource, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFrame.kind       = (CallFrameKind) kind;
    outFrame.provenance = (CallProvenance) provenance;

    hr = ReadByte (reader, outFrame.stackLevel);
    CHR (hr);

    hr = ReadNumber (reader, outFrame.cycle);
    CHR (hr);

    hr = ReadByte (reader, flags);
    CHR (hr);

    outFrame.isVerified  = (flags & kIsVerified) != 0;
    outFrame.isRewritten = (flags & kIsRewritten) != 0;
    outFrame.hasRisen    = (flags & kHasRisen) != 0;

    hr = ReadText (reader, outFrame.symbol);
    CHR (hr);

    hr = ReadText (reader, outFrame.note);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies::ReadBreak
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallRecordCopies::ReadBreak (
    Reader                     & reader,
    CallStackRecorder::Break   & outBreak)
{
    HRESULT   hr          = S_OK;
    Byte      kind        = 0;
    uint64_t  depth       = 0;
    bool      isKnownKind = false;



    hr = ReadByte (reader, kind);
    CHR (hr);

    isKnownKind = kind <= (Byte) CallBreakKind::TrackingRestarted;
    CBREx (isKnownKind, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outBreak.info.kind = (CallBreakKind) kind;

    hr = ReadWord (reader, outBreak.info.pc);
    CHR (hr);

    hr = ReadByte (reader, outBreak.info.opcode);
    CHR (hr);

    hr = ReadNumber (reader, depth);
    CHR (hr);

    outBreak.depth = (size_t) depth;

Error:
    return hr;
}





