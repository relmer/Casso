#include "Pch.h"

#include "UthernetRig.h"

#include "Devices/Network/PacketCodec.h"

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::UthernetRig
//
////////////////////////////////////////////////////////////////////////////////

UthernetRig::UthernetRig (
    const NetworkCardSettings  & settings,
    bool                         attachNetwork)
    : card (kSlot),
      bus  (card.GetChip())
{
    card.AttachNetwork (attachNetwork ? &network : nullptr, settings);
    bus.Initialize();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::Pump
//
////////////////////////////////////////////////////////////////////////////////

void UthernetRig::Pump (int count)
{
    int  i = 0;



    for (i = 0; i < count; i++)
    {
        card.Tick (UthernetCard::kPumpCycles);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::OpenSocket
//
////////////////////////////////////////////////////////////////////////////////

void UthernetRig::OpenSocket (
    int       socket,
    Byte      protocol,
    uint16_t  sourcePort)
{
    bus.Poke     (W5100Driver::SocketReg (socket, R::kSnMr),   protocol);
    bus.PokeWord (W5100Driver::SocketReg (socket, R::kSnPort), sourcePort);
    bus.Command  (socket, R::kCmdOpen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::SetDestination
//
////////////////////////////////////////////////////////////////////////////////

void UthernetRig::SetDestination (
    int       socket,
    uint32_t  ipv4,
    uint16_t  port)
{
    bus.PokeWord (W5100Driver::SocketReg (socket, R::kSnDipr),     (Word) (ipv4 >> 16));
    bus.PokeWord (W5100Driver::SocketReg (socket, R::kSnDipr + 2), (Word) (ipv4 & 0xFFFF));
    bus.PokeWord (W5100Driver::SocketReg (socket, R::kSnDport),    port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::ConnectEstablished
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FakeSocketState> UthernetRig::ConnectEstablished (
    int       socket,
    uint32_t  ipv4,
    uint16_t  port)
{
    std::shared_ptr<FakeSocketState>  peer;



    OpenSocket (socket, R::kProtocolTcp, 0xC000);
    SetDestination (socket, ipv4, port);
    bus.Command (socket, R::kCmdConnect);

    peer = network.FindConnected ({ ipv4, port });

    if (peer != nullptr)
    {
        peer->connect = FakeSocketState::ConnectOutcome::Accepted;
        Pump();
    }

    return peer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig::TryReceiveDatagram
//
////////////////////////////////////////////////////////////////////////////////

bool UthernetRig::TryReceiveDatagram (
    int                  socket,
    ReceivedDatagram   & out)
{
    std::vector<Byte>  bytes  = bus.ReceiveAll (socket);
    size_t             length = 0;



    if (bytes.size() < R::kUdpHeaderLength)
    {
        return false;
    }

    out.ipv4 = PacketCodec::ReadU32 (bytes, 0);
    out.port = PacketCodec::ReadU16 (bytes, 4);
    length   = PacketCodec::ReadU16 (bytes, 6);

    out.payload.assign (bytes.begin() + R::kUdpHeaderLength, bytes.begin() + R::kUdpHeaderLength + length);

    return true;
}
