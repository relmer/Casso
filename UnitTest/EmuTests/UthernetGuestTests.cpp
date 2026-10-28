#include "Pch.h"

#include "Assembler.h"
#include "AssemblerTypes.h"
#include "Core/TextEncoding.h"
#include "Cpu.h"
#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Devices/Network/W5100Registers.h"
#include "Machines/Apple2/Common/UthernetCard.h"

#include "../NetworkTests/DhcpTestPackets.h"
#include "../NetworkTests/FakeHostNetwork.h"
#include "Debugger/Events/MachineEventFeed.h"
#include "Debugger/Reverse/ReverseController.h"
#include "EmuTests/EventCacheRebuild.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/MachineStateFile.h"
#include "HResultAssert.h"
#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using P = PacketCodec;
using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetGuestTests
//
//  6502 programs, assembled by Casso's assembler, driving the Uthernet II in
//  slot 3 of a //e, with the test as the peer on the far side of a fake host
//  network. Every byte crosses the card's indirect registers, the W5100's
//  buffers and the socket bridge.
//
////////////////////////////////////////////////////////////////////////////////

namespace UthernetGuestTests
{
    static constexpr Word      kCodeOrg      = 0x6000;
    static constexpr Word      kDoneFlag     = 0x0006;
    static constexpr Word      kSendBuffer   = 0x3000;
    static constexpr Word      kHeaderBuffer = 0x7F00;
    static constexpr Word      kRecvBuffer   = 0x8000;
    static constexpr Byte      kDone         = 0x01;
    static constexpr Byte      kFailed       = 0xFF;
    static constexpr uint32_t  kLanPeer      = 0xC0A8010A;   // 192.168.1.10
    static constexpr uint16_t  kAdtPro       = 6502;
    static constexpr uint16_t  kHttp         = 80;
    static constexpr uint64_t  kSlice        = 10'000;
    static constexpr uint64_t  kCycleLimit   = 40'000'000;
    static constexpr Word      kMailFlag     = 0x0007;
    static constexpr Word      kTxLength     = 0x0008;
    static constexpr Word      kRxCount      = 0x000A;
    static constexpr Word      kSlotBase     = 0x8000;
    static constexpr Word      kSlotSize     = 0x0600;
    static constexpr int       kSlotCount    = 8;

    //  Register labels, scratch locations and the routines every program
    //  shares: pointer setup, 16-bit register reads and writes, and copy
    //  loops between memory and the data port.
    static constexpr char  s_kCommon[] = R"(
MODE    = $C0B4
ADDRH   = $C0B5
ADDRL   = $C0B6
DATA    = $C0B7
DONE    = $06
MAIL    = $07
TXLEN   = $08
RXCOUNT = $0A
ADDLO   = $E8
ADDHI   = $E9
CHUNK   = $EB
WORDLO  = $EC
WORDHI  = $ED
PTR     = $FA
CNT     = $FC

setaddr:    sta ADDRH
            stx ADDRL
            rts

rdword:     jsr setaddr
            lda DATA
            sta WORDHI
            lda DATA
            sta WORDLO
            lda WORDHI
            rts

wrword:     jsr setaddr
            lda WORDHI
            sta DATA
            lda WORDLO
            sta DATA
            rts

poke:       jsr setaddr
            sty DATA
            rts

addword:    clc
            lda WORDLO
            adc ADDLO
            sta WORDLO
            lda WORDHI
            adc ADDHI
            sta WORDHI
            rts

copyin:     ldy #0
ci_loop:    lda CNT
            ora CNT+1
            beq ci_done
            lda DATA
            sta (PTR),y
            inc PTR
            bne ci_cnt
            inc PTR+1
ci_cnt:     lda CNT
            bne ci_lo
            dec CNT+1
ci_lo:      dec CNT
            jmp ci_loop
ci_done:    rts

copyout:    ldy #0
co_loop:    lda CNT
            ora CNT+1
            beq co_done
            lda (PTR),y
            sta DATA
            inc PTR
            bne co_cnt
            inc PTR+1
co_cnt:     lda CNT
            bne co_lo
            dec CNT+1
co_lo:      dec CNT
            jmp co_loop
co_done:    rts

fail:       lda #$FF
            sta DONE
hang:       jmp hang

finish:     lda #$01
            sta DONE
            jmp hang
)";

    //  UDP on socket 1: send the 1 KB at $3000 to 192.168.1.10:6502, then
    //  copy the reply's 8-byte header to $7F00 and its 1 KB payload to $8000.
    static constexpr char  s_kUdpProgram[] = R"(
            .org $6000
start:      lda #$80
            sta MODE
            lda #$03
            sta MODE
            lda #$05
            ldx #$00
            ldy #$02
            jsr poke
            lda #$05
            ldx #$04
            jsr setaddr
            lda #$19
            sta DATA
            lda #$66
            sta DATA
            lda #$05
            ldx #$0C
            jsr setaddr
            lda #192
            sta DATA
            lda #168
            sta DATA
            lda #1
            sta DATA
            lda #10
            sta DATA
            lda #$19
            sta DATA
            lda #$66
            sta DATA
            lda #$05
            ldx #$01
            ldy #$01
            jsr poke
            lda #$05
            ldx #$03
            jsr setaddr
            lda DATA
            cmp #$22
            beq udp_open
            jmp fail
udp_open:   lda #$48
            ldx #$00
            jsr setaddr
            lda #$00
            sta PTR
            lda #$30
            sta PTR+1
            lda #$00
            sta CNT
            lda #$04
            sta CNT+1
            jsr copyout
            lda #$04
            sta WORDHI
            lda #$00
            sta WORDLO
            lda #$05
            ldx #$24
            jsr wrword
            lda #$05
            ldx #$01
            ldy #$20
            jsr poke
udp_wait:   lda #$05
            ldx #$26
            jsr rdword
            ora WORDLO
            beq udp_wait
            lda #$68
            ldx #$00
            jsr setaddr
            lda #$00
            sta PTR
            lda #$7F
            sta PTR+1
            lda #$08
            sta CNT
            lda #$00
            sta CNT+1
            jsr copyin
            lda #$00
            sta PTR
            lda #$80
            sta PTR+1
            lda #$00
            sta CNT
            lda #$04
            sta CNT+1
            jsr copyin
            lda #$04
            sta WORDHI
            lda #$08
            sta WORDLO
            lda #$05
            ldx #$28
            jsr wrword
            lda #$05
            ldx #$01
            ldy #$40
            jsr poke
            jmp finish
)";

    //  TCP on socket 0 (2 KB buffers): connect to 192.168.1.10:80, send the
    //  3 KB at $3000 in 256-byte pages as free space allows, receive 3 KB to
    //  $8000 the same way, and disconnect. 3 KB is half again the buffer, so
    //  Sn_TX_WR and Sn_RX_RD both pass the end of the socket's share.
    static constexpr char  s_kTcpProgram[] = R"(
            .org $6000
start:      lda #$80
            sta MODE
            lda #$03
            sta MODE
            lda #$04
            ldx #$00
            ldy #$01
            jsr poke
            lda #$04
            ldx #$04
            jsr setaddr
            lda #$C0
            sta DATA
            lda #$00
            sta DATA
            lda #$04
            ldx #$0C
            jsr setaddr
            lda #192
            sta DATA
            lda #168
            sta DATA
            lda #1
            sta DATA
            lda #10
            sta DATA
            lda #$00
            sta DATA
            lda #80
            sta DATA
            lda #$04
            ldx #$01
            ldy #$01
            jsr poke
            lda #$04
            ldx #$03
            jsr setaddr
            lda DATA
            cmp #$13
            beq tcp_init
            jmp fail
tcp_init:   lda #$04
            ldx #$01
            ldy #$04
            jsr poke
tcp_wait:   lda #$04
            ldx #$03
            jsr setaddr
            lda DATA
            cmp #$17
            beq tcp_up
            cmp #$00
            bne tcp_wait
            jmp fail
tcp_up:     lda #$00
            sta CHUNK
send_loop:  lda #$04
            ldx #$20
            jsr rdword
            cmp #$01
            bcc send_loop
            lda #$04
            ldx #$24
            jsr rdword
            and #$07
            clc
            adc #$40
            ldx #$00
            jsr setaddr
            lda #$00
            sta PTR
            lda CHUNK
            clc
            adc #$30
            sta PTR+1
            lda #$00
            sta CNT
            lda #$01
            sta CNT+1
            jsr copyout
            inc WORDHI
            lda #$04
            ldx #$24
            jsr wrword
            lda #$04
            ldx #$01
            ldy #$20
            jsr poke
            inc CHUNK
            lda CHUNK
            cmp #12
            bne send_loop
            lda #$00
            sta CHUNK
recv_loop:  lda #$04
            ldx #$26
            jsr rdword
            cmp #$01
            bcc recv_loop
            lda #$04
            ldx #$28
            jsr rdword
            and #$07
            clc
            adc #$60
            ldx #$00
            jsr setaddr
            lda #$00
            sta PTR
            lda CHUNK
            clc
            adc #$80
            sta PTR+1
            lda #$00
            sta CNT
            lda #$01
            sta CNT+1
            jsr copyin
            inc WORDHI
            lda #$04
            ldx #$28
            jsr wrword
            lda #$04
            ldx #$01
            ldy #$40
            jsr poke
            inc CHUNK
            lda CHUNK
            cmp #12
            bne recv_loop
            lda #$04
            ldx #$01
            ldy #$08
            jsr poke
            jmp finish
)";

    //  A MACRAW frame pump on socket 0, given all 8 KB of both buffers so the
    //  chip's own pointer wrap covers the buffer ends. When MAIL is set it
    //  sends the TXLEN-byte frame at $3000; each received frame goes, after
    //  its length, into slot RXCOUNT & 7 of eight $600-byte slots at $8000,
    //  and RXCOUNT counts up. DONE is set once the socket is open.
    static constexpr char  s_kMacRawProgram[] = R"(
            .org $6000
start:      lda #$80
            sta MODE
            lda #$03
            sta MODE
            lda #$00
            ldx #$1A
            ldy #$03
            jsr poke
            lda #$00
            ldx #$1B
            ldy #$03
            jsr poke
            lda #$00
            ldx #$09
            jsr setaddr
            lda #$00
            sta DATA
            lda #$08
            sta DATA
            lda #$DC
            sta DATA
            lda #$11
            sta DATA
            sta DATA
            sta DATA
            lda #$04
            ldx #$00
            ldy #$44
            jsr poke
            lda #$04
            ldx #$01
            ldy #$01
            jsr poke
            lda #$04
            ldx #$03
            jsr setaddr
            lda DATA
            cmp #$42
            beq pump
            jmp fail
pump:       lda #$01
            sta DONE
loop:       lda MAIL
            beq check_rx
            lda #$04
            ldx #$24
            jsr rdword
            and #$1F
            ora #$40
            ldx WORDLO
            jsr setaddr
            lda #$00
            sta PTR
            lda #$30
            sta PTR+1
            lda TXLEN
            sta CNT
            sta ADDLO
            lda TXLEN+1
            sta CNT+1
            sta ADDHI
            jsr copyout
            jsr addword
            lda #$04
            ldx #$24
            jsr wrword
            lda #$04
            ldx #$01
            ldy #$20
            jsr poke
            lda #$00
            sta MAIL
check_rx:   lda #$04
            ldx #$26
            jsr rdword
            ora WORDLO
            beq loop
            lda #$04
            ldx #$28
            jsr rdword
            and #$1F
            ora #$60
            ldx WORDLO
            jsr setaddr
            lda DATA
            sta ADDHI
            lda DATA
            sta ADDLO
            lda RXCOUNT
            and #$07
            tax
            lda slots,x
            sta PTR+1
            lda #$00
            sta PTR
            sec
            lda ADDLO
            sbc #2
            sta CNT
            ldy #0
            sta (PTR),y
            lda ADDHI
            sbc #0
            sta CNT+1
            iny
            sta (PTR),y
            lda #$02
            sta PTR
            jsr copyin
            jsr addword
            lda #$04
            ldx #$28
            jsr wrword
            lda #$04
            ldx #$01
            ldy #$40
            jsr poke
            inc RXCOUNT
            jmp loop
slots:      db $80, $86, $8C, $92, $98, $9E, $A4, $AA
)";


    static constexpr MacAddress::Octets  kGuestMac = { 0x00, 0x08, 0xDC, 0x11, 0x11, 0x11 };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Guest
    //
    //  A //e with the card enabled in slot 3, the fake host network behind it,
    //  and a program loaded at $6000 with the PC on it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Guest
    {
    public:

        explicit Guest (const char * program)
            : m_machine ("Apple2e", TestMachine::Slots::AsShipped,
                         [this] (MachineConfig & config, MachineBuildServices & services)
                         {
                             for (SlotConfig & slot : config.slots)
                             {
                                 if (slot.device == "uthernet2")
                                 {
                                     slot.enabled = true;
                                 }
                             }

                             services.hostNetwork = &host;
                         })
        {
            Cpu             asmCpu;
            Assembler       assembler (asmCpu.GetInstructionSet());
            AssemblyResult  result     = assembler.Assemble (std::string (program) + s_kCommon);
            size_t          i          = 0;

            if (!result.success)
            {
                Assert::Fail (std::format (L"guest program must assemble: {}",
                                           result.errors.empty() ? L"(none)" : TextEncoding::NarrowToWide (result.errors[0].message)).c_str());
            }

            Assert::AreEqual (kCodeOrg, result.startAddress);
            Assert::IsNotNull (m_machine.GetRefs().uthernet, L"the card must be installed");

            for (i = 0; i < result.bytes.size(); i++)
            {
                Poke ((Word) (kCodeOrg + i), result.bytes[i]);
            }

            // Zero page starts with the power-on pattern.
            Poke (kDoneFlag, 0);
            Poke (kMailFlag, 0);
            Poke (kRxCount,  0);

            m_machine.GetCpu()->SetPC (kCodeOrg);
        }

        Byte  Peek (Word address)             { return m_machine.GetMemoryBus().ReadByte (address); }
        void  Poke (Word address, Byte value) { m_machine.GetMemoryBus().WriteByte (address, value); }

        void  PokeBytes (Word address, std::span<const Byte> bytes)
        {
            size_t  i = 0;

            for (i = 0; i < bytes.size(); i++)
            {
                Poke ((Word) (address + i), bytes[i]);
            }
        }

        std::vector<Byte>  PeekBytes (Word address, size_t count)
        {
            std::vector<Byte>  bytes (count);
            size_t             i     = 0;

            for (i = 0; i < count; i++)
            {
                bytes[i] = Peek ((Word) (address + i));
            }

            return bytes;
        }

        // Runs until `done` is true, failing the test past kCycleLimit or when
        // the program sets its failure flag.
        void  RunUntil (const std::function<bool()> & done, const wchar_t * what)
        {
            uint64_t  spent = 0;

            while (!done())
            {
                Assert::AreNotEqual (kFailed, Peek (kDoneFlag), what);
                Assert::IsTrue (spent < kCycleLimit, what);

                if (onSlice)
                {
                    onSlice();
                }

                spent += m_machine.RunCycles (kSlice);
            }
        }

        TestMachine  & GetMachine() { return m_machine; }

        FakeHostNetwork        host;
        std::function<void()>  onSlice;     // called before each slice RunUntil runs

    private:

        TestMachine  m_machine;
    };





    static std::vector<Byte> MakePattern (size_t count, Byte seed)
    {
        std::vector<Byte>  bytes (count);
        size_t             i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (seed + i * 11 + (i >> 8));
        }

        return bytes;
    }





    TEST_CLASS (UthernetGuestTests)
    {
    public:

        TEST_METHOD (UdpRoundTripOfOneKilobyte)
        {
            constexpr size_t  kLength = 1024;

            Guest              guest (s_kUdpProgram);
            std::vector<Byte>  outbound = MakePattern (kLength, 0x21);
            std::vector<Byte>  reply    = MakePattern (kLength, 0x5A);
            std::vector<Byte>  header;

            guest.PokeBytes (kSendBuffer, outbound);

            guest.RunUntil ([&] { auto s = guest.host.FindBound (HostSocketKind::Udp, kAdtPro); return s && !s->sentDatagrams.empty(); },
                            L"the guest sends its datagram");

            auto  socket = guest.host.FindBound (HostSocketKind::Udp, kAdtPro);

            Assert::IsTrue (socket->sentDatagrams[0].peer == HostEndpoint { kLanPeer, kAdtPro });
            Assert::IsTrue (socket->sentDatagrams[0].bytes == outbound, L"1 KB out, byte for byte");

            socket->inboundDatagrams.push_back ({ { kLanPeer, kAdtPro }, reply });
            guest.RunUntil ([&] { return guest.Peek (kDoneFlag) == kDone; }, L"the guest receives the reply");

            header = guest.PeekBytes (kHeaderBuffer, 8);
            Assert::IsTrue (header == std::vector<Byte> { 192, 168, 1, 10, 0x19, 0x66, 0x04, 0x00 }, L"peer IP, port and payload length");
            Assert::IsTrue (guest.PeekBytes (kRecvBuffer, kLength) == reply, L"1 KB back, byte for byte");
        }


        TEST_METHOD (TcpRoundTripLargerThanTheSocketBuffer)
        {
            constexpr size_t  kLength = 3 * 1024;

            Guest                             guest (s_kTcpProgram);
            std::vector<Byte>                 outbound = MakePattern (kLength, 0x13);
            std::vector<Byte>                 reply    = MakePattern (kLength, 0x77);
            std::shared_ptr<FakeSocketState>  peer;

            guest.PokeBytes (kSendBuffer, outbound);

            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() == kLength; }, L"the guest sends 3 KB");
            Assert::IsTrue (peer->sent == outbound, L"3 KB out, in order, across the 2 KB buffer end");

            peer->inbound.assign (reply.begin(), reply.end());
            guest.RunUntil ([&] { return guest.Peek (kDoneFlag) == kDone; }, L"the guest receives 3 KB and disconnects");

            Assert::IsTrue (guest.PeekBytes (kRecvBuffer, kLength) == reply, L"3 KB back, in order");
            Assert::IsTrue (peer->shutdownSent, L"DISCON shuts the host connection down");
        }


        //  The replies from the host's sockets arrive from outside the
        //  machine, so a replay has them only from history. The program
        //  connects, sends and receives 3 KB each way while history is on;
        //  after a seek back to each sample the machine must match its live
        //  state there, and the peer must receive nothing more during the
        //  seeks.
        TEST_METHOD (ATcpSessionReplaysFromHistory)
        {
            constexpr size_t  kLength      = 3 * 1024;
            constexpr size_t  kSampleEvery = 1;
            constexpr size_t  kShuffle     = 7;

            struct Sample
            {
                uint64_t           position = 0;
                uint64_t           cycle    = 0;
                std::vector<Byte>  state;
            };

            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            ReverseController                 controller (machine);
            std::vector<Byte>                 outbound   = MakePattern (kLength, 0x21);
            std::vector<Byte>                 reply      = MakePattern (kLength, 0x5A);
            std::vector<Sample>               samples;
            std::shared_ptr<FakeSocketState>  peer;
            ReverseResult                     result;
            Sample                            last;
            size_t                            slice      = 0;
            size_t                            sent       = 0;
            size_t                            i          = 0;
            HRESULT                           hr         = S_OK;
            auto                              take       = [&machine] ()
            {
                return Sample { machine.GetPosition(), machine.GetCpu()->GetTotalCycles(), ReverseSessionRig::Save (machine) };
            };



            guest.PokeBytes (kSendBuffer, outbound);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            guest.onSlice = [&] ()
            {
                if (slice++ % kSampleEvery == 0)
                {
                    samples.push_back (take());
                }
            };

            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() == kLength; }, L"the guest sends 3 KB");

            peer->inbound.assign (reply.begin(), reply.end());
            guest.RunUntil ([&] { return guest.Peek (kDoneFlag) == kDone; }, L"the guest receives 3 KB and disconnects");

            guest.onSlice = nullptr;
            last          = take();
            sent          = peer->sent.size();

            Assert::IsTrue (guest.PeekBytes (kRecvBuffer, kLength) == reply, L"3 KB back, in order");
            Assert::IsTrue (samples.size() > 4, std::format (L"the session spans several samples, not {}", samples.size()).c_str());

            for (i = 0; i < samples.size(); i++)
            {
                const Sample  & sample = samples[(i * kShuffle) % samples.size()];

                hr = controller.SeekToCycle (sample.cycle, result);
                AssertSucceeded (hr, L"SeekToCycle");

                Assert::AreEqual<uint64_t> (sample.position, result.position, L"the seek lands on the sampled boundary");
                Assert::IsTrue (sample.state == ReverseSessionRig::Save (machine), std::format (L"the replayed state at position {} matches live", sample.position).c_str());
            }

            hr = controller.SeekToCycle (UINT64_MAX, result);
            AssertSucceeded (hr, L"SeekToCycle live");

            Assert::IsTrue (last.state == ReverseSessionRig::Save (machine), L"the live end matches");
            Assert::AreEqual (sent, peer->sent.size(), L"a replay sends nothing to the peer");
        }


        //  The network log's cache has the entries a rebuild from history
        //  makes: the socket's states, and the data it sent and received with
        //  the bytes.
        TEST_METHOD (TheNetworkCacheHasWhatARebuildGives)
        {
            constexpr size_t  kLength = 3 * 1024;

            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            ReverseController                 controller (machine);
            InlineWorkQueue                   queue;
            DebugEventLogs                    logs       (machine);
            std::vector<Byte>                      outbound = MakePattern (kLength, 0x31);
            std::vector<Byte>                      reply    = MakePattern (kLength, 0x4C);
            std::vector<DebugEventEntry<IoEvent>>  entries;
            std::shared_ptr<FakeSocketState>       peer;
            HRESULT                                hr       = S_OK;
            size_t                                 events   = 0;
            size_t                                 sent     = 0;
            size_t                                 received = 0;



            guest.PokeBytes (kSendBuffer, outbound);

            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            logs.SetWorkQueue (&queue);
            logs.Start (&controller);

            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() == kLength; }, L"the guest sends 3 KB");

            peer->inbound.assign (reply.begin(), reply.end());
            guest.RunUntil ([&] { return guest.Peek (kDoneFlag) == kDone; }, L"the guest receives 3 KB and disconnects");

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            EventCacheRebuild::AssertMatches<IoEvent> (machine, controller, logs.GetNetworkCache(), std::make_unique<IoEventProbe> (IoLogKind::Network), events);

            logs.GetNetworkCache().CopySpan (0, UINT64_MAX, entries);

            for (const DebugEventEntry<IoEvent> & entry : entries)
            {
                sent     += (entry.event.kind == IoEventKind::SocketSent)     ? entry.event.length : 0;
                received += (entry.event.kind == IoEventKind::SocketReceived) ? entry.event.length : 0;
            }

            Assert::AreEqual (kLength, sent,     L"the log has every byte sent");
            Assert::AreEqual (kLength, received, L"and every byte received");
            Assert::IsTrue   (std::ranges::any_of (entries, [] (const DebugEventEntry<IoEvent> & e) { return e.event.kind == IoEventKind::SocketState && e.event.value == R::kSockEstablished; }), L"and the socket connecting");

            logs.Stop();
        }


        //  A change made at a point in the past where the connection was open
        //  makes the machine live there. The host's connection is the one the
        //  guest's socket has, so it stays open, and the session goes on.
        TEST_METHOD (AResumeFromThePastKeepsTheConnectionOpen)
        {
            constexpr size_t  kLength = 3 * 1024;

            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            ReverseController                 controller (machine);
            W5100                           & chip       = machine.GetRefs().uthernet->GetChip();
            std::vector<Byte>                 reply      = MakePattern (kLength, 0x5A);
            std::shared_ptr<FakeSocketState>  peer;
            ReverseResult                     result;
            uint64_t                          connected  = 0;
            HRESULT                           hr         = S_OK;



            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() > 0; }, L"the guest starts sending");

            connected = machine.GetCpu()->GetTotalCycles();
            guest.RunUntil ([&] { return machine.GetCpu()->GetTotalCycles() > connected + 4 * kSlice; }, L"the session runs on");

            hr = controller.SeekToCycle (connected, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::IsTrue (controller.IsInHistory(), L"the machine is behind live");

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::IsFalse  (controller.IsInHistory(), L"the machine is live where it stands");
            Assert::AreEqual (R::kSockEstablished, chip.GetSocketStatus (0), L"the guest's socket is connected");
            Assert::IsFalse  (peer->destroyed, L"and the host connection is open");

            peer->inbound.assign (reply.begin(), reply.end());
            guest.RunUntil ([&] { return guest.Peek (kDoneFlag) == kDone; }, L"the guest receives 3 KB and disconnects");

            Assert::IsTrue (guest.PeekBytes (kRecvBuffer, kLength) == reply, L"3 KB back, in order, on the same connection");
            Assert::IsTrue (peer->shutdownSent, L"DISCON shuts the host connection down");
        }


        //  At a point before the guest connected, the host's connection
        //  doesn't match any socket the guest has, so it is closed.
        TEST_METHOD (AResumeFromBeforeTheConnectClosesTheConnection)
        {
            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            ReverseController                 controller (machine);
            W5100                           & chip       = machine.GetRefs().uthernet->GetChip();
            std::shared_ptr<FakeSocketState>  peer;
            ReverseResult                     result;
            uint64_t                          before     = 0;
            HRESULT                           hr         = S_OK;



            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            before = machine.GetCpu()->GetTotalCycles();
            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() > 0; }, L"the guest starts sending");

            hr = controller.SeekToCycle (before, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::AreEqual (R::kSockClosed, chip.GetSocketStatus (0), L"the socket is closed at that point");

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::AreEqual (R::kSockClosed, chip.GetSocketStatus (0), L"the guest's socket is closed");
            Assert::IsTrue   (peer->destroyed, L"and the host connection is closed");
        }


        //  At a point where the connection was open, after the peer has
        //  closed it since, the guest's socket reads as closed by the peer.
        TEST_METHOD (AResumeAfterThePeerClosedReadsAsClosedByThePeer)
        {
            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            ReverseController                 controller (machine);
            W5100                           & chip       = machine.GetRefs().uthernet->GetChip();
            std::shared_ptr<FakeSocketState>  peer;
            ReverseResult                     result;
            uint64_t                          connected  = 0;
            HRESULT                           hr         = S_OK;



            hr = controller.Start (ReverseSessionRig::MakeSettings (1));
            AssertSucceeded (hr, L"Start");

            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() > 0; }, L"the guest starts sending");

            connected        = machine.GetCpu()->GetTotalCycles();
            peer->inboundEof = true;
            guest.RunUntil ([&] { return chip.GetSocketStatus (0) == R::kSockCloseWait; }, L"the peer closes");

            hr = controller.SeekToCycle (connected, result);
            AssertSucceeded (hr, L"SeekToCycle");
            Assert::AreEqual (R::kSockEstablished, chip.GetSocketStatus (0), L"the socket is connected at that point");

            hr = controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::AreEqual (R::kSockCloseWait, chip.GetSocketStatus (0), L"the guest's socket is closed by the peer");
            Assert::IsTrue   ((chip.GetSocketInterrupt (0) & R::kSnIrDisconnect) != 0, L"with the disconnect interrupt");
            Assert::IsFalse  (peer->destroyed, L"until the guest closes it");
        }


        //  A state file saved with a socket open has no host connection behind
        //  it once loaded, so the socket reads as closed by the remote end,
        //  and the connection the machine had before the load is closed.
        TEST_METHOD (ALoadedStateFileClosesTheOpenSockets)
        {
            Guest                             guest      (s_kTcpProgram);
            TestMachine                     & machine    = guest.GetMachine();
            W5100                           & chip       = machine.GetRefs().uthernet->GetChip();
            std::shared_ptr<FakeSocketState>  peer;
            std::vector<Byte>                 bytes;
            MachineStateContents              contents;
            MachineStateError                 error;
            uint64_t                          saved      = 0;
            HRESULT                           hr         = S_OK;



            guest.RunUntil ([&] { return guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the guest connects");

            peer          = guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            guest.RunUntil ([&] { return peer->sent.size() > 0; }, L"the guest starts sending");

            hr = MachineStateFile::Build (machine, bytes, error);
            AssertSucceeded (hr, L"Build");

            saved = machine.GetCpu()->GetTotalCycles();
            guest.RunUntil ([&] { return machine.GetCpu()->GetTotalCycles() > saved + 4 * kSlice; }, L"the session runs on");

            hr = MachineStateFile::Parse (bytes, contents, error);
            AssertSucceeded (hr, L"Parse");

            hr = MachineStateFile::Apply (machine, contents, error);
            AssertSucceeded (hr, L"Apply");

            Assert::AreEqual (R::kSockClosed, chip.GetSocketStatus (0), L"the guest's socket is closed");
            Assert::IsTrue   ((chip.GetSocketInterrupt (0) & R::kSnIrDisconnect) != 0, L"as closed by the remote end");
            Assert::IsTrue   (peer->destroyed, L"the host connection is closed");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FramePump
    //
    //  The MACRAW program plus the test's half of its mailbox: frames in
    //  through MAIL, frames out of the slot ring.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FramePump
    {
    public:

        FramePump() : guest (s_kMacRawProgram)
        {
            guest.RunUntil ([this] { return guest.Peek (kDoneFlag) == kDone; }, L"socket 0 opens in MACRAW");
        }

        void  Send (std::span<const Byte> frame)
        {
            guest.PokeBytes (kSendBuffer, frame);
            guest.Poke (kTxLength,     (Byte) (frame.size() & 0xFF));
            guest.Poke (kTxLength + 1, (Byte) (frame.size() >> 8));
            guest.Poke (kMailFlag, 1);

            guest.RunUntil ([this] { return guest.Peek (kMailFlag) == 0; }, L"the guest sends the frame");
        }

        // Runs until a frame arrives that `accept` takes, discarding others.
        std::vector<Byte>  Receive (const std::function<bool (std::span<const Byte>)> & accept, const wchar_t * what)
        {
            std::vector<Byte>  frame;
            bool               found = false;

            guest.RunUntil ([&]
            {
                while (!found && m_consumed != guest.Peek (kRxCount))
                {
                    Word  slot   = (Word) (kSlotBase + (m_consumed % kSlotCount) * kSlotSize);
                    Word  length = (Word) (guest.Peek (slot) | (guest.Peek ((Word) (slot + 1)) << 8));

                    frame = guest.PeekBytes ((Word) (slot + 2), length);
                    found = accept (frame);
                    m_consumed++;
                }

                return found;
            }, what);

            return frame;
        }

        Guest  guest;

    private:

        Byte  m_consumed = 0;
    };


    static std::vector<Byte> MakeUdpFrame (uint32_t source, uint32_t destination, uint16_t sourcePort, uint16_t destinationPort, std::span<const Byte> payload)
    {
        return P::BuildEthernet (P::kBroadcastMac,
                                 kGuestMac,
                                 P::kEtherTypeIpv4,
                                 P::BuildIpv4 (source, destination, P::kProtocolUdp, 1, P::BuildUdp (source, destination, sourcePort, destinationPort, payload)));
    }


    static std::vector<Byte> MakeTcpFrame (const TcpSegment & segment)
    {
        return P::BuildEthernet (VirtualSubnet::kGatewayMac,
                                 kGuestMac,
                                 P::kEtherTypeIpv4,
                                 P::BuildIpv4 (segment.sourceIp, segment.destinationIp, P::kProtocolTcp, 1, P::BuildTcp (segment)));
    }


    static bool TryParseTcpFrame (std::span<const Byte> frame, TcpSegment & out)
    {
        EthernetFrame  ethernet;
        Ipv4Packet     ip;

        return P::TryParseEthernet (frame, ethernet)
            && ethernet.etherType == P::kEtherTypeIpv4
            && P::TryParseIpv4 (ethernet.payload, ip)
            && ip.protocol == P::kProtocolTcp
            && P::TryParseTcp (ip, out);
    }


    static bool TryParseDhcpReply (std::span<const Byte> frame, Byte messageType, std::vector<Byte> & outPayload)
    {
        EthernetFrame  ethernet;
        Ipv4Packet     ip;
        UdpDatagram    udp;

        if (!P::TryParseEthernet (frame, ethernet)
            || !P::TryParseIpv4 (ethernet.payload, ip)
            || !P::TryParseUdp (ip, udp)
            || udp.destinationPort != 68)
        {
            return false;
        }

        outPayload.assign (udp.payload.begin(), udp.payload.end());

        return DhcpTestPackets::GetMessageType (outPayload) == messageType;
    }





    TEST_CLASS (UthernetMacRawGuestTests)
    {
    public:

        TEST_METHOD (DhcpArpAndATcpRoundTripThroughTheVirtualNetwork)
        {
            constexpr uint32_t  kXid       = 0x1234ABCD;
            constexpr uint32_t  kIsn       = 5000;
            constexpr uint16_t  kGuestPort = 1025;
            constexpr uint16_t  kMss       = 536;
            constexpr uint16_t  kWindow    = 2048;
            constexpr size_t    kLength    = 1024;
            constexpr size_t    kHalf      = kLength / 2;

            FramePump                         pump;
            std::vector<Byte>                 dhcp;
            std::vector<Byte>                 outbound = MakePattern (kLength, 0x31);
            std::vector<Byte>                 reply    = MakePattern (kLength, 0x4C);
            std::vector<Byte>                 received;
            std::shared_ptr<FakeSocketState>  peer;
            TcpSegment                        segment;
            TcpSegment                        incoming;
            ArpPacket                         arp;
            uint32_t                          ourNext  = 0;

            // DHCP: DISCOVER -> OFFER, REQUEST -> ACK, address 10.0.65.15.
            pump.Send (MakeUdpFrame (0, VirtualSubnet::kBroadcastIp, 68, 67, DhcpTestPackets::MakeRequest (DhcpTestPackets::kDiscover, kGuestMac, kXid)));
            pump.Receive ([&] (std::span<const Byte> f) { return TryParseDhcpReply (f, DhcpTestPackets::kOffer, dhcp); }, L"DHCP OFFER");

            pump.Send (MakeUdpFrame (0, VirtualSubnet::kBroadcastIp, 68, 67, DhcpTestPackets::MakeRequest (DhcpTestPackets::kRequest, kGuestMac, kXid)));
            pump.Receive ([&] (std::span<const Byte> f) { return TryParseDhcpReply (f, DhcpTestPackets::kAck, dhcp); }, L"DHCP ACK");
            Assert::AreEqual (VirtualSubnet::kGuestIp, DhcpTestPackets::GetYourAddress (dhcp));

            // ARP for the gateway.
            arp.operation = P::kArpRequest;
            arp.senderMac = kGuestMac;
            arp.senderIp  = VirtualSubnet::kGuestIp;
            arp.targetIp  = VirtualSubnet::kGatewayIp;
            pump.Send (P::BuildEthernet (P::kBroadcastMac, kGuestMac, P::kEtherTypeArp, P::BuildArp (arp)));
            pump.Receive ([&] (std::span<const Byte> f)
            {
                EthernetFrame  e;
                return P::TryParseEthernet (f, e) && e.etherType == P::kEtherTypeArp && P::TryParseArp (e.payload, arp) && arp.operation == P::kArpReply;
            }, L"ARP reply");
            Assert::IsTrue (arp.senderMac == VirtualSubnet::kGatewayMac);

            // TCP handshake with 192.168.1.10:80 through the NAT.
            segment.sourceIp        = VirtualSubnet::kGuestIp;
            segment.destinationIp   = kLanPeer;
            segment.sourcePort      = kGuestPort;
            segment.destinationPort = kHttp;
            segment.sequence        = kIsn;
            segment.flags           = P::kTcpSyn;
            segment.window          = kWindow;
            segment.mss             = kMss;
            pump.Send (MakeTcpFrame (segment));

            pump.guest.RunUntil ([&] { return pump.guest.host.FindConnected ({ kLanPeer, kHttp }) != nullptr; }, L"the NAT connects");
            peer          = pump.guest.host.FindConnected ({ kLanPeer, kHttp });
            peer->connect = FakeSocketState::ConnectOutcome::Accepted;

            pump.Receive ([&] (std::span<const Byte> f) { return TryParseTcpFrame (f, incoming) && incoming.flags == (P::kTcpSyn | P::kTcpAck); }, L"SYN-ACK");
            ourNext = incoming.sequence + 1;

            segment.mss            = 0;
            segment.flags          = P::kTcpAck;
            segment.sequence       = kIsn + 1;
            segment.acknowledgment = ourNext;
            pump.Send (MakeTcpFrame (segment));

            // 1 KB from the guest in two segments.
            segment.flags = P::kTcpAck | P::kTcpPsh;
            segment.payload.assign (outbound.begin(), outbound.begin() + kHalf);
            pump.Send (MakeTcpFrame (segment));

            segment.sequence += (uint32_t) kHalf;
            segment.payload.assign (outbound.begin() + kHalf, outbound.end());
            pump.Send (MakeTcpFrame (segment));

            pump.guest.RunUntil ([&] { return peer->sent.size() == kLength; }, L"1 KB reaches the peer");
            Assert::IsTrue (peer->sent == outbound, L"1 KB out, byte for byte");

            // 1 KB from the peer, acknowledged as it arrives.
            peer->inbound.assign (reply.begin(), reply.end());
            segment.sequence += (uint32_t) kHalf;
            segment.payload.clear();
            segment.flags = P::kTcpAck;

            while (received.size() < kLength)
            {
                pump.Receive ([&] (std::span<const Byte> f) { return TryParseTcpFrame (f, incoming) && !incoming.payload.empty(); }, L"data from the peer");

                Assert::AreEqual (ourNext + (uint32_t) received.size(), incoming.sequence, L"in sequence");
                Assert::IsTrue (incoming.payload.size() <= kMss, L"within the guest's MSS");

                received.insert (received.end(), incoming.payload.begin(), incoming.payload.end());
                segment.acknowledgment = ourNext + (uint32_t) received.size();
                pump.Send (MakeTcpFrame (segment));
            }

            Assert::IsTrue (received == reply, L"1 KB back, byte for byte");
        }
    };
}
