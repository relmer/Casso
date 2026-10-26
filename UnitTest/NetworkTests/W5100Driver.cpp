#include "Pch.h"

#include "W5100Driver.h"

#include "Devices/Network/W5100.h"

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Initialize
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::Initialize()
{
    m_chip.WriteIndirect (R::kIndirectMode, R::kMrReset);
    m_chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect | R::kMrAutoIncrement);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::SetPointer
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::SetPointer (Word address)
{
    m_chip.WriteIndirect (R::kIndirectAddressHigh, (Byte) (address >> 8));
    m_chip.WriteIndirect (R::kIndirectAddressLow,  (Byte) (address & 0xFF));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100Driver::Peek (Word address)
{
    SetPointer (address);

    return m_chip.ReadIndirect (R::kIndirectData);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Poke
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::Poke (
    Word  address,
    Byte  value)
{
    SetPointer (address);
    m_chip.WriteIndirect (R::kIndirectData, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::PeekWord
//
//  High byte first, as the datasheet requires for 16-bit registers.
//
////////////////////////////////////////////////////////////////////////////////

Word W5100Driver::PeekWord (Word address)
{
    Byte  high = Peek (address);
    Byte  low  = Peek ((Word) (address + 1));



    return (Word) ((high << 8) | low);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::PokeWord
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::PokeWord (
    Word  address,
    Word  value)
{
    Poke (address,              (Byte) (value >> 8));
    Poke ((Word) (address + 1), (Byte) (value & 0xFF));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::PeekBytes
//
//  One pointer setup, then auto-increment, as a guest copy loop does.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> W5100Driver::PeekBytes (
    Word    address,
    size_t  count)
{
    std::vector<Byte>  bytes (count);
    size_t             i     = 0;



    SetPointer (address);

    for (i = 0; i < count; i++)
    {
        bytes[i] = m_chip.ReadIndirect (R::kIndirectData);
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::PokeBytes
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::PokeBytes (
    Word                    address,
    std::span<const Byte>   bytes)
{
    SetPointer (address);

    for (Byte b : bytes)
    {
        m_chip.WriteIndirect (R::kIndirectData, b);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::SocketReg
//
////////////////////////////////////////////////////////////////////////////////

Word W5100Driver::SocketReg (
    int   socket,
    Word  offset)
{
    return (Word) (R::kSocketBase + socket * R::kSocketStride + offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Command
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::Command (
    int   socket,
    Byte  command)
{
    Poke (SocketReg (socket, R::kSnCr), command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Status
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100Driver::Status (int socket)
{
    return Peek (SocketReg (socket, R::kSnSr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Interrupts
//
////////////////////////////////////////////////////////////////////////////////

Byte W5100Driver::Interrupts (int socket)
{
    return Peek (SocketReg (socket, R::kSnIr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::ClearInterrupts
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::ClearInterrupts (
    int   socket,
    Byte  bits)
{
    Poke (SocketReg (socket, R::kSnIr), bits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::Send
//
////////////////////////////////////////////////////////////////////////////////

void W5100Driver::Send (
    int                     socket,
    std::span<const Byte>   bytes)
{
    Word    base   = R::kTxMemory;
    size_t  size   = m_chip.GetTxBufferSize (socket);
    Word    write  = PeekWord (SocketReg (socket, R::kSnTxWr));
    size_t  offset = write & (size - 1);
    size_t  upper  = std::min (bytes.size(), size - offset);
    int     i      = 0;



    for (i = 0; i < socket; i++)
    {
        base = (Word) (base + m_chip.GetTxBufferSize (i));
    }

    PokeBytes ((Word) (base + offset), bytes.first (upper));
    PokeBytes (base, bytes.subspan (upper));

    PokeWord (SocketReg (socket, R::kSnTxWr), (Word) (write + bytes.size()));
    Command (socket, R::kCmdSend);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver::ReceiveAll
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> W5100Driver::ReceiveAll (int socket)
{
    Word               base     = R::kRxMemory;
    size_t             size     = m_chip.GetRxBufferSize (socket);
    Word               received = PeekWord (SocketReg (socket, R::kSnRxRsr));
    Word               read     = PeekWord (SocketReg (socket, R::kSnRxRd));
    size_t             offset   = read & (size - 1);
    size_t             upper    = std::min ((size_t) received, size - offset);
    std::vector<Byte>  bytes;
    std::vector<Byte>  rest;
    int                i        = 0;



    for (i = 0; i < socket; i++)
    {
        base = (Word) (base + m_chip.GetRxBufferSize (i));
    }

    bytes = PeekBytes ((Word) (base + offset), upper);
    rest  = PeekBytes (base, received - upper);
    bytes.insert (bytes.end(), rest.begin(), rest.end());

    PokeWord (SocketReg (socket, R::kSnRxRd), (Word) (read + received));
    Command (socket, R::kCmdRecv);

    return bytes;
}
