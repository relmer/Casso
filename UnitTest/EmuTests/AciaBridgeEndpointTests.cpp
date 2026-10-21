#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpointTests
//
//  The bridge joins the 6551 to a stream, and the 6551 paces the line: one
//  character at a time, at the speed the control register sets, on the
//  machine's clock. A guest that reads each byte in time receives every
//  byte; one that falls behind overruns, as on a real //c. Each test ticks
//  the bridge and the ACIA together, as the machine does.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AciaBridgeEndpointTests)
{
public:

    static constexpr Word  kBase       = 0xC0A8;
    static constexpr Word  kAddrData   = kBase + Acia6551::kRegData;
    static constexpr Word  kAddrStatus = kBase + Acia6551::kRegStatus;
    static constexpr Word  kAddrCmd    = kBase + Acia6551::kRegCommand;
    static constexpr Word  kAddrCtrl   = kBase + Acia6551::kRegControl;

    //  8 data bits, 1 stop bit: 10 bits a character.
    static constexpr Byte  kControl115200 = 0x00;


    struct Line
    {
        Acia6551                              acia   { kBase };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::unique_ptr<AciaBridgeEndpoint>   bridge;

        //  DTR on and the transmitter on, as the link's shim writes them ($0B).
        //  The ACIA neither receives nor sends without them.
        explicit Line (Byte control)
        {
            InProcessByteStream::CreatePair (machineEnd, pcEnd);

            bridge = std::make_unique<AciaBridgeEndpoint> (&acia, machineEnd);
            acia.SetEndpoint (bridge.get());
            acia.Write (kAddrCmd,  Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable | Acia6551::kCommandTicTxIrqOff);
            acia.Write (kAddrCtrl, control);
        }

        void  Run (int cycles)
        {
            for (int cycle = 0; cycle < cycles; cycle++)
            {
                bridge->Tick (1);
                acia.Tick (1);
            }
        }

        //  Cycles until a byte lands, or -1 if none does within `limit`.
        int  RunUntilRxFull (int limit)
        {
            for (int cycle = 1; cycle <= limit; cycle++)
            {
                Run (1);

                if (IsRxFull())
                {
                    return cycle;
                }
            }

            return -1;
        }

        int  GetCharacterCycles() const { return (int) std::ceil (acia.GetTransmitCharacterCycles()); }

        void  SendFromPc (std::initializer_list<Byte> bytes)
        {
            std::vector<Byte>  data (bytes);

            Assert::AreEqual (S_OK, pcEnd->Write (data.data(), data.size()));
        }

        std::vector<Byte>  TakeAtPc()
        {
            std::vector<Byte>  data (64);
            size_t             got = 0;

            Assert::AreEqual (S_OK, pcEnd->Read (data.data(), data.size(), 0, got));
            data.resize (got);
            return data;
        }

        bool  IsRxFull() const { return (acia.GetStatus() & Acia6551::kStatusRxFull) != 0; }
        bool  IsTxEmpty() const { return (acia.GetStatus() & Acia6551::kStatusTxEmpty) != 0; }
    };


    TEST_METHOD (ReceivedBytesLandOneCharacterTimeApart)
    {
        Line  line (kControl115200);
        int   character = line.GetCharacterCycles();
        int   first     = 0;
        int   second    = 0;

        line.SendFromPc ({ 0x11, 0x22 });

        first = line.RunUntilRxFull (character * 3);
        Assert::IsTrue (first > 0, L"the first byte lands");
        Assert::AreEqual<Byte> (0x11, line.acia.Read (kAddrData));

        second = line.RunUntilRxFull (character * 3);
        Assert::IsTrue (second >= character - 1 && second <= character + 1, L"the second byte lands one character time later");
        Assert::AreEqual<Byte> (0x22, line.acia.Read (kAddrData));
    }


    TEST_METHOD (AGuestThatReadsInTimeReceivesEveryByte)
    {
        Line               line (kControl115200);
        std::vector<Byte>  received;

        line.SendFromPc ({ 1, 2, 3, 4, 5, 6, 7, 8 });

        for (int cycle = 0; cycle < line.GetCharacterCycles() * 10; cycle++)
        {
            line.Run (1);

            if (line.IsRxFull())
            {
                received.push_back (line.acia.Read (kAddrData));
            }
        }

        Assert::IsTrue (received == std::vector<Byte> ({ 1, 2, 3, 4, 5, 6, 7, 8 }));
        Assert::IsTrue ((line.acia.GetStatus() & Acia6551::kStatusOverrun) == 0);
    }


    TEST_METHOD (AGuestThatFallsBehindOverruns)
    {
        Line  line (kControl115200);

        line.SendFromPc ({ 1, 2 });
        line.Run (line.GetCharacterCycles() * 3);

        Assert::IsTrue ((line.acia.GetStatus() & Acia6551::kStatusOverrun) != 0);
        Assert::AreEqual<Byte> (2, line.acia.Read (kAddrData), L"the later byte replaces the unread one");
    }


    TEST_METHOD (TransmittedBytesReachTheStreamAsTheirShiftEnds)
    {
        Line  line (kControl115200);
        int   character = line.GetCharacterCycles();

        line.acia.Write (kAddrData, 0xA1);
        line.acia.Write (kAddrData, 0xA2);
        Assert::IsTrue (line.TakeAtPc().empty(), L"nothing is sent until a shift ends");

        line.Run (character + 1);
        Assert::IsTrue (line.TakeAtPc() == std::vector<Byte> ({ 0xA1 }));

        line.Run (character + 1);
        Assert::IsTrue (line.TakeAtPc() == std::vector<Byte> ({ 0xA2 }));
    }


    TEST_METHOD (TheByteLogMarksEachByteWithItsDirection)
    {
        Line                                  line (kControl115200);
        std::vector<std::pair<bool, Byte>>    log;
        std::vector<uint64_t>                 cycles;

        line.bridge->SetByteLog ([&log, &cycles] (bool fromGuest, Byte value, uint64_t cycle) { log.emplace_back (fromGuest, value); cycles.push_back (cycle); });

        line.SendFromPc ({ 0x55 });
        line.Run (1);
        line.acia.Write (kAddrData, 0xAA);
        line.Run (line.GetCharacterCycles() + 1);

        Assert::AreEqual<size_t> (2, log.size());
        Assert::IsTrue (log[0] == std::make_pair (false, (Byte) 0x55));
        Assert::IsTrue (log[1] == std::make_pair (true,  (Byte) 0xAA));
        Assert::IsTrue (cycles[1] > cycles[0], L"the sent byte is logged once its shift ends, after the received one");
    }


    TEST_METHOD (BytesTransmittedWithNothingConnectedAreDropped)
    {
        Line  line (kControl115200);

        line.pcEnd->Close();
        line.acia.Write (kAddrData, 0x01);
        line.Run (line.GetCharacterCycles() + 1);

        Assert::IsTrue (line.IsTxEmpty());
    }
};
