#include "Pch.h"

#include "Debugger/Handlers/MonitorHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::Examine:              Examine       (session, command, reply);        return true;
    case DebugVerb::Deposit:              Deposit       (session, command, reply);        return true;
    case DebugVerb::List:                 List          (session, command, reply);        return true;
    case DebugVerb::Verify:               Verify        (session, command, reply);        return true;
    case DebugVerb::Arithmetic:           Arithmetic    (command, reply);                 return true;
    case DebugVerb::SetInverse:           SetTextMode   (session, true,  reply);          return true;
    case DebugVerb::SetNormal:            SetTextMode   (session, false, reply);          return true;
    case DebugVerb::SetInputSlot:         SetHook       (session, command, true,  reply); return true;
    case DebugVerb::SetOutputSlot:        SetHook       (session, command, false, reply); return true;
    case DebugVerb::BasicColdStart:       RunAt         (session, kBasicCold,  reply);    return true;
    case DebugVerb::BasicWarmStart:       RunAt         (session, kBasicWarm,  reply);    return true;
    case DebugVerb::UserVector:           RunAt         (session, kUserVector, reply);    return true;
    case DebugVerb::ShowRegistersForEdit: ShowRegisters (session, reply);                 return true;
    case DebugVerb::EditRegisters:        EditRegisters (session, command, reply);        return true;
    case DebugVerb::ReadFile:             ReadFile      (session, command, reply);        return true;
    case DebugVerb::WriteFile:            WriteFile     (session, command, reply);        return true;
    default:                                                                              return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::MakeRows
//
//  ROWS BREAK ON EIGHT-BYTE BOUNDARIES, NOT ON THE RANGE.
//
//  `303.30F` prints five bytes against $0303 and then eight against $0308,
//  because the Monitor labels a row with the address that starts it. The
//  AppleWin dump starts its first row wherever the reader asked, so the two
//  cannot share a row builder.
//
////////////////////////////////////////////////////////////////////////////////

MemoryData MonitorHandlers::MakeRows (IDebugTarget & target, Word first, Word last)
{
    MemoryData  data;
    uint32_t    address = first;



    while (address <= last)
    {
        MemoryRow  row;
        uint32_t   rowEnd = (address | (kBytesPerRow - 1));



        row.address = (Word) address;
        row.region  = target.GetRegion ((Word) address);

        for (; address <= rowEnd && address <= last; ++address)
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
//  MonitorHandlers::Examine
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::Examine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    reply.data = MakeRows (session.GetTarget(), command.a1, GetLast (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::Deposit
//
//  The reply shows what was written, as the Monitor does not echo it.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::Deposit (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  last = 0;



    if (command.values.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "A deposit takes one or more bytes.");
        return;
    }

    last = (Word) (command.a1 + command.values.size() - 1);

    if (TryPokeRange (session, command.a1, command.values, reply))
    {
        reply.data = MakeRows (session.GetTarget(), command.a1, last);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::List
//
//  Twenty instructions, which is what the Monitor's L shows.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::List (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    DisassemblyData      data;
    std::optional<Word>  last;



    if (command.hasA2)
    {
        last = command.a2;
    }

    DataDirectiveHandlers::Disassemble (session, command.a1, last, kListLines, data);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::Verify
//
//  Each byte of the range against the byte at the same offset from the
//  destination. A range that matches produces no differences, and the
//  formatter prints nothing for it.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::Verify (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget & target = session.GetTarget();
    Word           last   = GetLast (command);
    CompareData    data;



    for (uint32_t address = command.a1; address <= last; ++address)
    {
        Word  other      = (Word) (command.a3 + (address - command.a1));
        Byte  value      = Peek (target, (Word) address);
        Byte  otherValue = Peek (target, other);

        ++data.compared;

        if (value != otherValue)
        {
            data.differences.push_back (CompareDifference { (Word) address, value, other, otherValue });
        }
    }

    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::Arithmetic
//
//  Eight-bit, as the Monitor's is: FF+FF is FE, not 01FE.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::Arithmetic (const DebugCommand & command, Reply & reply)
{
    Word  result = (command.text == "-")
                 ? (Word) ((command.a1 - command.a2) & kLowByte)
                 : (Word) ((command.a1 + command.a2) & kLowByte);



    reply.data = CalcData { result };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::SetTextMode
//
//  INVFLG is the mask the ROM's output routine ands each character with, so
//  $3F makes text inverse and $FF leaves it alone.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::SetTextMode (DebugSession & session, bool isInverse, Reply & reply)
{
    Byte  value = isInverse ? kInverseValue : kNormalValue;



    session.GetTarget().TryPoke (kInverseFlag, value);
    reply.data = MessageData { { isInverse ? "Inverse" : "Normal" } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::SetHook
//
//  `n^K` points the input hook at slot n's $Cn00 and `n^P` the output hook;
//  slot 0 restores the ROM's own keyboard and screen routines.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::SetHook (DebugSession & session, const DebugCommand & command, bool isInput, Reply & reply)
{
    Word  hook    = isInput ? kInputHook : kOutputHook;
    Word  restore = isInput ? kKeyIn : kCharacterOut;
    int   slot    = (int) command.count;
    Word  target  = 0;



    if (slot < 0 || slot > kLastSlot)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("There is no slot {}. The slots are 0 to {}.", slot, kLastSlot));
        return;
    }

    target = (slot == 0) ? restore : (Word) (kSlotBase + kSlotStride * slot);

    PokeWord (session.GetTarget(), hook, target);

    reply.data = MessageData { { std::format ("{} hook: ${:04X}", isInput ? "Input" : "Output", target) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::ShowRegisters
//
//  The arming of the next `:` happened when the line was parsed, so this
//  only shows them.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::ShowRegisters (DebugSession & session, Reply & reply)
{
    reply.data = RegistersData { session.GetTarget().GetRegisters() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::EditRegisters
//
//  A, X, Y, P and S in that order, as many as were given.
//
//  THE CPU IS THE TRUTH AND $45-$49 FOLLOW IT. The ROM's own G, S and T
//  reload the registers from those five bytes, so a program that changed
//  them would clobber what the reader set; Casso's never do, and the bytes
//  are written anyway so a guest that reads them sees what the reader typed.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::EditRegisters (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget      & target    = session.GetTarget();
    Cpu6502Registers    registers = target.GetRegisters();
    Byte              * fields[]  = { &registers.a, &registers.x, &registers.y, &registers.p, &registers.sp };
    size_t              count     = std::min (command.values.size(), std::size (fields));



    for (size_t i = 0; i < count; ++i)
    {
        *fields[i] = command.values[i];
        target.TryPoke ((Word) (kRegisterSave + i), command.values[i]);
    }

    target.SetRegisters (registers);
    reply.data = RegistersData { registers };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::ReadFile
//
//  Reads the smaller of the file and the range, and says so when they differ
//  (FR-020).
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::ReadFile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem      * files   = nullptr;
    Word               last    = GetLast (command);
    uint32_t           wanted  = (uint32_t) (last - command.a1 + 1);
    std::string        content;
    std::vector<Byte>  bytes;
    FileIoData         data;
    HRESULT            hr      = S_OK;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "R takes a file name here. Only the debugger window can prompt for one.");
        return;
    }

    if (!TryGetFiles (session, reply, files))
    {
        return;
    }

    hr = files->ReadAllText (session.ResolvePath (command.text), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not found", std::format ("{} could not be read.", command.text));
        return;
    }

    bytes.assign (content.begin(), content.begin() + std::min<size_t> (content.size(), wanted));

    if (!TryPokeRange (session, command.a1, bytes, reply))
    {
        return;
    }

    data.path        = command.text;
    data.requested   = wanted;
    data.transferred = (uint32_t) bytes.size();
    data.mismatch    = data.transferred != wanted;
    reply.data       = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::WriteFile
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::WriteFile (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem  * files = nullptr;
    Word           last  = GetLast (command);
    std::string    content;
    FileIoData     data;
    HRESULT        hr    = S_OK;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "W takes a file name here. Only the debugger window can prompt for one.");
        return;
    }

    if (!TryGetFiles (session, reply, files))
    {
        return;
    }

    for (uint32_t address = command.a1; address <= last; ++address)
    {
        content.push_back ((char) Peek (session.GetTarget(), (Word) address));
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text));
        return;
    }

    data.path        = command.text;
    data.requested   = (uint32_t) content.size();
    data.transferred = (uint32_t) content.size();
    reply.data       = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::RunAt
//
//  Through the session's own run, so a run started this way is a run like
//  any other: the budget applies, the breakpoints apply, and the stop is
//  announced.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::RunAt (DebugSession & session, Word address, Reply & reply)
{
    DebugCommand  go;



    go.verb       = DebugVerb::Go;
    go.sourceName = "G";
    go.mode       = CommandMode::Monitor;
    go.a3         = address;
    go.hasA3      = true;

    reply = session.Execute (go);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::TryGetFiles
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorHandlers::TryGetFiles (DebugSession & session, Reply & reply, IFileSystem *& files)
{
    files = session.GetFileSystem();

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session has no file system.");
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::TryPokeRange
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorHandlers::TryPokeRange (DebugSession & session, Word first, std::span<const Byte> bytes, Reply & reply)
{
    IDebugTarget & target = session.GetTarget();



    for (size_t i = 0; i < bytes.size(); ++i)
    {
        Word  address = (Word) (first + i);

        if (!target.TryPoke (address, bytes[i]))
        {
            reply.SetError (CommandStatus::Error, "memory not writable", std::format ("${:04X} cannot be written.", address));
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::GetLast
//
////////////////////////////////////////////////////////////////////////////////

Word MonitorHandlers::GetLast (const DebugCommand & command)
{
    return command.hasA2 ? command.a2 : command.a1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte MonitorHandlers::Peek (IDebugTarget & target, Word address)
{
    Byte  value = 0;



    target.TryPeek (address, value);
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers::PokeWord
//
//  Low byte first, as the 6502 stores an address.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorHandlers::PokeWord (IDebugTarget & target, Word address, Word value)
{
    target.TryPoke (address,             (Byte) (value & kLowByte));
    target.TryPoke ((Word) (address + 1), (Byte) (value >> 8));
}
