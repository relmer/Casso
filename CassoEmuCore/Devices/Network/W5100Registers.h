#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  W5100Registers
//
//  Addresses, bits and values from the WIZnet W5100 datasheet: the common
//  registers at $0000, the socket register blocks at $0400 + n * $100, and
//  the TX and RX memory. Socket register addresses are offsets within a
//  socket's block.
//
////////////////////////////////////////////////////////////////////////////////

class W5100Registers
{
public:

    // Indirect bus interface: the four locations the slot exposes.
    static constexpr int   kIndirectMode        = 0;
    static constexpr int   kIndirectAddressHigh = 1;
    static constexpr int   kIndirectAddressLow  = 2;
    static constexpr int   kIndirectData        = 3;

    // MR bits.
    static constexpr Byte  kMrReset             = 0x80;
    static constexpr Byte  kMrPingBlock         = 0x10;
    static constexpr Byte  kMrPppoe             = 0x08;
    static constexpr Byte  kMrAutoIncrement     = 0x02;
    static constexpr Byte  kMrIndirect          = 0x01;

    // Common registers.
    static constexpr Word  kMr                  = 0x0000;
    static constexpr Word  kGar                 = 0x0001;
    static constexpr Word  kSubr                = 0x0005;
    static constexpr Word  kShar                = 0x0009;
    static constexpr Word  kSipr                = 0x000F;
    static constexpr Word  kIr                  = 0x0015;
    static constexpr Word  kImr                 = 0x0016;
    static constexpr Word  kRtr                 = 0x0017;
    static constexpr Word  kRcr                 = 0x0019;
    static constexpr Word  kRmsr                = 0x001A;
    static constexpr Word  kTmsr                = 0x001B;
    static constexpr Word  kPatr                = 0x001C;
    static constexpr Word  kPtimer              = 0x0028;
    static constexpr Word  kPmagic              = 0x0029;
    static constexpr Word  kUipr                = 0x002A;
    static constexpr Word  kUport               = 0x002E;
    static constexpr Word  kCommonEnd           = 0x0030;

    static constexpr size_t  kMacLength         = 6;
    static constexpr size_t  kIpv4Length        = 4;

    // Common register reset values that are not zero.
    static constexpr Word  kRtrReset            = 0x07D0;
    static constexpr Byte  kRcrReset            = 0x08;
    static constexpr Byte  kMemorySizeReset     = 0x55;
    static constexpr Byte  kPtimerReset         = 0x28;

    // IR bits.
    static constexpr Byte  kIrConflict          = 0x80;
    static constexpr Byte  kIrUnreachable       = 0x40;
    static constexpr Byte  kIrPppoe             = 0x20;
    static constexpr Byte  kIrSocketBits        = 0x0F;

    // Socket register blocks.
    static constexpr Word  kSocketBase          = 0x0400;
    static constexpr Word  kSocketStride        = 0x0100;
    static constexpr Word  kSocketEnd           = 0x0800;

    static constexpr Word  kSnMr                = 0x00;
    static constexpr Word  kSnCr                = 0x01;
    static constexpr Word  kSnIr                = 0x02;
    static constexpr Word  kSnSr                = 0x03;
    static constexpr Word  kSnPort              = 0x04;
    static constexpr Word  kSnDhar              = 0x06;
    static constexpr Word  kSnDipr              = 0x0C;
    static constexpr Word  kSnDport             = 0x10;
    static constexpr Word  kSnMssr              = 0x12;
    static constexpr Word  kSnProto             = 0x14;
    static constexpr Word  kSnTos               = 0x15;
    static constexpr Word  kSnTtl               = 0x16;
    static constexpr Word  kSnTxFsr             = 0x20;
    static constexpr Word  kSnTxRd              = 0x22;
    static constexpr Word  kSnTxWr              = 0x24;
    static constexpr Word  kSnRxRsr             = 0x26;
    static constexpr Word  kSnRxRd              = 0x28;
    static constexpr Word  kSnRegisterEnd       = 0x2A;

    // Socket register reset values that are not zero.
    static constexpr Byte  kSnDharReset         = 0xFF;
    static constexpr Byte  kSnTtlReset          = 0x80;

    // Sn_MR.
    static constexpr Byte  kSnMrProtocolMask    = 0x0F;
    static constexpr Byte  kSnMrMacFilter       = 0x40;

    static constexpr Byte  kProtocolClosed      = 0x00;
    static constexpr Byte  kProtocolTcp         = 0x01;
    static constexpr Byte  kProtocolUdp         = 0x02;
    static constexpr Byte  kProtocolIpRaw       = 0x03;
    static constexpr Byte  kProtocolMacRaw      = 0x04;
    static constexpr Byte  kProtocolPppoe       = 0x05;

    // Sn_CR commands.
    static constexpr Byte  kCmdOpen             = 0x01;
    static constexpr Byte  kCmdListen           = 0x02;
    static constexpr Byte  kCmdConnect          = 0x04;
    static constexpr Byte  kCmdDisconnect       = 0x08;
    static constexpr Byte  kCmdClose            = 0x10;
    static constexpr Byte  kCmdSend             = 0x20;
    static constexpr Byte  kCmdSendMac          = 0x21;
    static constexpr Byte  kCmdSendKeep         = 0x22;
    static constexpr Byte  kCmdRecv             = 0x40;

    // Sn_IR bits.
    static constexpr Byte  kSnIrConnect         = 0x01;
    static constexpr Byte  kSnIrDisconnect      = 0x02;
    static constexpr Byte  kSnIrReceive         = 0x04;
    static constexpr Byte  kSnIrTimeout         = 0x08;
    static constexpr Byte  kSnIrSendOk          = 0x10;

    // Sn_SR values.
    static constexpr Byte  kSockClosed          = 0x00;
    static constexpr Byte  kSockInit            = 0x13;
    static constexpr Byte  kSockListen          = 0x14;
    static constexpr Byte  kSockSynSent         = 0x15;
    static constexpr Byte  kSockSynReceived     = 0x16;
    static constexpr Byte  kSockEstablished     = 0x17;
    static constexpr Byte  kSockFinWait         = 0x18;
    static constexpr Byte  kSockClosing         = 0x1A;
    static constexpr Byte  kSockTimeWait        = 0x1B;
    static constexpr Byte  kSockCloseWait       = 0x1C;
    static constexpr Byte  kSockLastAck         = 0x1D;
    static constexpr Byte  kSockUdp             = 0x22;
    static constexpr Byte  kSockIpRaw           = 0x32;
    static constexpr Byte  kSockMacRaw          = 0x42;

    // TX and RX memory.
    static constexpr Word    kTxMemory          = 0x4000;
    static constexpr Word    kRxMemory          = 0x6000;
    static constexpr Word    kMemoryEnd         = 0x8000;
    static constexpr size_t  kBufferTotal       = 0x2000;
    static constexpr size_t  kMinBufferSize     = 0x0400;
    static constexpr int     kSizeBitsPerSocket = 2;
    static constexpr Byte    kSizeFieldMask     = 0x03;

    // Receive headers.
    static constexpr size_t  kUdpHeaderLength    = 8;
    static constexpr size_t  kMacRawHeaderLength = 2;
};
