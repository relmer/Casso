#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Seams/InProcessByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeNetworkPeer
//
//  The network an original Uthernet or a LANceGS is plugged into, as the
//  test's frames: the PC, a DHCP server, and the wire between them and the
//  card. The PC's link bytes come and go through an in-process pair's
//  machine end, so a CassoLinkHost on the pair's PC end, through a
//  SteppedByteStream, runs the link over UDP. Each write the host makes is
//  sent as datagrams of at most 1,024 bytes to the Apple's address and port,
//  and each datagram from there to the PC's port is the host's to read.
//
//  The first datagram, empty, is sent once an announcement arrives, as
//  Casso Explorer connects then. The DHCP server offers kAppleIp.
//
////////////////////////////////////////////////////////////////////////////////

class FakeNetworkPeer
{
public:
    static constexpr std::array<Byte, 4>  kPcIp     = { 10, 0, 0, 1 };
    static constexpr std::array<Byte, 4>  kAppleIp  = { 10, 0, 0, 2 };
    static constexpr std::array<Byte, 6>  kPcMac    = { 0x02, 'P', 'C', 0, 0, 1 };
    static constexpr uint16_t             kPcPort   = 50000;
    static constexpr size_t               kChunk    = 1024;

    FakeNetworkPeer (std::array<Byte, 6> appleMac, uint16_t applePort)
        : m_appleMac (appleMac), m_applePort (applePort)
    {
        InProcessByteStream::CreatePair (m_machineEnd, m_pcEnd);
    }

    std::shared_ptr<InProcessByteStream>  GetPcEnd () const { return m_pcEnd; }

    //  The frames the card has sent, by kind.
    const std::vector<std::vector<Byte>> &  GetAnnouncements () const { return m_announcements; }
    const std::vector<std::vector<Byte>> &  GetArpReplies    () const { return m_arpReplies; }
    int                                     GetDhcpAcks      () const { return m_dhcpAcks; }

    //  The next frame for the card, after the host's bytes waiting are made
    //  into datagrams; false with none.
    bool  TakeFrame (std::vector<Byte> & outFrame)
    {
        Byte    chunk[kChunk] = {};
        size_t  read          = 0;
        bool    taken         = false;



        while (m_connected && m_machineEnd->GetReadableCount() > 0)
        {
            m_machineEnd->Read (chunk, sizeof (chunk), 0, read);
            m_toApple.push_back (MakeUdp (m_appleMac, m_appleIp, kPcIp, kPcPort, m_applePort, std::span<const Byte> (chunk, read)));
        }

        if (!m_toApple.empty())
        {
            outFrame = std::move (m_toApple.front());
            m_toApple.pop_front();
            taken = true;
        }

        return taken;
    }

    //  A frame the card sent.
    void  Receive (const std::vector<Byte> & frame)
    {
        uint16_t  type  = (frame.size() >= 14) ? (uint16_t) ((frame[12] << 8) | frame[13]) : 0;
        bool      udp   = type == 0x0800 && frame.size() >= 42 && frame[23] == 17;
        uint16_t  dport = udp ? (uint16_t) ((frame[36] << 8) | frame[37]) : 0;
        size_t    plen  = udp ? (size_t) (((frame[38] << 8) | frame[39]) - 8) : 0;



        if (type == 0x0806 && frame.size() >= 42 && frame[21] == 2)
        {
            m_arpReplies.push_back (frame);
        }
        else if (udp && dport == 6510)
        {
            m_announcements.emplace_back (frame.begin() + 42, frame.begin() + 42 + (ptrdiff_t) plen);
            std::copy_n (frame.begin() + 52, 4, m_appleIp.begin());

            if (!m_holding)
            {
                Connect();
            }
        }
        else if (udp && dport == 67)
        {
            ReplyToDhcp (std::span<const Byte> (frame).subspan (42, plen));
        }
        else if (udp && dport == kPcPort)
        {
            m_machineEnd->Write (frame.data() + 42, plen);
        }
    }

    //  With SetHold (true), the PC connects at Connect rather than at the
    //  first announcement.
    void  SetHold (bool hold) { m_holding = hold; }

    void  Connect()
    {
        if (!m_connected)
        {
            m_connected = true;
            m_toApple.push_back (MakeUdp (m_appleMac, m_appleIp, kPcIp, kPcPort, m_applePort, {}));
        }
    }

    //  An ARP request for the Apple's address, from the PC.
    void  SendArpRequest()
    {
        std::vector<Byte>  frame (42, 0);



        std::fill_n (frame.begin(), 6, 0xFF);
        std::copy (kPcMac.begin(), kPcMac.end(), frame.begin() + 6);

        const Byte  arp[] = { 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01 };

        std::copy (std::begin (arp), std::end (arp), frame.begin() + 12);
        std::copy (kPcMac.begin(), kPcMac.end(), frame.begin() + 22);
        std::copy (kPcIp.begin(), kPcIp.end(), frame.begin() + 28);
        std::copy (m_appleIp.begin(), m_appleIp.end(), frame.begin() + 38);

        m_toApple.push_back (frame);
    }

private:
    static std::vector<Byte>  MakeUdp (const std::array<Byte, 6> & dmac, const std::array<Byte, 4> & dip, const std::array<Byte, 4> & sip,
                                       uint16_t sport, uint16_t dport, std::span<const Byte> payload)
    {
        std::vector<Byte>  frame (42 + payload.size(), 0);
        size_t             ipLength  = 28 + payload.size();
        size_t             udpLength = 8 + payload.size();
        uint32_t           sum       = 0;



        std::copy (dmac.begin(), dmac.end(), frame.begin());
        std::copy (kPcMac.begin(), kPcMac.end(), frame.begin() + 6);
        frame[12] = 0x08;
        frame[14] = 0x45;
        frame[16] = (Byte) (ipLength >> 8);
        frame[17] = (Byte) ipLength;
        frame[22] = 64;
        frame[23] = 17;
        std::copy (sip.begin(), sip.end(), frame.begin() + 26);
        std::copy (dip.begin(), dip.end(), frame.begin() + 30);
        frame[34] = (Byte) (sport >> 8);
        frame[35] = (Byte) sport;
        frame[36] = (Byte) (dport >> 8);
        frame[37] = (Byte) dport;
        frame[38] = (Byte) (udpLength >> 8);
        frame[39] = (Byte) udpLength;
        std::copy (payload.begin(), payload.end(), frame.begin() + 42);

        for (size_t i = 14; i < 34; i += 2)
        {
            sum += (uint32_t) ((frame[i] << 8) | frame[i + 1]);
        }

        sum       = (sum & 0xFFFF) + (sum >> 16);
        frame[24] = (Byte) (~sum >> 8);
        frame[25] = (Byte) ~sum;
        return frame;
    }

    //  An OFFER for a DISCOVER and an ACK for a REQUEST, broadcast to port 68.
    void  ReplyToDhcp (std::span<const Byte> message)
    {
        static constexpr std::array<Byte, 6>  kBroadcastMac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        static constexpr std::array<Byte, 4>  kBroadcastIp  = { 0xFF, 0xFF, 0xFF, 0xFF };
        std::vector<Byte>                     reply (300, 0);
        Byte  type    = (message.size() > 242 && message[240] == 53) ? message[242] : 0;
        Byte  reply53 = (type == 1) ? 2 : (type == 3) ? 5 : 0;



        if (reply53 == 0)
        {
            return;
        }

        reply[0] = 2;
        reply[1] = 1;
        reply[2] = 6;
        std::copy_n (message.begin() + 4, 4, reply.begin() + 4);
        std::copy (kAppleIp.begin(), kAppleIp.end(), reply.begin() + 16);
        std::copy_n (message.begin() + 28, 6, reply.begin() + 28);

        const Byte  options[] = { 0x63, 0x82, 0x53, 0x63, 53, 1, reply53, 54, 4, 10, 0, 0, 1, 1, 4, 255, 255, 255, 0, 255 };

        std::copy (std::begin (options), std::end (options), reply.begin() + 236);

        m_dhcpAcks += (reply53 == 5) ? 1 : 0;
        m_toApple.push_back (MakeUdp (kBroadcastMac, kBroadcastIp, kPcIp, 67, 68, reply));
    }

    std::array<Byte, 6>                   m_appleMac;
    uint16_t                              m_applePort;
    std::array<Byte, 4>                   m_appleIp    = kAppleIp;
    std::shared_ptr<InProcessByteStream>  m_machineEnd;
    std::shared_ptr<InProcessByteStream>  m_pcEnd;
    std::deque<std::vector<Byte>>         m_toApple;
    std::vector<std::vector<Byte>>        m_announcements;
    std::vector<std::vector<Byte>>        m_arpReplies;
    int                                   m_dhcpAcks   = 0;
    bool                                  m_connected  = false;
    bool                                  m_holding    = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeNetworkCard
//
//  A card's 16 registers at $C080 + slot * 16, with its frames to and from
//  a FakeNetworkPeer. A frame is passed to the card only once its receiver
//  is on, and only to its individual address or broadcast, so a driver that
//  set the chip up wrongly receives nothing.
//
////////////////////////////////////////////////////////////////////////////////

class FakeNetworkCard : public MemoryDevice
{
public:
    FakeNetworkCard (int slot, FakeNetworkPeer & peer)
        : m_base ((Word) (0xC080 + slot * 16)), m_peer (peer) {}

    Word  GetStart () const override { return m_base; }
    Word  GetEnd   () const override { return (Word) (m_base + 15); }
    void  Reset    () override       {}

    const std::array<Byte, 6> &  GetAddress () const { return m_address; }

protected:
    //  The next frame from the peer for this card, dropping any the card
    //  would not take.
    bool  NextFrame (std::vector<Byte> & outFrame)
    {
        bool  found = false;



        while (!found && m_receiving && m_peer.TakeFrame (outFrame))
        {
            bool  broadcast = std::all_of (outFrame.begin(), outFrame.begin() + 6, [] (Byte b) { return b == 0xFF; });

            found = broadcast || std::equal (m_address.begin(), m_address.end(), outFrame.begin());
        }

        return found;
    }

    Word                 m_base;
    FakeNetworkPeer    & m_peer;
    std::array<Byte, 6>  m_address   = {};
    bool                 m_receiving = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeCs8900a
//
//  An original Uthernet's CS8900A in its 8-bit I/O mode: the PacketPage
//  pointer and data, the receive and transmit data port, TxCMD and
//  TxLength. A frame's status and length words are read high byte first,
//  and its data low byte first, word by word.
//
////////////////////////////////////////////////////////////////////////////////

class FakeCs8900a : public FakeNetworkCard
{
public:
    using FakeNetworkCard::FakeNetworkCard;

    Byte  Read (Word address) override
    {
        int   offset = address & 0x0F;
        Byte  value  = 0;



        switch (offset)
        {
            case 0x0:
            case 0x1:  value = ReadFrameByte (offset);                                         break;
            case 0xA:  value = (Byte) m_pointer;                                                break;
            case 0xB:  value = (Byte) (((m_pointer >> 8) & 0x8F) | 0x30);                       break;
            case 0xC:  value = (Byte) ReadPacketPage();                                         break;
            case 0xD:  value = (Byte) (ReadPacketPage() >> 8);                                  break;
            default:   break;
        }

        return value;
    }

    void  Write (Word address, Byte value) override
    {
        int  offset = address & 0x0F;



        switch (offset)
        {
            case 0x0:
            case 0x1:  WriteFrameByte (value);                                                  break;
            case 0x6:  m_txLength = (Word) ((m_txLength & 0xFF00) | value);                     break;
            case 0x7:  m_txLength = (Word) ((m_txLength & 0x00FF) | (value << 8)); m_tx.clear(); break;
            case 0xA:  m_pointer  = (Word) ((m_pointer & 0xFF00) | value);                      break;
            case 0xB:  m_pointer  = (Word) ((m_pointer & 0x00FF) | (value << 8));               break;
            case 0xC:  m_low      = value;                                                      break;
            case 0xD:  WritePacketPage ((Word) (m_low | (value << 8)));                         break;
            default:   break;
        }
    }

    Word  GetRxCtl   () const { return m_rxCtl; }
    Word  GetLineCtl () const { return m_lineCtl; }

private:
    Word  ReadPacketPage()
    {
        Word  value = 0;



        switch (m_pointer)
        {
            case 0x0000:  value = 0x630E;                                                       break;
            case 0x0136:  value = 0x0080 | 0x0036;                                              break;
            case 0x0138:  value = 0x0100 | 0x0018;                                              break;
            case 0x0124:  value = (Word) (0x0004 | (HasFrame() ? 0x0100 : 0));                  break;
            default:      break;
        }

        return value;
    }

    void  WritePacketPage (Word value)
    {
        if (m_pointer >= 0x0158 && m_pointer <= 0x015C)
        {
            m_address[m_pointer - 0x0158]     = (Byte) value;
            m_address[m_pointer - 0x0158 + 1] = (Byte) (value >> 8);
        }
        else if (m_pointer == 0x0104)
        {
            m_rxCtl = value;
        }
        else if (m_pointer == 0x0112)
        {
            m_lineCtl   = value;
            m_receiving = (value & 0x0040) != 0 && (m_rxCtl & 0x0D00) == 0x0D00;
        }
        else if (m_pointer == 0x0102 && (value & 0x0040) != 0)
        {
            m_rx.clear();
        }
    }

    bool  HasFrame()
    {
        if (m_rx.empty() && NextFrame (m_rx))
        {
            m_rxWord = 0;
            m_halves = 0;
        }

        return !m_rx.empty();
    }

    //  Word 0 is the status, word 1 the length, and words 2 on the frame.
    Byte  ReadFrameByte (int offset)
    {
        Word  word   = 0;
        Byte  value  = 0;
        bool  odd    = offset == 1;



        if (m_rx.empty())
        {
            return 0;
        }

        word  = (m_rxWord == 0) ? 0x0104 : (m_rxWord == 1) ? (Word) m_rx.size() : 0;
        value = (m_rxWord < 2) ? (Byte) (odd ? word >> 8 : word) : AtOr0 (m_rx, (m_rxWord - 2) * 2 + (odd ? 1 : 0));

        m_halves |= odd ? 2 : 1;

        if (m_halves == 3)
        {
            m_halves = 0;
            m_rxWord++;

            if (m_rxWord >= 2 && (size_t) (m_rxWord - 2) * 2 >= m_rx.size())
            {
                m_rx.clear();
            }
        }

        return value;
    }

    void  WriteFrameByte (Byte value)
    {
        m_tx.push_back (value);

        if (m_txLength != 0 && m_tx.size() >= m_txLength && (m_tx.size() % 2) == 0)
        {
            m_tx.resize (m_txLength);
            m_peer.Receive (m_tx);
            m_tx.clear();
            m_txLength = 0;
        }
    }

    static Byte  AtOr0 (const std::vector<Byte> & bytes, size_t at) { return at < bytes.size() ? bytes[at] : 0; }

    Word               m_pointer  = 0;
    Byte               m_low      = 0;
    Word               m_rxCtl    = 0;
    Word               m_lineCtl  = 0;
    Word               m_txLength = 0;
    std::vector<Byte>  m_tx;
    std::vector<Byte>  m_rx;
    int                m_rxWord   = 0;
    int                m_halves   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeLan91c96
//
//  A LANceGS's LAN91C96: its bank select, bank 0's TCR and RCR, bank 1's
//  individual address and CTR, and bank 2's MMU, packet number, allocation
//  result, receive FIFO, pointer, data and interrupts. A packet is a status
//  word, an even byte count of the whole packet, the frame, and a control
//  byte, with an odd frame's last byte ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

class FakeLan91c96 : public FakeNetworkCard
{
public:
    using FakeNetworkCard::FakeNetworkCard;

    Byte  Read (Word address) override
    {
        int   offset = address & 0x0F;
        Byte  value  = 0;



        if (offset == 0xE)
        {
            value = m_bank;
        }
        else if (offset == 0xF)
        {
            value = 0x33;
        }
        else if (m_bank == 2)
        {
            switch (offset)
            {
                case 0x0:  value = 0;                                                           break;
                case 0x2:  value = m_packet;                                                    break;
                case 0x3:  value = m_allocated;                                                 break;
                case 0x5:  value = HasFrame() ? kRxPacket : 0x80;                               break;
                case 0x8:  value = ReadData();                                                  break;
                case 0xC:  value = m_allocInt ? 0x08 : 0;                                       break;
                default:   break;
            }
        }

        return value;
    }

    void  Write (Word address, Byte value) override
    {
        int  offset = address & 0x0F;



        if (offset == 0xE)
        {
            m_bank = value & 0x07;
        }
        else if (m_bank == 0)
        {
            m_tcr = (offset == 0) ? value : m_tcr;
            m_rcr = (offset == 5) ? value : m_rcr;
        }
        else if (m_bank == 1)
        {
            if (offset >= 4 && offset <= 9)
            {
                m_address[offset - 4] = value;
            }

            m_ctr = (offset == 0xD) ? value : m_ctr;
        }
        else if (m_bank == 2)
        {
            switch (offset)
            {
                case 0x0:  RunCommand (value);                                                  break;
                case 0x2:  m_packet  = value;                                                   break;
                case 0x6:  m_pointer = (Word) ((m_pointer & 0xFF00) | value);                   break;
                case 0x7:  m_pointer = (Word) ((m_pointer & 0x00FF) | (value << 8));            break;
                case 0x8:  WriteData (value);                                                   break;
                default:   break;
            }
        }

        m_receiving = (m_rcr & 0x01) != 0;
    }

    Byte  GetTcr () const { return m_tcr; }
    Byte  GetCtr () const { return m_ctr; }

private:
    static constexpr Byte  kRxPacket = 5;
    static constexpr Byte  kTxPacket = 1;

    bool  HasFrame()
    {
        std::vector<Byte>  frame;
        size_t             count = 0;



        if (m_rx.empty() && NextFrame (frame))
        {
            count = (frame.size() & ~(size_t) 1) + 6;

            m_rx = { 0, (Byte) ((frame.size() & 1) ? 0x10 : 0), (Byte) count, (Byte) (count >> 8) };
            m_rx.insert (m_rx.end(), frame.begin(), frame.end());

            if ((frame.size() & 1) == 0)
            {
                m_rx.push_back (0);
            }

            m_rx.push_back (0x40);
        }

        return !m_rx.empty();
    }

    Byte  ReadData()
    {
        size_t  at    = m_pointer & 0x07FF;
        Byte    value = ((m_pointer & 0x8000) != 0 && at < m_rx.size()) ? m_rx[at] : 0;



        if ((m_pointer & 0x4000) != 0)
        {
            m_pointer = (Word) ((m_pointer & 0xF800) | ((at + 1) & 0x07FF));
        }

        return value;
    }

    void  WriteData (Byte value)
    {
        size_t  at = m_pointer & 0x07FF;



        if (m_tx.size() <= at)
        {
            m_tx.resize (at + 1);
        }

        m_tx[at] = value;

        if ((m_pointer & 0x4000) != 0)
        {
            m_pointer = (Word) ((m_pointer & 0xF800) | ((at + 1) & 0x07FF));
        }
    }

    //  The frame of an enqueued packet: its byte count less 6, and its odd
    //  byte when the control byte has ODD.
    void  RunCommand (Byte command)
    {
        size_t  count  = 0;
        size_t  length = 0;



        switch (command & 0xE0)
        {
            case 0x20:
                m_allocated = kTxPacket;
                m_allocInt  = true;
                m_tx.clear();
                break;

            case 0x40:
                m_rx.clear();
                m_tx.clear();
                m_allocInt = false;
                break;

            case 0x80:
                m_rx.clear();
                break;

            case 0xC0:
                count  = (m_tx.size() >= 4) ? (size_t) (m_tx[2] | (m_tx[3] << 8)) : 0;
                length = (count >= 6 && count <= m_tx.size()) ? count - 6 + ((m_tx[count - 1] & 0x20) ? 1 : 0) : 0;

                if (length > 0 && (m_tcr & 0x01) != 0)
                {
                    m_peer.Receive (std::vector<Byte> (m_tx.begin() + 4, m_tx.begin() + 4 + (ptrdiff_t) length));
                }

                m_allocInt = false;
                break;

            default:
                break;
        }
    }

    Byte               m_bank      = 0;
    Byte               m_tcr       = 0;
    Byte               m_rcr       = 0;
    Byte               m_ctr       = 0;
    Byte               m_packet    = 0;
    Byte               m_allocated = 0;
    bool               m_allocInt  = false;
    Word               m_pointer   = 0;
    std::vector<Byte>  m_tx;
    std::vector<Byte>  m_rx;
};
