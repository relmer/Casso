#pragma once

#include "Pch.h"

#include "Devices/Network/IW5100Backend.h"
#include "Devices/Network/W5100Registers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  W5100
//
//  The WIZnet W5100 as the Uthernet II wires it: indirect bus mode only,
//  four slot locations in front of a 32 KB internal space of common
//  registers, socket registers and TX/RX memory. Everything that touches a
//  network goes through an IW5100Backend; this class is the register file,
//  the address pointer and the buffer arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

class W5100
{
public:

    static constexpr int     kSocketCount = 4;
    static constexpr size_t  kMemorySize  = W5100Registers::kMemoryEnd;

    W5100 ();

    void  SetBackend         (IW5100Backend * backend);

    // The slot's four locations, offset 0-3 from $C0n4.
    Byte  ReadIndirect       (int offset);
    void  WriteIndirect      (int offset, Byte value);

    // Power-on: every register to its reset value and the pointer to 0.
    void  HardwareReset      ();

    bool  IsInterruptPending () const;
    Word  GetAddressPointer  () const { return m_pointer; }

    // Internal space, with the register side effects of an IDM_DR access.
    Byte  ReadInternal       (Word address);
    void  WriteInternal      (Word address, Byte value);

    // Backend interface.
    Byte      GetSocketMode        (int socket) const;
    Byte      GetSocketProtocol    (int socket) const;
    Byte      GetSocketStatus      (int socket) const;
    void      SetSocketStatus      (int socket, Byte status);
    void      RaiseSocketInterrupt (int socket, Byte bits);
    uint16_t  GetSourcePort        (int socket) const;
    uint32_t  GetDestinationIp     (int socket) const;
    uint16_t  GetDestinationPort   (int socket) const;
    void      SetDestination       (int socket, uint32_t ipv4, uint16_t port);
    void      GetSourceMac         (std::array<Byte, W5100Registers::kMacLength> & outMac) const;
    void      SetUnreachable       (uint32_t ipv4, uint16_t port);

    size_t    GetTxBufferSize      (int socket) const { return m_sockets[socket].txSize; }
    size_t    GetRxBufferSize      (int socket) const { return m_sockets[socket].rxSize; }

    // Bytes written by the guest between Sn_TX_RD and Sn_TX_WR, `skip`
    // bytes in; Commit advances Sn_TX_RD.
    size_t    GetTxPending         (int socket) const;
    void      CopyTx               (int socket, size_t skip, size_t count, std::vector<Byte> & out) const;
    void      CommitTx             (int socket, size_t count);

    size_t    GetRxFree            (int socket) const;
    bool      TryAppendRx          (int socket, std::span<const Byte> bytes);

private:

    struct SocketState
    {
        Word    txBase   = 0;
        size_t  txSize   = 0;
        Word    rxBase   = 0;
        size_t  rxSize   = 0;
        Word    rxWrite  = 0;      // where the next received byte goes, in Sn_RX_RD's terms
        Word    latch    = 0;      // 16-bit value latched by a high-byte read
    };

    static Word  AdvancePointer   (Word pointer);
    static Word  GetSocketAddress (int socket, Word offset);
    static bool  IsCommonRegister (Word address);
    static bool  IsSocketRegister (Word offset);

    void      SoftwareReset        ();
    void      ResetRegisters       ();
    void      LayOutBuffers        ();
    void      ResetSocketPointers  (int socket);

    Byte      ReadCommon           (Word address) const;
    void      WriteCommon          (Word address, Byte value);
    Byte      ReadSocket           (int socket, Word offset);
    void      WriteSocket          (int socket, Word offset, Byte value);
    void      ExecuteCommand       (int socket, Byte command);
    void      OpenSocket           (int socket);
    void      WriteMode            (Byte value);

    Word      GetWord              (Word address) const;
    void      SetWord              (Word address, Word value);
    Word      GetSocketWord        (int socket, Word offset) const;
    void      SetSocketWord        (int socket, Word offset, Word value);
    Word      GetTxFreeSize        (int socket) const;
    Word      GetRxReceivedSize    (int socket) const;
    Byte      ReadLatchedWord      (int socket, Word offset, Word value);

    std::array<Byte, kMemorySize>               m_mem     = {};
    std::array<SocketState, kSocketCount>       m_sockets = {};
    Word                                        m_pointer = 0;
    IW5100Backend                             * m_backend = nullptr;
    NullW5100Backend                            m_nullBackend;
};
