#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaBridgeEndpointTests
//
//  The bridge is a serial line between the 6551 and a stream: one character
//  at a time, at the speed the control register sets, on the machine's clock.
//  A guest that reads each byte in time receives every byte; one that falls
//  behind overruns, as on a real //c.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AciaBridgeEndpointTests)
{
public:

    static constexpr Word  kBase       = 0xC0A8;
    static constexpr Word  kAddrData   = kBase + Acia6551::kRegData;
    static constexpr Word  kAddrStatus = kBase + Acia6551::kRegStatus;
    static constexpr Word  kAddrCtrl   = kBase + Acia6551::kRegControl;

    //  8 data bits, 1 stop bit: 10 bits a character.
    static constexpr Byte  kControl115200 = 0x00;
    static constexpr Byte  kControl9600   = 0x0E;


    struct Line
    {
        Acia6551                              acia   { kBase };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::unique_ptr<AciaBridgeEndpoint>   bridge;

        explicit Line (Byte control)
        {
            InProcessByteStream::CreatePair (machineEnd, pcEnd);

            bridge = std::make_unique<AciaBridgeEndpoint> (&acia, machineEnd);
            acia.SetEndpoint (bridge.get());
            acia.Write (kAddrCtrl, control);
        }

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


    TEST_METHOD (CharacterTimeFollowsTheControlRegister)
    {
        //  10 bits at 115,200 and at 9,600 baud on a 1,020,484 Hz CPU, rounded up.
        Assert::AreEqual<uint32_t> (89,   AciaBridgeEndpoint::GetCyclesPerCharacter (kControl115200, 0));
        Assert::AreEqual<uint32_t> (1064, AciaBridgeEndpoint::GetCyclesPerCharacter (kControl9600, 0));

        //  Two stop bits and a parity bit make 12.
        Assert::AreEqual<uint32_t> ((uint32_t) ((1020484ull * 12 + 9599) / 9600),
                                    AciaBridgeEndpoint::GetCyclesPerCharacter (kControl9600 | Acia6551::kControlStopBits, Acia6551::kCommandParityEnable));
    }


    TEST_METHOD (BaudRateZeroIsTheExternalClockAt115200)
    {
        Assert::AreEqual<uint32_t> (115200, AciaBridgeEndpoint::GetBaudRate (0x00));
        Assert::AreEqual<uint32_t> (9600,   AciaBridgeEndpoint::GetBaudRate (0x0E));
        Assert::AreEqual<uint32_t> (19200,  AciaBridgeEndpoint::GetBaudRate (0x0F));
        Assert::AreEqual<uint32_t> (300,    AciaBridgeEndpoint::GetBaudRate (0x06));
    }


    TEST_METHOD (BytesArriveNoFasterThanOneACharacterTime)
    {
        Line  line (kControl115200);

        line.SendFromPc ({ 0x11, 0x22 });

        line.bridge->Tick (1);
        Assert::IsTrue (line.IsRxFull(), L"the first byte arrives on an idle line");
        Assert::AreEqual<Byte> (0x11, line.acia.Read (kAddrData));

        line.bridge->Tick (88);
        Assert::IsFalse (line.IsRxFull(), L"the second byte is on the wire");

        line.bridge->Tick (1);
        Assert::IsTrue (line.IsRxFull());
        Assert::AreEqual<Byte> (0x22, line.acia.Read (kAddrData));
    }


    TEST_METHOD (AGuestThatReadsInTimeReceivesEveryByte)
    {
        Line               line (kControl115200);
        std::vector<Byte>  received;

        line.SendFromPc ({ 1, 2, 3, 4, 5, 6, 7, 8 });

        for (int cycle = 0; cycle < 89 * 10; cycle++)
        {
            line.bridge->Tick (1);

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

        line.bridge->Tick (1);
        line.bridge->Tick (89);

        Assert::IsTrue ((line.acia.GetStatus() & Acia6551::kStatusOverrun) != 0);
        Assert::AreEqual<Byte> (2, line.acia.Read (kAddrData), L"the later byte replaces the unread one");
    }


    TEST_METHOD (TransmitShiftsOneByteWhileTheNextRemainsInTheRegister)
    {
        Line  line (kControl115200);

        line.acia.Write (kAddrData, 0xA1);
        Assert::IsTrue (line.IsTxEmpty(), L"the first byte moves straight into the shift register");

        line.acia.Write (kAddrData, 0xA2);
        Assert::IsFalse (line.IsTxEmpty(), L"the second remains in the data register");
        Assert::IsTrue (line.TakeAtPc().empty(), L"nothing is sent yet");

        line.bridge->Tick (89);
        Assert::IsTrue (line.TakeAtPc() == std::vector<Byte> ({ 0xA1 }));
        Assert::IsTrue (line.IsTxEmpty(), L"the second byte moved into the shift register");

        line.bridge->Tick (89);
        Assert::IsTrue (line.TakeAtPc() == std::vector<Byte> ({ 0xA2 }));
    }


    TEST_METHOD (TheByteLogMarksEachByteWithItsDirectionAndCycle)
    {
        Line                                  line (kControl115200);
        std::vector<std::pair<bool, Byte>>    log;
        std::vector<uint64_t>                 cycles;

        line.bridge->SetByteLog ([&log, &cycles] (bool fromGuest, Byte value, uint64_t cycle) { log.emplace_back (fromGuest, value); cycles.push_back (cycle); });

        line.SendFromPc ({ 0x55 });
        line.bridge->Tick (1);
        line.acia.Write (kAddrData, 0xAA);
        line.bridge->Tick (89);

        Assert::AreEqual<size_t> (2, log.size());
        Assert::IsTrue (log[0] == std::make_pair (false, (Byte) 0x55));
        Assert::IsTrue (log[1] == std::make_pair (true,  (Byte) 0xAA));

        //  Received on the first cycle; sent once the 89-cycle character has
        //  shifted out after it.
        Assert::IsTrue (cycles == std::vector<uint64_t> ({ 1, 90 }));
    }


    TEST_METHOD (BytesTransmittedWithNothingConnectedAreDropped)
    {
        Line  line (kControl115200);

        line.pcEnd->Close();
        line.acia.Write (kAddrData, 0x01);
        line.bridge->Tick (89);

        Assert::IsTrue (line.IsTxEmpty());
    }
};
