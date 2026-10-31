#pragma once

#include "Pch.h"

class W5100;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Driver
//
//  Drives a W5100 through its four indirect locations the way 6502 code
//  does: set the pointer high then low, then read or write the data port.
//  Tests state register addresses and let this do the bus work.
//
////////////////////////////////////////////////////////////////////////////////

class W5100Driver
{
public:

    explicit W5100Driver (W5100 & chip) : m_chip (chip) {}

    // MR = indirect + auto-increment, after a software reset.
    void               Initialize   ();

    void               SetPointer   (Word address);
    Byte               Peek         (Word address);
    void               Poke         (Word address, Byte value);
    Word               PeekWord     (Word address);
    void               PokeWord     (Word address, Word value);
    std::vector<Byte>  PeekBytes    (Word address, size_t count);
    void               PokeBytes    (Word address, std::span<const Byte> bytes);

    // Socket register helpers.
    static Word        SocketReg    (int socket, Word offset);
    void               Command      (int socket, Byte command);
    Byte               Status       (int socket);
    Byte               Interrupts   (int socket);
    void               ClearInterrupts (int socket, Byte bits);

    // Writes `bytes` into the socket's TX buffer at Sn_TX_WR, splitting at
    // the buffer end as the datasheet's procedure does, advances Sn_TX_WR
    // and issues SEND.
    void               Send         (int socket, std::span<const Byte> bytes);

    // Reads Sn_RX_RSR bytes from Sn_RX_RD, splitting at the buffer end,
    // advances Sn_RX_RD and issues RECV.
    std::vector<Byte>  ReceiveAll   (int socket);

private:

    W5100  & m_chip;
};
