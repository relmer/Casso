#include "Pch.h"

#include "Devices/BlockDevice/CassoBlockFirmware.h"
#include "Assembler.h"
#include "Cpu.h"
#include "InstructionSetProvider.h"

#include "CassoBlock.s.h"




static constexpr Word    s_kPageBytes   = 0x100;
static constexpr Word    s_kSlotPages   = 0xC000;
static constexpr Byte    s_kDriveBit    = 0x80;
static constexpr Byte    s_kStatus      = 0;
static constexpr Byte    s_kRead        = 1;
static constexpr Byte    s_kWrite       = 2;
static constexpr Byte    s_kFormat      = 3;
static constexpr size_t  s_kBlockBytes  = BlockDeviceMedia::kBlockBytes;
static constexpr uint32_t s_kMaxBlocks  = 0xFFFF;





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

HRESULT CassoBlockFirmware::Enable (UnitLookup getUnit)
{
    HRESULT      hr = S_OK;
    std::string  errors;



    hr = AssemblePage (m_slot, m_page, errors);
    CHR (hr);

    m_getUnit = std::move (getUnit);

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
        case kRunPort:    value = Run();                          break;
        case kCountLow:   value = (Byte) (GetCount() & 0xFF);     break;
        case kCountHigh:  value = (Byte) (GetCount() >> 8);       break;
        default:          value = m_bus.GetFloatingBusValue();    break;
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
        case kCommandPort:  m_command = value;                                       break;
        case kUnitPort:     m_unit    = value;                                       break;
        case kBufferLow:    m_buffer  = (Word) ((m_buffer & 0xFF00) | value);        break;
        case kBufferHigh:   m_buffer  = (Word) ((m_buffer & 0x00FF) | (value << 8)); break;
        case kBlockLow:     m_block   = (Word) ((m_block  & 0xFF00) | value);        break;
        case kBlockHigh:    m_block   = (Word) ((m_block  & 0x00FF) | (value << 8)); break;
        default:                                                                     break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  A command other than a write to the last written drive first saves that
//  drive's writes to its image, so a run of writes is saved once.
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::Run()
{
    ICassoBlockUnit  * unit   = GetUnit();
    Byte               result = kErrNone;
    BlockDeviceOp      op     = BlockDeviceOp::Status;



    if (m_command != s_kWrite || unit != m_written)
    {
        FlushUnit();
    }

    if (unit == nullptr || !unit->GetMedia().IsAttached())
    {
        return kErrNoDevice;
    }

    switch (m_command)
    {
        case s_kStatus:
            result = kErrNone;
            break;

        case s_kRead:
            op     = BlockDeviceOp::Read;
            result = ReadTo (*unit);
            break;

        case s_kWrite:
            op     = BlockDeviceOp::Write;
            result = WriteFrom (*unit);
            break;

        case s_kFormat:
            op     = BlockDeviceOp::Format;
            result = unit->GetMedia().IsWriteProtected() ? kErrWriteProtected : kErrNone;
            break;

        default:
            result = kErrBadCommand;
            break;
    }

    unit->RecordFastAccess (op, m_block, result, GetNow());

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadTo
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::ReadTo (ICassoBlockUnit & unit)
{
    std::array<Byte, s_kBlockBytes>  block {};
    HRESULT                          hr    = unit.GetMedia().ReadBlock (m_block, block);



    if (FAILED (hr))
    {
        return kErrIo;
    }

    for (size_t i = 0; i < s_kBlockBytes; i++)
    {
        m_bus.WriteByte ((Word) (m_buffer + i), block[i]);
    }

    return kErrNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFrom
//
////////////////////////////////////////////////////////////////////////////////

Byte CassoBlockFirmware::WriteFrom (ICassoBlockUnit & unit)
{
    std::array<Byte, s_kBlockBytes>  block {};
    HRESULT                          hr    = S_OK;



    if (unit.GetMedia().IsWriteProtected())
    {
        return kErrWriteProtected;
    }

    for (size_t i = 0; i < s_kBlockBytes; i++)
    {
        block[i] = m_bus.ReadByte ((Word) (m_buffer + i));
    }

    hr = unit.GetMedia().WriteBlock (m_block, block);

    if (FAILED (hr))
    {
        return kErrIo;
    }

    m_written = &unit;

    return kErrNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUnit
//
////////////////////////////////////////////////////////////////////////////////

ICassoBlockUnit * CassoBlockFirmware::GetUnit() const
{
    return m_getUnit ? m_getUnit ((m_unit & s_kDriveBit) != 0 ? 1 : 0) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCount
//
//  ProDOS addresses at most 65,535 blocks of a volume.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CassoBlockFirmware::GetCount() const
{
    ICassoBlockUnit  * unit = GetUnit();



    if (unit == nullptr || !unit->GetMedia().IsAttached())
    {
        return 0;
    }

    return std::min (unit->GetMedia().GetBlockCount(), s_kMaxBlocks);
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





