#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Machines/Apple2/Common/UthernetCard.h"

#include "FakeHostNetwork.h"
#include "W5100Driver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetRig
//
//  A card in slot 3 on a fake host network, with a W5100Driver for guest
//  register access. Pump advances the card by one pump interval.
//
////////////////////////////////////////////////////////////////////////////////

class UthernetRig
{
public:

    static constexpr int       kSlot          = 3;
    static constexpr uint32_t  kLanPeer       = 0xC0A8010A;   // 192.168.1.10
    static constexpr uint32_t  kOtherLanPeer  = 0xC0A80114;   // 192.168.1.20

    explicit UthernetRig (const NetworkCardSettings & settings = {}, bool attachNetwork = true);

    void  Pump           (int count = 1);

    void  OpenSocket     (int socket, Byte protocol, uint16_t sourcePort);
    void  SetDestination (int socket, uint32_t ipv4, uint16_t port);

    // CONNECT to `ipv4`:`port`, the fake accepts, and one pump completes it.
    std::shared_ptr<FakeSocketState>  ConnectEstablished (int socket, uint32_t ipv4, uint16_t port);

    // The 8-byte UDP header and payload of the next datagram in the RX
    // buffer, consumed with RECV.
    struct ReceivedDatagram
    {
        uint32_t           ipv4 = 0;
        uint16_t           port = 0;
        std::vector<Byte>  payload;
    };

    bool  TryReceiveDatagram (int socket, ReceivedDatagram & out);

    FakeHostNetwork  network;
    UthernetCard     card;
    W5100Driver      bus;
};
