#include "Pch.h"

#include "Devices/Network/W5100.h"

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::W5100
//
////////////////////////////////////////////////////////////////////////////////

W5100::W5100()
{
    m_backend = &m_nullBackend;

    ResetRegisters();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetBackend
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetBackend (IW5100Backend * backend)
{
    m_backend = (backend != nullptr) ? backend : &m_nullBackend;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ReadIndirect
//
//  With IND clear only two address lines matter and the rest are pulled
//  down, so the four locations are internal addresses 0-3 directly.
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::ReadIndirect (int offset)
{
    Byte  value    = 0;
    bool  indirect = (m_mem[R::kMr] & R::kMrIndirect) != 0;



    if (!indirect || offset == R::kIndirectMode)
    {
        return ReadInternal ((Word) offset);
    }

    switch (offset)
    {
        case R::kIndirectAddressHigh:
            value = (Byte) (m_pointer >> 8);
            break;

        case R::kIndirectAddressLow:
            value = (Byte) (m_pointer & 0xFF);
            break;

        default:
            value = ReadInternal (m_pointer);

            if ((m_mem[R::kMr] & R::kMrAutoIncrement) != 0)
            {
                m_pointer = AdvancePointer (m_pointer);
            }

            break;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::WriteIndirect
//
////////////////////////////////////////////////////////////////////////////////

void W5100::WriteIndirect (
    int   offset,
    Byte  value)
{
    bool  indirect = (m_mem[R::kMr] & R::kMrIndirect) != 0;



    if (!indirect || offset == R::kIndirectMode)
    {
        WriteInternal ((Word) offset, value);
        return;
    }

    switch (offset)
    {
        case R::kIndirectAddressHigh:
            m_pointer = (Word) ((m_pointer & 0x00FF) | (value << 8));
            break;

        case R::kIndirectAddressLow:
            m_pointer = (Word) ((m_pointer & 0xFF00) | value);
            break;

        default:
            WriteInternal (m_pointer, value);

            if ((m_mem[R::kMr] & R::kMrAutoIncrement) != 0)
            {
                m_pointer = AdvancePointer (m_pointer);
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::AdvancePointer
//
//  The wrap the Uthernet II manual documents: past the last byte of the TX
//  memory the pointer returns to its first byte, and likewise for the RX
//  memory, including their images above $8000 ($FFFF goes to $E000).
//  Everywhere else the pointer counts up through all 16 bits.
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::AdvancePointer (Word pointer)
{
    constexpr Word  kBufferRegion = 0x4000;
    constexpr Word  kBufferMask   = 0x1FFF;
    bool            inBuffer      = (pointer & kBufferRegion) != 0;
    bool            atLastByte    = (pointer & kBufferMask) == kBufferMask;



    if (inBuffer && atLastByte)
    {
        return (Word) (pointer & ~kBufferMask);
    }

    return (Word) (pointer + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::HardwareReset
//
////////////////////////////////////////////////////////////////////////////////

void W5100::HardwareReset()
{
    ResetRegisters();
    m_pointer = 0;
    m_backend->OnReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SoftwareReset
//
//  MR bit 7. The registers return to their reset values; the pointer does
//  not move, per the Uthernet II manual.
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SoftwareReset()
{
    ResetRegisters();
    m_backend->OnReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ResetRegisters
//
////////////////////////////////////////////////////////////////////////////////

void W5100::ResetRegisters()
{
    int   socket = 0;
    Word  base   = 0;



    std::fill (m_mem.begin(), m_mem.end(), (Byte) 0);

    SetWord (R::kRtr, R::kRtrReset);
    m_mem[R::kRcr]    = R::kRcrReset;
    m_mem[R::kRmsr]   = R::kMemorySizeReset;
    m_mem[R::kTmsr]   = R::kMemorySizeReset;
    m_mem[R::kPtimer] = R::kPtimerReset;

    for (socket = 0; socket < kSocketCount; socket++)
    {
        base = GetSocketAddress (socket, 0);

        std::fill_n (m_mem.begin() + base + R::kSnDhar, R::kMacLength, R::kSnDharReset);
        m_mem[base + R::kSnTtl] = R::kSnTtlReset;

        m_sockets[socket] = {};
    }

    LayOutBuffers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::LayOutBuffers
//
//  Each socket's share of the 8 KB TX and RX memories from TMSR and RMSR,
//  two bits per socket starting with socket 0 in bits 1-0. Shares are
//  allocated from socket 0 up; a socket whose share does not fit in what is
//  left gets none, as the datasheet specifies.
//
////////////////////////////////////////////////////////////////////////////////

void W5100::LayOutBuffers()
{
    Byte    rmsr     = m_mem[R::kRmsr];
    Byte    tmsr     = m_mem[R::kTmsr];
    size_t  rxUsed   = 0;
    size_t  txUsed   = 0;
    size_t  rxSize   = 0;
    size_t  txSize   = 0;
    int     shift    = 0;
    int     socket   = 0;



    for (socket = 0; socket < kSocketCount; socket++)
    {
        shift  = socket * R::kSizeBitsPerSocket;
        rxSize = R::kMinBufferSize << ((rmsr >> shift) & R::kSizeFieldMask);
        txSize = R::kMinBufferSize << ((tmsr >> shift) & R::kSizeFieldMask);

        if (rxUsed + rxSize > R::kBufferTotal)
        {
            rxSize = 0;
        }

        if (txUsed + txSize > R::kBufferTotal)
        {
            txSize = 0;
        }

        m_sockets[socket].rxBase = (Word) (R::kRxMemory + rxUsed);
        m_sockets[socket].rxSize = rxSize;
        m_sockets[socket].txBase = (Word) (R::kTxMemory + txUsed);
        m_sockets[socket].txSize = txSize;

        rxUsed += rxSize;
        txUsed += txSize;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::IsInterruptPending
//
////////////////////////////////////////////////////////////////////////////////

bool W5100::IsInterruptPending() const
{
    Byte  mask = m_mem[R::kImr];



    // The card checks this after every instruction; with every source
    // masked, which is how most software runs, IR is not computed at all.
    if (mask == 0)
    {
        return false;
    }

    return (ReadCommon (R::kIr) & mask) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ReadInternal
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::ReadInternal (Word address)
{
    Word  location = (Word) (address & (kMemorySize - 1));
    int   socket   = 0;
    Word  offset   = 0;



    if (location < R::kCommonEnd)
    {
        return ReadCommon (location);
    }

    if (location >= R::kSocketBase && location < R::kSocketEnd)
    {
        socket = (location - R::kSocketBase) / R::kSocketStride;
        offset = (Word) ((location - R::kSocketBase) % R::kSocketStride);

        return ReadSocket (socket, offset);
    }

    if (location >= R::kTxMemory)
    {
        return m_mem[location];
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::WriteInternal
//
////////////////////////////////////////////////////////////////////////////////

void W5100::WriteInternal (
    Word  address,
    Byte  value)
{
    Word  location = (Word) (address & (kMemorySize - 1));
    int   socket   = 0;
    Word  offset   = 0;



    if (location < R::kCommonEnd)
    {
        WriteCommon (location, value);
    }
    else if (location >= R::kSocketBase && location < R::kSocketEnd)
    {
        socket = (location - R::kSocketBase) / R::kSocketStride;
        offset = (Word) ((location - R::kSocketBase) % R::kSocketStride);

        WriteSocket (socket, offset, value);
    }
    else if (location >= R::kTxMemory)
    {
        m_mem[location] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::IsCommonRegister
//
//  The implemented common registers; the rest of $0000-$002F is reserved.
//
////////////////////////////////////////////////////////////////////////////////

bool W5100::IsCommonRegister (Word address)
{
    constexpr Word  kReservedLow  = 0x0013;
    constexpr Word  kReservedHigh = 0x0014;
    constexpr Word  kGapLow       = 0x001E;
    constexpr Word  kGapHigh      = 0x0027;
    bool            inLowGap      = address >= kReservedLow && address <= kReservedHigh;
    bool            inHighGap     = address >= kGapLow && address <= kGapHigh;



    return address < R::kCommonEnd && !inLowGap && !inHighGap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::IsSocketRegister
//
//  The implemented offsets within a socket block: $00-$16 and $20-$29.
//
////////////////////////////////////////////////////////////////////////////////

bool W5100::IsSocketRegister (Word offset)
{
    return offset <= R::kSnTtl || (offset >= R::kSnTxFsr && offset < R::kSnRegisterEnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ReadCommon
//
//  IR's low nibble is not stored: bit n is set while socket n's Sn_IR is
//  non-zero.
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::ReadCommon (Word address) const
{
    Byte  value  = 0;
    int   socket = 0;



    if (!IsCommonRegister (address))
    {
        return 0;
    }

    if (address != R::kIr)
    {
        return m_mem[address];
    }

    value = (Byte) (m_mem[R::kIr] & ~R::kIrSocketBits);

    for (socket = 0; socket < kSocketCount; socket++)
    {
        if (m_mem[GetSocketAddress (socket, R::kSnIr)] != 0)
        {
            value |= (Byte) (1 << socket);
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::WriteCommon
//
////////////////////////////////////////////////////////////////////////////////

void W5100::WriteCommon (
    Word  address,
    Byte  value)
{
    constexpr Byte  kIrClearable = R::kIrConflict | R::kIrUnreachable | R::kIrPppoe;
    bool            readOnly     = (address >= R::kPatr && address < R::kPatr + 2) || address >= R::kUipr;



    if (!IsCommonRegister (address) || readOnly)
    {
        return;
    }

    switch (address)
    {
        case R::kMr:
            WriteMode (value);
            break;

        case R::kIr:
            m_mem[R::kIr] = (Byte) (m_mem[R::kIr] & ~(value & kIrClearable));
            break;

        case R::kRmsr:
        case R::kTmsr:
            m_mem[address] = value;
            LayOutBuffers();
            break;

        default:
            m_mem[address] = value;
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::WriteMode
//
////////////////////////////////////////////////////////////////////////////////

void W5100::WriteMode (Byte value)
{
    if ((value & R::kMrReset) != 0)
    {
        SoftwareReset();
        return;
    }

    m_mem[R::kMr] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ReadSocket
//
//  Sn_CR always reads $00 because commands complete immediately. Sn_TX_FSR
//  and Sn_RX_RSR are computed; their high-byte read latches the word so the
//  low-byte read that follows matches it.
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::ReadSocket (
    int   socket,
    Word  offset)
{
    Word  base = GetSocketAddress (socket, 0);



    if (!IsSocketRegister (offset) || offset == R::kSnCr)
    {
        return 0;
    }

    if (offset == R::kSnTxFsr || offset == R::kSnTxFsr + 1)
    {
        return ReadLatchedWord (socket, (Word) (offset - R::kSnTxFsr), GetTxFreeSize (socket));
    }

    if (offset == R::kSnRxRsr || offset == R::kSnRxRsr + 1)
    {
        return ReadLatchedWord (socket, (Word) (offset - R::kSnRxRsr), GetRxReceivedSize (socket));
    }

    return m_mem[base + offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ReadLatchedWord
//
//  `half` 0 is the high byte, which latches; 1 is the low byte of the
//  latched value.
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::ReadLatchedWord (
    int   socket,
    Word  half,
    Word  value)
{
    if (half == 0)
    {
        m_sockets[socket].latch = value;
        return (Byte) (value >> 8);
    }

    return (Byte) (m_sockets[socket].latch & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::WriteSocket
//
////////////////////////////////////////////////////////////////////////////////

void W5100::WriteSocket (
    int   socket,
    Word  offset,
    Byte  value)
{
    Word  base     = GetSocketAddress (socket, 0);
    bool  readOnly = offset == R::kSnSr
                  || (offset >= R::kSnTxFsr && offset < R::kSnTxWr)
                  || (offset >= R::kSnRxRsr && offset < R::kSnRxRd);



    if (!IsSocketRegister (offset) || readOnly)
    {
        return;
    }

    switch (offset)
    {
        case R::kSnCr:
            ExecuteCommand (socket, value);
            break;

        case R::kSnIr:
            m_mem[base + R::kSnIr] = (Byte) (m_mem[base + R::kSnIr] & ~value);
            break;

        default:
            m_mem[base + offset] = value;
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ExecuteCommand
//
//  The chip's own share of each command; the backend then does the network
//  part. RECV re-raises RECV while unread data remains, per the datasheet.
//
////////////////////////////////////////////////////////////////////////////////

void W5100::ExecuteCommand (
    int   socket,
    Byte  command)
{
    switch (command)
    {
        case R::kCmdOpen:
            OpenSocket (socket);
            break;

        case R::kCmdClose:
            SetSocketStatus (socket, R::kSockClosed);
            break;

        case R::kCmdRecv:
            if (GetRxReceivedSize (socket) != 0)
            {
                RaiseSocketInterrupt (socket, R::kSnIrReceive);
            }

            break;

        default:
            break;
    }

    m_backend->OnCommand (socket, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::OpenSocket
//
////////////////////////////////////////////////////////////////////////////////

void W5100::OpenSocket (int socket)
{
    Byte  protocol = GetSocketProtocol (socket);
    Byte  status   = R::kSockClosed;



    ResetSocketPointers (socket);

    switch (protocol)
    {
        case R::kProtocolTcp:
            status = R::kSockInit;
            break;

        case R::kProtocolUdp:
            status = R::kSockUdp;
            break;

        case R::kProtocolMacRaw:
            status = (socket == 0) ? R::kSockMacRaw : R::kSockClosed;
            break;

        default:
            status = R::kSockClosed;
            break;
    }

    SetSocketStatus (socket, status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::ResetSocketPointers
//
////////////////////////////////////////////////////////////////////////////////

void W5100::ResetSocketPointers (int socket)
{
    SetSocketWord (socket, R::kSnTxRd, 0);
    SetSocketWord (socket, R::kSnTxWr, 0);
    SetSocketWord (socket, R::kSnRxRd, 0);
    m_sockets[socket].rxWrite = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetTxFreeSize
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::GetTxFreeSize (int socket) const
{
    size_t  size    = m_sockets[socket].txSize;
    size_t  pending = GetTxPending (socket);



    return (Word) (size - pending);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetRxReceivedSize
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::GetRxReceivedSize (int socket) const
{
    Word    read     = GetSocketWord (socket, R::kSnRxRd);
    Word    received = (Word) (m_sockets[socket].rxWrite - read);



    return (Word) std::min ((size_t) received, m_sockets[socket].rxSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetTxPending
//
////////////////////////////////////////////////////////////////////////////////

size_t W5100::GetTxPending (int socket) const
{
    Word  write   = GetSocketWord (socket, R::kSnTxWr);
    Word  read    = GetSocketWord (socket, R::kSnTxRd);
    Word  pending = (Word) (write - read);



    return std::min ((size_t) pending, m_sockets[socket].txSize);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::CopyTx
//
//  Physical address = base + (pointer & (size - 1)), wrapping within the
//  socket's share.
//
////////////////////////////////////////////////////////////////////////////////

void W5100::CopyTx (
    int                  socket,
    size_t               skip,
    size_t               count,
    std::vector<Byte>  & out) const
{
    const SocketState  & state = m_sockets[socket];
    Word                 read  = (Word) (GetSocketWord (socket, R::kSnTxRd) + skip);
    size_t               i     = 0;



    out.clear();

    if (state.txSize == 0)
    {
        return;
    }

    out.reserve (count);

    for (i = 0; i < count; i++)
    {
        out.push_back (m_mem[state.txBase + ((read + i) & (state.txSize - 1))]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::CommitTx
//
////////////////////////////////////////////////////////////////////////////////

void W5100::CommitTx (
    int     socket,
    size_t  count)
{
    Word  read = GetSocketWord (socket, R::kSnTxRd);



    SetSocketWord (socket, R::kSnTxRd, (Word) (read + count));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetRxFree
//
////////////////////////////////////////////////////////////////////////////////

size_t W5100::GetRxFree (int socket) const
{
    return m_sockets[socket].rxSize - GetRxReceivedSize (socket);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::TryAppendRx
//
//  All or nothing: a block that does not fit is left for later, which is
//  how TCP data waits for RECV and a UDP datagram stays whole.
//
////////////////////////////////////////////////////////////////////////////////

bool W5100::TryAppendRx (
    int                     socket,
    std::span<const Byte>   bytes)
{
    SocketState  & state = m_sockets[socket];
    size_t         free  = GetRxFree (socket);
    size_t         i     = 0;



    if (bytes.size() > free || state.rxSize == 0)
    {
        return false;
    }

    for (i = 0; i < bytes.size(); i++)
    {
        m_mem[state.rxBase + ((state.rxWrite + i) & (state.rxSize - 1))] = bytes[i];
    }

    state.rxWrite = (Word) (state.rxWrite + bytes.size());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSocketMode
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::GetSocketMode (int socket) const
{
    return m_mem[GetSocketAddress (socket, R::kSnMr)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSocketProtocol
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::GetSocketProtocol (int socket) const
{
    return (Byte) (GetSocketMode (socket) & R::kSnMrProtocolMask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSocketStatus
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100::GetSocketStatus (int socket) const
{
    return m_mem[GetSocketAddress (socket, R::kSnSr)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetSocketStatus
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetSocketStatus (
    int   socket,
    Byte  status)
{
    m_mem[GetSocketAddress (socket, R::kSnSr)] = status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::RaiseSocketInterrupt
//
////////////////////////////////////////////////////////////////////////////////

void W5100::RaiseSocketInterrupt (
    int   socket,
    Byte  bits)
{
    m_mem[GetSocketAddress (socket, R::kSnIr)] |= bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSourcePort
//
////////////////////////////////////////////////////////////////////////////////

uint16_t W5100::GetSourcePort (int socket) const
{
    return GetSocketWord (socket, R::kSnPort);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetDestinationIp
//
////////////////////////////////////////////////////////////////////////////////

uint32_t W5100::GetDestinationIp (int socket) const
{
    Word  high = GetSocketWord (socket, R::kSnDipr);
    Word  low  = GetSocketWord (socket, (Word) (R::kSnDipr + 2));



    return ((uint32_t) high << 16) | low;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetDestinationPort
//
////////////////////////////////////////////////////////////////////////////////

uint16_t W5100::GetDestinationPort (int socket) const
{
    return GetSocketWord (socket, R::kSnDport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetDestination
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetDestination (
    int       socket,
    uint32_t  ipv4,
    uint16_t  port)
{
    SetSocketWord (socket, R::kSnDipr,              (Word) (ipv4 >> 16));
    SetSocketWord (socket, (Word) (R::kSnDipr + 2), (Word) (ipv4 & 0xFFFF));
    SetSocketWord (socket, R::kSnDport,             port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSourceMac
//
////////////////////////////////////////////////////////////////////////////////

void W5100::GetSourceMac (std::array<Byte, W5100Registers::kMacLength> & outMac) const
{
    std::copy_n (m_mem.begin() + R::kShar, R::kMacLength, outMac.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetUnreachable
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetUnreachable (
    uint32_t  ipv4,
    uint16_t  port)
{
    SetWord (R::kUipr,              (Word) (ipv4 >> 16));
    SetWord ((Word) (R::kUipr + 2), (Word) (ipv4 & 0xFFFF));
    SetWord (R::kUport,             port);

    m_mem[R::kIr] |= R::kIrUnreachable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSocketAddress
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::GetSocketAddress (
    int   socket,
    Word  offset)
{
    return (Word) (R::kSocketBase + socket * R::kSocketStride + offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetWord
//
//  The W5100 is big-endian: the high byte is at the lower address.
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::GetWord (Word address) const
{
    return (Word) ((m_mem[address] << 8) | m_mem[address + 1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetWord
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetWord (
    Word  address,
    Word  value)
{
    m_mem[address]     = (Byte) (value >> 8);
    m_mem[address + 1] = (Byte) (value & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::GetSocketWord
//
////////////////////////////////////////////////////////////////////////////////

Word W5100::GetSocketWord (
    int   socket,
    Word  offset) const
{
    return GetWord (GetSocketAddress (socket, offset));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100::SetSocketWord
//
////////////////////////////////////////////////////////////////////////////////

void W5100::SetSocketWord (
    int   socket,
    Word  offset,
    Word  value)
{
    SetWord (GetSocketAddress (socket, offset), value);
}
