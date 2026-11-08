#include "Pch.h"

#include "Debugger/Handlers/MemoryHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/BinaryImageReader.h"
#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::DumpMemory:        Dump         (session, command, reply); return true;
    case DebugVerb::EnterBytes:
    case DebugVerb::EnterWords:        Enter        (session, command, reply); return true;
    case DebugVerb::PatchBytes:        Patch        (session, command, reply); return true;
    case DebugVerb::MoveMemory:        Move         (session, command, reply); return true;
    case DebugVerb::CompareMemory:     Compare      (session, command, reply); return true;
    case DebugVerb::FillMemory:        Fill         (session, command, reply); return true;
    case DebugVerb::SearchMemory:
    case DebugVerb::SearchHex:         Search       (session, command, reply); return true;
    case DebugVerb::ShowSearchResults: ShowResults  (session, reply);          return true;
    case DebugVerb::LoadBinary:        LoadBinary   (session, command, reply); return true;
    case DebugVerb::SaveBinary:        SaveBinary   (session, command, reply); return true;
    case DebugVerb::SaveText:          SaveText     (session, command, reply); return true;
    case DebugVerb::ReadIo:            ReadIo       (session, command, reply); return true;
    case DebugVerb::WriteIo:           WriteIo      (session, command, reply); return true;
    case DebugVerb::ShowSwitches:      ShowSwitches (session, reply);          return true;
    default:                                                                   return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::MakeRows
//
//  An unreadable byte, which is any in $C000-$C0FF, is an empty cell.
//
////////////////////////////////////////////////////////////////////////////////

MemoryData MemoryHandlers::MakeRows (IDebugTarget & target, Word first, Word last)
{
    MemoryData  data;
    uint32_t    address = first;



    while (address <= last)
    {
        MemoryRow  row;



        row.address = (Word) address;
        row.region  = target.GetRegion ((Word) address);

        for (int i = 0; i < kBytesPerRow && address <= last; ++i, ++address)
        {
            Byte  value = 0;



            row.bytes.push_back (target.TryPeek ((Word) address, value) ? std::optional<Byte> (value) : std::nullopt);
        }

        data.rows.push_back (row);
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::MakeWrittenRows
//
//  The rows of count bytes written from first. A write that ran past $FFFF
//  went on at $0000, so its rows are the part up to $FFFF and then the part
//  from $0000.
//
////////////////////////////////////////////////////////////////////////////////

MemoryData MemoryHandlers::MakeWrittenRows (IDebugTarget & target, Word first, size_t count)
{
    uint32_t    end     = (uint32_t) first + (uint32_t) count - 1;
    MemoryData  data    = MakeRows (target, first, (Word) std::min<uint32_t> (end, kAddressSpace));
    MemoryData  wrapped;



    if (end > kAddressSpace)
    {
        wrapped = MakeRows (target, 0, (Word) (end - kAddressSpace - 1));
        data.rows.insert (data.rows.end(), wrapped.rows.begin(), wrapped.rows.end());
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Dump
//
//  D range shows the range; D addr shows 64 bytes from it; D alone continues
//  from the last dump's end.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Dump (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  first = command.hasA1 ? command.a1 : m_nextDump;
    Word  last  = (Word) std::min<uint32_t> ((uint32_t) first + kDefaultDump - 1, kAddressSpace);



    if (command.hasA2 && !TryGetRange (command, first, last, reply))
    {
        return;
    }

    reply.data = MakeRows (session.GetTarget(), first, last);
    m_nextDump = (Word) (last + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Enter
//
//  ME, MEB and the addr:bytes form take bytes, with a value above $FF as two
//  bytes; MEW takes words. The parser has already laid the bytes out low
//  byte first. The reply shows the rows written.
//
//  Only the addr:bytes form can arrive with no values, since the parsers
//  refuse every named form without one; its name is the address the user
//  typed, so the message does not use it as a command name.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Enter (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "A deposit takes an address and one or more values, as 300: A9 00.");
        return;
    }

    if (TryPokeRange (session.GetTarget(), command.a1, command.values, reply))
    {
        reply.data = MakeWrittenRows (session.GetTarget(), command.a1, command.values.size());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Patch
//
//  A memory window's edit, and a command a person can type: RAM takes it as a
//  write and ROM into the image the CPU reads from. An I/O address stops it,
//  because writing one has effects only OUT should cause.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Patch (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget  & target = session.GetTarget();



    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PATCH takes an address and one or more values.");
        return;
    }

    for (size_t i = 0; i < command.values.size(); ++i)
    {
        Word  address = (Word) (command.a1 + i);



        if (!target.TryPatch (address, command.values[i]))
        {
            reply.SetError (CommandStatus::Error, "memory not writable",
                            target.GetRegion (address) == MemoryRegion::Io
                                ? std::format ("${:04X} is I/O. Use OUT to write it.", address)
                                : std::format ("${:04X} cannot be written. {} of {} bytes were written.", address, i, command.values.size()));
            return;
        }
    }

    reply.data = MakeWrittenRows (target, command.a1, command.values.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Move
//
//  M dest range: the source is read whole before anything is written, so an
//  overlapping move is as if through a buffer. A source that reaches I/O is
//  refused before anything is written, since reading it would change the
//  machine and a zero in its place would be a byte that was never there.
//
//  The Monitor's M without `dest<` has no destination at all, and is refused
//  rather than taken as $0000.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Move (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget      & target = session.GetTarget();
    Word                first  = command.a1;
    Word                last   = command.a1;
    std::vector<Byte>   bytes;



    if (!command.hasA3)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "A move needs a destination, as 300<800.8FFM.");
        return;
    }

    if (!TryGetRange (command, first, last, reply) || !TryReadRange (target, first, last, "moved", bytes, reply))
    {
        return;
    }

    if (TryPokeRange (target, command.a3, bytes, reply))
    {
        reply.data = MessageData { { std::format ("Moved {} from ${:04X} to ${:04X}.", FormatByteCount (bytes.size()), first, command.a3) } };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Compare
//
//  MC dest range: each byte of the range against the byte at the same offset
//  from dest. A pair with an I/O byte in it is not compared, and not counted
//  as compared, since that byte cannot be read without changing the machine.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Compare (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget  & target = session.GetTarget();
    Word            first  = command.a1;
    Word            last   = command.a1;
    CompareData     data;



    if (!TryGetRange (command, first, last, reply))
    {
        return;
    }

    for (uint32_t address = first; address <= last; ++address)
    {
        Word  other      = (Word) (command.a3 + (address - first));
        Byte  value      = 0;
        Byte  otherValue = 0;



        if (!target.TryPeek ((Word) address, value) || !target.TryPeek (other, otherValue))
        {
            continue;
        }

        ++data.compared;

        if (value != otherValue)
        {
            data.differences.push_back ({ (Word) address, value, other, otherValue });
        }
    }

    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Fill
//
//  The values repeat across the range.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Fill (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word               first = command.a1;
    Word               last  = command.a1;
    std::vector<Byte>  bytes;



    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "F takes a range and one or more values.");
        return;
    }

    if (!TryGetRange (command, first, last, reply))
    {
        return;
    }

    for (uint32_t address = first; address <= last; ++address)
    {
        bytes.push_back (command.values[(address - first) % command.values.size()]);
    }

    if (TryPokeRange (session.GetTarget(), first, bytes, reply))
    {
        reply.data = MessageData { { std::format ("Filled {} at ${:04X}-${:04X}.", FormatByteCount (bytes.size()), first, last) } };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Search
//
//  Every position in the range where each pattern byte matches under its
//  mask. The results replace the session's, so @1 is the first hit. A byte
//  that cannot be read, which is any in $C000-$C0FF, matches nothing, not
//  even a wildcard: nothing is known to be there.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::Search (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget    & target = session.GetTarget();
    Word              first  = command.a1;
    Word              last   = command.a1;
    SearchHitsData    data;



    if (!TryGetRange (command, first, last, reply))
    {
        return;
    }

    for (uint32_t address = first; address + command.values.size() - 1 <= last; ++address)
    {
        bool  isMatch = true;



        for (size_t i = 0; i < command.values.size() && isMatch; ++i)
        {
            Byte  value = 0;



            isMatch = target.TryPeek ((Word) (address + i), value) &&
                      (value & command.mask[i]) == (command.values[i] & command.mask[i]);
        }

        if (isMatch)
        {
            data.addresses.push_back ((Word) address);
        }
    }

    session.SetSearchResults (data.addresses);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::ShowResults
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::ShowResults (DebugSession & session, Reply & reply)
{
    reply.data = SearchHitsData { session.GetSearchResults() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::LoadBinary
//
//  BLOAD file[,DOS|RAW|HEX|SREC|AS] [addr[,len]]. Intel HEX, S-records and
//  AppleSingle are known from their content and carry their own addresses;
//  raw bytes need one, and a DOS 3.3 binary is chosen by the word after the
//  file name because its header cannot be told from data. A length caps a
//  single-segment load.
//
//  The format word may sit inside the quotes or after them: `"a.bin",DOS` as
//  typed, and `"a.bin,DOS"` as GSSquared's load passes it on. The name is
//  reported without its quotes.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::LoadBinary (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem                  * files     = nullptr;
    std::string                    name      = Unquote (command.text);
    std::optional<BinaryFormat>    format;
    std::optional<Word>            address;
    BinaryFormat                   chosen    = BinaryFormat::Raw;
    size_t                         comma     = name.rfind (',');
    std::string                    content;
    std::string                    error;
    BinaryImage                    image;
    size_t                         requested = 0;
    size_t                         loaded    = 0;
    Word                           first     = command.a1;
    Word                           last      = command.a1;
    FileIoData                     data;
    HRESULT                        hr        = S_OK;



    if (comma != std::string::npos && BinaryImageReader::TryGetFormatName (name.substr (comma + 1), chosen))
    {
        format = chosen;
        name   = Unquote (name.substr (0, comma));
    }

    if (command.hasA2 && !TryGetRange (command, first, last, reply))
    {
        return;
    }

    if (command.hasA1)
    {
        address = command.a1;
    }

    if (!TryGetFiles (session, reply, files))
    {
        return;
    }

    hr = files->ReadAllText (session.ResolvePath (name), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not found", std::format ("{} could not be read.", name));
        return;
    }

    hr = BinaryImageReader::Read (std::span<const Byte> ((const Byte *) content.data(), content.size()), format, address, image, error);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not loadable", error);
        return;
    }

    for (const BinarySegment & segment : image.segments)
    {
        size_t  count = segment.bytes.size();



        if (command.hasA2 && image.segments.size() == 1)
        {
            count = std::min<size_t> (count, (size_t) (last - first) + 1);
        }

        count      = std::min<size_t> (count, (size_t) (kAddressSpace - segment.address) + 1);
        requested += segment.bytes.size();

        if (!TryPokeRange (session.GetTarget(), segment.address, std::span<const Byte> (segment.bytes.data(), count), reply))
        {
            return;
        }

        loaded += count;
    }

    data.path        = std::format ("{} ({})", name, BinaryImageReader::GetFormatName (image.format));
    data.requested   = (uint32_t) requested;
    data.transferred = (uint32_t) loaded;
    data.mismatch    = loaded != requested;
    reply.data       = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::SaveBinary
//
//  BSAVE file range writes the range's bytes as they read now. A range that
//  reaches I/O is refused and nothing is written, since those bytes cannot be
//  read without changing the machine.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::SaveBinary (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem        * files = nullptr;
    std::string          name  = Unquote (command.text);
    Word                 first = command.a1;
    Word                 last  = command.a1;
    std::vector<Byte>    bytes;
    std::string          content;
    FileIoData           data;
    HRESULT              hr    = S_OK;



    if (!command.hasA1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BSAVE takes a file name and the range to save.");
        return;
    }

    if (!TryGetRange (command, first, last, reply) || !TryGetFiles (session, reply, files) ||
        !TryReadRange (session.GetTarget(), first, last, "saved", bytes, reply))
    {
        return;
    }

    content.assign (bytes.begin(), bytes.end());
    hr = files->WriteAllText (session.ResolvePath (name), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    data.path        = name;
    data.requested   = (uint32_t) content.size();
    data.transferred = (uint32_t) content.size();
    reply.data       = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::SaveText
//
//  TSAVE file writes the 40-column text page as 24 lines of 40 characters,
//  read from main memory in the interleaved order the screen holds them,
//  with the high bit dropped and control characters as periods.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::SaveText (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr Byte  kLowBits    = 0x7F;
    static constexpr Byte  kFirstPrint = 0x20;
    static constexpr Byte  kDelete     = 0x7F;
    IFileSystem          * files       = nullptr;
    std::string            content;
    HRESULT                hr          = S_OK;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "TSAVE takes a file name.");
        return;
    }

    if (!TryGetFiles (session, reply, files))
    {
        return;
    }

    for (int row = 0; row < kTextRows; ++row)
    {
        Word  base = (Word) (kTextPage + (row % kTextRowGroups) * kTextGroupStride + (row / kTextRowGroups) * kTextRowOffset);



        for (int column = 0; column < kTextColumns; ++column)
        {
            Byte  ch = (Byte) (Peek (session.GetTarget(), (Word) (base + column)) & kLowBits);



            content.push_back ((ch < kFirstPrint || ch == kDelete) ? '.' : (char) ch);
        }

        content.push_back ('\n');
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", Unquote (command.text)));
        return;
    }

    reply.data = MessageData { { std::format ("Saved the text screen to {}.", Unquote (command.text)) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::ReadIo
//
//  IN addr is a real bus read, with its side effects.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::ReadIo (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    MemoryData  data;
    MemoryRow   row;



    if (!command.hasA1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "IN takes an address.");
        return;
    }

    row.address = command.a1;
    row.region  = session.GetTarget().GetRegion (command.a1);
    row.bytes.push_back (session.GetTarget().ReadIo (command.a1));
    data.rows.push_back (row);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::WriteIo
//
//  OUT addr value...: each value goes to the next address, through the bus.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::WriteIo (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (!command.hasA1 || command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "OUT takes an address and one or more values.");
        return;
    }

    for (size_t i = 0; i < command.values.size(); ++i)
    {
        session.GetTarget().WriteIo ((Word) (command.a1 + i), command.values[i]);
    }

    reply.data = MessageData { { std::format ("Wrote {} at ${:04X}.", FormatByteCount (command.values.size()), command.a1) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::ShowSwitches
//
////////////////////////////////////////////////////////////////////////////////

void MemoryHandlers::ShowSwitches (DebugSession & session, Reply & reply)
{
    SoftSwitchData  data;



    session.GetTarget().GetSoftSwitches (data.switches);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::TryPokeRange
//
//  Nothing is written past an address that cannot be written: ROM or I/O.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryHandlers::TryPokeRange (IDebugTarget & target, Word first, std::span<const Byte> bytes, Reply & reply)
{
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        Word  address = (Word) (first + i);



        if (!target.TryPoke (address, bytes[i]))
        {
            reply.SetError (CommandStatus::Error, "memory not writable",
                            std::format ("${:04X} cannot be written. {} of {} bytes were written.", address, i, bytes.size()));
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::TryGetFiles
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryHandlers::TryGetFiles (DebugSession & session, Reply & reply, IFileSystem *& files)
{
    files = session.GetFileSystem();

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::TryGetRange
//
//  The command's range: from a1 to a2, or a1 alone. A range whose end is
//  below its start is refused rather than cut to one byte. Every range the
//  parsers produce runs upward, so this is a range typed backward, or a
//  length that carried its end past $FFFF and round to the bottom.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryHandlers::TryGetRange (const DebugCommand & command, Word & first, Word & last, Reply & reply)
{
    first = command.a1;
    last  = command.hasA2 ? command.a2 : command.a1;

    if (last < first)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("${:04X}-${:04X} ends before it starts. A range runs up from its first address to its last, and stops at $FFFF.",
                                     first, last));
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::TryReadRange
//
//  Every byte from first to last, or false at the first that cannot be read
//  without changing the machine, which is any in $C000-$C0FF. verb says what
//  the command does with them, for the message.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryHandlers::TryReadRange (
    IDebugTarget       & target,
    Word                 first,
    Word                 last,
    const char         * verb,
    std::vector<Byte>  & bytes,
    Reply              & reply)
{
    for (uint32_t address = first; address <= last; ++address)
    {
        Byte  value = 0;



        if (!target.TryPeek ((Word) address, value))
        {
            reply.SetError (CommandStatus::Error, "memory not readable",
                            std::format ("${:04X} is I/O, and reading it would change the machine. Nothing was {}.", (Word) address, verb));
            return false;
        }

        bytes.push_back (value);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::FormatByteCount
//
////////////////////////////////////////////////////////////////////////////////

std::string MemoryHandlers::FormatByteCount (size_t count)
{
    return std::format ("{} {}", count, count == 1 ? "byte" : "bytes");
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Unquote
//
//  A file name without the quotes it was typed in, as the replies show it.
//
////////////////////////////////////////////////////////////////////////////////

std::string MemoryHandlers::Unquote (const std::string & text)
{
    bool  isQuoted = text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front();



    return isQuoted ? text.substr (1, text.size() - 2) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryHandlers::Peek
//
//  An unreadable byte reads as zero.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryHandlers::Peek (IDebugTarget & target, Word address)
{
    Byte  value = 0;



    target.TryPeek (address, value);
    return value;
}
