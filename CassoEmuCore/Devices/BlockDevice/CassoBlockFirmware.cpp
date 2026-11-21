#include "Pch.h"

#include "Devices/BlockDevice/CassoBlockFirmware.h"
#include "Assembler.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"

#include "CassoBlock.s.h"




static constexpr Word     s_kPageBytes    = 0x100;
static constexpr Word     s_kSlotPages    = 0xC000;
static constexpr Byte     s_kDriveBit     = 0x80;
static constexpr Byte     s_kStatus       = 0;
static constexpr Byte     s_kRead         = 1;
static constexpr Byte     s_kWrite        = 2;
static constexpr Byte     s_kFormat       = 3;
static constexpr Byte     s_kControl      = 4;
static constexpr Byte     s_kInit         = 5;
static constexpr Byte     s_kLastCommand  = 9;
static constexpr Byte     s_kExtended     = 0x40;
static constexpr size_t   s_kBlockBytes   = BlockDeviceMedia::kBlockBytes;
static constexpr uint32_t s_kMaxBlocks    = 0xFFFF;
static constexpr Byte     s_kNoInterrupt  = 0x40;
static constexpr size_t   s_kNameBytes    = 16;
static constexpr Byte     s_kBlockDevice  = 0x80;
static constexpr Byte     s_kWriteOk      = 0x40;
static constexpr Byte     s_kReadOk       = 0x20;
static constexpr Byte     s_kOnline       = 0x10;
static constexpr Byte     s_kFormatOk     = 0x08;
static constexpr Byte     s_kProtected    = 0x04;
static constexpr Byte     s_kInfoCode     = 3;

//  The parameter count of each SmartPort command, status to write.
static constexpr Byte     s_kParamCounts[] = { 3, 3, 3, 1, 3, 1, 1, 1, 4, 4 };





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblePage
//
//  The page for `slot`, assembled from CassoBlock.s with SLOT set ahead of
//  it. It is exactly one page at $Cn00.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoBlockFirmware::AssemblePage (int slot, std::vector<Byte> & outPage, std::string & outErrors)
{
    static const Cpu  s_cpu;
    HRESULT           hr     = S_OK;
    std::string       source (reinterpret_cast<const char *> (s_kCassoBlockSource), sizeof (s_kCassoBlockSource) - 1);
    std::string       text   = std::format ("SLOT = {}\n", slot) + source;
    Assembler         assembler (InstructionSetProvider (s_cpu.GetInstructionSet()));
    AssemblyResult    result;
    bool              placed = false;



    outPage.clear();
    outErrors.clear();

    CBRAEx (slot >= 1 && slot <= 7, E_INVALIDARG);

    result = assembler.Assemble (text);

    for (const AssemblyError & error : result.errors)
    {
        outErrors += std::format ("line {}: {}\n", error.lineNumber - 1, error.message);
    }

    CBREx (result.success, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    placed = result.startAddress == s_kSlotPages + slot * s_kPageBytes && result.bytes.size() == s_kPageBytes;
    CBREx (placed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outPage = result.bytes;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Enable
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoBlockFirmware::Enable (UnitLookup getUnit, Device device)
{
    HRESULT      hr = S_OK;
    std::string  errors;



    hr = AssemblePage (m_slot, m_page, errors);
    CHR (hr);

    m_getUnit = std::move (getUnit);
    m_device  = std::move (device);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void CassoBlockFirmware::Reset()
{
    FlushUnit();

    m_command = 0;
    m_unit    = 0;
    m_buffer  = 0;
    m_block   = 0;
    m_return  = 0;
    m_resume  = 0;
    m_error   = 0;
    m_x       = 0;
    m_y       = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::Read (Word offset)
{
    Byte  value = 0;



    switch (offset)
    {
        case kRunPort:     value = m_error = Run();                break;
        case kSmartPort:   value = m_error = RunSmartPort();       break;
        case kCountLow:    value = m_x;                            break;
        case kCountHigh:   value = m_y;                            break;
        case kResumeLow:   value = (Byte) (m_resume & 0xFF);       break;
        case kResumeHigh:  value = (Byte) (m_resume >> 8);         break;
        case kLastError:   value = m_error;                        break;
        default:           value = m_bus.GetFloatingBusValue();    break;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void CassoBlockFirmware::Write (Word offset, Byte value)
{
    switch (offset)
    {
        case kCommandPort:  m_command = value;                                        break;
        case kUnitPort:     m_unit    = value;                                        break;
        case kBufferLow:    m_buffer  = (Word) ((m_buffer & 0xFF00) | value);         break;
        case kBufferHigh:   m_buffer  = (Word) ((m_buffer & 0x00FF) | (value << 8));  break;
        case kBlockLow:     m_block   = (Word) ((m_block  & 0xFF00) | value);         break;
        case kBlockHigh:    m_block   = (Word) ((m_block  & 0x00FF) | (value << 8));  break;
        case kReturnLow:    m_return  = (Word) ((m_return & 0xFF00) | value);         break;
        case kReturnHigh:   m_return  = (Word) ((m_return & 0x00FF) | (value << 8));  break;
        default:                                                                      break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  A ProDOS call. X and Y return the drive's block count, at most the
//  65,535 blocks ProDOS addresses.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::Run()
{
    ICassoBlockUnit  * unit = GetUnit();
    BlockDeviceOp      op   = BlockDeviceOp::Status;



    SetXy (0);

    if (unit == nullptr || !unit->GetMedia().IsAttached())
    {
        FlushUnit();
        return kErrNoDevice;
    }

    SetXy (std::min (unit->GetMedia().GetBlockCount(), s_kMaxBlocks));

    switch (m_command)
    {
        case s_kStatus:  op = BlockDeviceOp::Status;  break;
        case s_kRead:    op = BlockDeviceOp::Read;    break;
        case s_kWrite:   op = BlockDeviceOp::Write;   break;
        case s_kFormat:  op = BlockDeviceOp::Format;  break;

        default:
            FlushUnit();
            return kErrBadCommand;
    }

    return Transfer (*unit, op, m_block, m_buffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunSmartPort
//
//  A SmartPort call. The return address points at the last byte of the
//  caller's JSR, followed by the command byte and the parameter list's
//  address: 2 bytes, or 4 for an extended call (bit 6 of the command). The
//  list starts with its parameter count and the unit, 0 for the card
//  itself. Buffers and blocks in an extended list are 4 bytes; the Apple's
//  memory is the low 16 bits.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::RunSmartPort()
{
    Byte               command  = Peek ((Word) (m_return + 1));
    bool               extended = (command & s_kExtended) != 0;
    Word               list     = PeekWord ((Word) (m_return + 2));
    Byte               base     = (Byte) (command & ~s_kExtended);
    Byte               count    = Peek (list);
    int                index    = Peek ((Word) (list + 1)) - 1;
    Word               buffer   = PeekWord ((Word) (list + 2));
    Word               field    = (Word) (list + (extended ? 6 : 4));
    uint32_t           block    = 0;
    ICassoBlockUnit  * unit     = nullptr;
    BlockDeviceOp      op       = BlockDeviceOp::Read;



    m_resume = (Word) (m_return + (extended ? 5 : 3));
    SetXy (0);

    if (base > s_kLastCommand)
    {
        return kErrBadCommand;
    }

    if (count != s_kParamCounts[base])
    {
        return kErrBadParamCount;
    }

    if (base == s_kStatus)
    {
        return Status (index, buffer, Peek (field));
    }

    if (index < 0 || index >= GetUnitCount())
    {
        return kErrBadUnit;
    }

    unit = m_getUnit (index);

    switch (base)
    {
        case s_kRead:
        case s_kWrite:
            op    = base == s_kRead ? BlockDeviceOp::Read : BlockDeviceOp::Write;
            block = (uint32_t) PeekWord (field) | ((uint32_t) Peek ((Word) (field + 2)) << 16);

            if (extended)
            {
                block |= (uint32_t) Peek ((Word) (field + 3)) << 24;
            }

            break;

        case s_kFormat:
            op = BlockDeviceOp::Format;
            break;

        case s_kControl:
            return Peek (field) == 0 ? kErrNone : kErrBadControl;

        case s_kInit:
            return kErrNone;

        default:
            return kErrBadCommand;
    }

    if (!unit->GetMedia().IsAttached())
    {
        return kErrOffline;
    }

    if (op != BlockDeviceOp::Format)
    {
        if (block >= unit->GetMedia().GetBlockCount())
        {
            return kErrBadBlock;
        }

        SetXy ((uint32_t) s_kBlockBytes);
    }

    return Transfer (*unit, op, block, buffer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Transfer
//
//  A command other than a write to the last written drive first saves that
//  drive's writes to its image, so a run of writes is saved once.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::Transfer (ICassoBlockUnit & unit, BlockDeviceOp op, uint32_t block, Word buffer)
{
    Byte  result = kErrNone;



    if (op != BlockDeviceOp::Write || &unit != m_written)
    {
        FlushUnit();
    }

    switch (op)
    {
        case BlockDeviceOp::Read:
            result = ReadTo (unit, block, buffer);
            break;

        case BlockDeviceOp::Write:
            result = WriteFrom (unit, block, buffer);
            break;

        case BlockDeviceOp::Format:
            result = unit.GetMedia().IsWriteProtected() ? kErrWriteProtected : kErrNone;
            break;

        default:
            result = kErrNone;
            break;
    }

    unit.RecordFastAccess (op, block, result, GetNow());

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadTo
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::ReadTo (ICassoBlockUnit & unit, uint32_t block, Word buffer)
{
    std::array<Byte, s_kBlockBytes>  bytes {};
    HRESULT                          hr    = unit.GetMedia().ReadBlock (block, bytes);



    if (FAILED (hr))
    {
        return kErrIo;
    }

    for (size_t i = 0; i < s_kBlockBytes; i++)
    {
        m_bus.WriteByte ((Word) (buffer + i), bytes[i]);
    }

    return kErrNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFrom
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::WriteFrom (ICassoBlockUnit & unit, uint32_t block, Word buffer)
{
    std::array<Byte, s_kBlockBytes>  bytes {};
    HRESULT                          hr    = S_OK;



    if (unit.GetMedia().IsWriteProtected())
    {
        return kErrWriteProtected;
    }

    for (size_t i = 0; i < s_kBlockBytes; i++)
    {
        bytes[i] = m_bus.ReadByte ((Word) (buffer + i));
    }

    hr = unit.GetMedia().WriteBlock (block, bytes);

    if (FAILED (hr))
    {
        return kErrIo;
    }

    m_written = &unit;

    return kErrNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Status
//
//  SmartPort's status call. For the card itself (index -1), code 0 returns
//  the unit count and no interrupt. For a unit, code 0 returns its status
//  byte and 3-byte block count, and code 3 adds the device information
//  block: the name, padded with spaces to 16 bytes, the type, the subtype
//  and a version.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::Status (int index, Word buffer, Byte code)
{
    std::vector<Byte>  reply;
    ICassoBlockUnit  * unit   = nullptr;
    uint32_t           blocks = 0;
    Byte               status = s_kBlockDevice | s_kReadOk | s_kWriteOk | s_kFormatOk;



    if (index < 0)
    {
        if (code != 0)
        {
            return kErrBadControl;
        }

        reply = { (Byte) GetUnitCount(), s_kNoInterrupt, 0, 0, 0, 0, 0, 0 };
    }
    else
    {
        if (index >= GetUnitCount())
        {
            return kErrBadUnit;
        }

        if (code != 0 && code != s_kInfoCode)
        {
            return kErrBadControl;
        }

        unit = m_getUnit (index);

        if (unit->GetMedia().IsAttached())
        {
            blocks  = unit->GetMedia().GetBlockCount();
            status |= s_kOnline;

            if (unit->GetMedia().IsWriteProtected())
            {
                status = (Byte) ((status & ~s_kWriteOk) | s_kProtected);
            }
        }

        reply = { status, (Byte) (blocks & 0xFF), (Byte) ((blocks >> 8) & 0xFF), (Byte) ((blocks >> 16) & 0xFF) };

        if (code == s_kInfoCode)
        {
            reply.push_back ((Byte) std::min (m_device.name.size(), s_kNameBytes));

            for (size_t i = 0; i < s_kNameBytes; i++)
            {
                reply.push_back (i < m_device.name.size() ? (Byte) m_device.name[i] : (Byte) ' ');
            }

            reply.insert (reply.end(), { m_device.type, m_device.subtype, (Byte) 0x00, (Byte) 0x01 });
        }
    }

    for (size_t i = 0; i < reply.size(); i++)
    {
        m_bus.WriteByte ((Word) (buffer + i), reply[i]);
    }

    SetXy ((uint32_t) reply.size());

    return kErrNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUnit
//
//  ProDOS's drive 2 is bit 7 of the unit.
//
////////////////////////////////////////////////////////////////////////////////

ICassoBlockUnit * CassoBlockFirmware::GetUnit() const
{
    return m_getUnit ? m_getUnit ((m_unit & s_kDriveBit) != 0 ? 1 : 0) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUnitCount
//
////////////////////////////////////////////////////////////////////////////////

int CassoBlockFirmware::GetUnitCount() const
{
    int  count = 0;



    while (m_getUnit && m_getUnit (count) != nullptr)
    {
        count++;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushUnit
//
////////////////////////////////////////////////////////////////////////////////

void CassoBlockFirmware::FlushUnit()
{
    HRESULT  hr = S_OK;



    if (m_written != nullptr)
    {
        hr = m_written->GetMedia().Flush();
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_written = nullptr;
    }
}
