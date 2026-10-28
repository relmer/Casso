#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/Serial/AciaComPortEndpoint.h"
#include "Devices/Serial/SerialLineFormat.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "FakeHostSerialPorts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace AciaComPortEndpointTestNs
{
    static constexpr Word      kBase       = 0xC098;
    static constexpr Word      kAddrData   = 0xC098;
    static constexpr Word      kAddrStatus = 0xC099;
    static constexpr Word      kAddrCmd    = 0xC09A;
    static constexpr Word      kAddrCtrl   = 0xC09B;
    static constexpr Byte      kCmdPolled  = Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte      kCmdRxIrq   = Acia6551::kCommandDtr | Acia6551::kCommandTicTxIrqOff;
    static constexpr Byte      kCtrl9600   = 0x1E;
    static constexpr uint64_t  kSecond     = Acia6551::kDefaultCpuClockHz;
    static constexpr char      kTitle[]    = "Serial port 1";




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ComRig
    //
    //  A 6551 attached to COM3 on the fake PC, set up for 9600 baud polled
    //  I/O.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ComRig
    {
    public:
        explicit ComRig (Byte command = kCmdPolled, const char * portName = "COM3")
            : port     (ports.AddPort ("COM3")),
              endpoint (acia, &ports, reports, kTitle, portName)
        {
            acia.Write (kAddrCtrl, kCtrl9600);
            acia.Write (kAddrCmd,  command);
            acia.SetEndpoint (&endpoint);
            Run (Acia6551::kIdlePollCycles);
        }

        ~ComRig()
        {
            acia.SetEndpoint (nullptr);
        }

        void  Run (uint64_t cycles)
        {
            uint64_t  i = 0;



            for (i = 0; i < cycles; i += kStep)
            {
                acia.Tick (kStep);
            }
        }

        //  Ticks until a byte lands, then reads it, with the status it
        //  landed with.
        Byte  ReadNextByte (Byte & outStatus)
        {
            constexpr uint64_t  kTimeout = kSecond;
            uint64_t            ran      = 0;



            for (ran = 0; ran < kTimeout && (acia.GetStatus() & Acia6551::kStatusRxFull) == 0; ran += kStep)
            {
                acia.Tick (kStep);
            }

            outStatus = acia.GetStatus();

            return acia.Read (kAddrData);
        }

        std::string  GetReport() const
        {
            std::vector<SerialLinkReports::Report>  listed = reports.GetReports();



            return listed.empty() ? std::string() : listed.back().text;
        }

        static constexpr uint32_t  kStep = 4;

        Acia6551                              acia { kBase };
        FakeHostSerialPorts                   ports;
        std::shared_ptr<FakeSerialPortState>  port;
        SerialLinkReports                     reports;
        AciaComPortEndpoint                   endpoint;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaComPortEndpointTests
    //
    //  A 6551 on a PC COM port: the port runs at the guest's format, the
    //  control and status lines pass both ways, line errors arrive with their
    //  bytes, and a port that is absent, busy or unplugged is reported.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AciaComPortEndpointTests)
    {
    public:
        TEST_METHOD (ThePortRunsAtTheGuestsFormat)
        {
            ComRig  rig;



            Assert::IsFalse (rig.port->formats.empty());
            Assert::AreEqual (9600.0, rig.port->formats.back().baud);
            Assert::AreEqual (8, rig.port->formats.back().wordBits);
            Assert::IsTrue (rig.port->formats.back().parity == SerialParity::None);
            Assert::IsTrue (rig.port->formats.back().stopBits == SerialStopBits::One);

            rig.acia.Write (kAddrCtrl, 0x1E | 0x20 | Acia6551::kControlStopBits);
            rig.acia.Write (kAddrCmd,  kCmdPolled | Acia6551::kCommandParityEnable | 0x40);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (7, rig.port->formats.back().wordBits);
            Assert::IsTrue (rig.port->formats.back().parity == SerialParity::Even);
            Assert::IsTrue (rig.port->formats.back().stopBits == SerialStopBits::Two);
        }


        TEST_METHOD (TheFormatIsSetOnlyWhenItChanges)
        {
            ComRig  rig;
            size_t  configured = rig.port->formats.size();



            rig.Run (Acia6551::kIdlePollCycles * 4);

            Assert::AreEqual (configured, rig.port->formats.size());
        }


        TEST_METHOD (DtrRtsAndBreakDriveThePort)
        {
            ComRig  rig;



            Assert::IsTrue (rig.port->dtr);
            Assert::IsTrue (rig.port->rts);

            rig.acia.Write (kAddrCmd, 0x00);
            rig.Run (Acia6551::kIdlePollCycles);
            Assert::IsFalse (rig.port->dtr);
            Assert::IsFalse (rig.port->rts);

            rig.acia.Write (kAddrCmd, Acia6551::kCommandDtr | Acia6551::kCommandTicBreak);
            rig.Run (Acia6551::kIdlePollCycles);
            Assert::IsTrue (rig.port->isBreak);
        }


        TEST_METHOD (TheStatusLinesReachTheChipWithTheInterrupt)
        {
            ComRig  rig (kCmdRxIrq);



            (void) rig.acia.Read (kAddrStatus);
            rig.port->lines = { true, true, false };
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD off");
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusIrq) != 0, L"the change raises the interrupt");

            rig.port->lines = { false, true, false };
            rig.Run (Acia6551::kIdlePollCycles);
            Assert::IsFalse (rig.acia.IsCtsTrue());
        }


        TEST_METHOD (BytesPassBothWays)
        {
            ComRig  rig;



            rig.acia.Write (kAddrData, 'Q');
            rig.Run (Acia6551::kIdlePollCycles * 2);
            Assert::IsTrue (std::vector<Byte> { 'Q' } == rig.port->written);

            rig.port->inbound = { 'R' };
            rig.Run (Acia6551::kIdlePollCycles * 2);
            Assert::AreEqual<Byte> ('R', rig.acia.Read (kAddrData));
        }


        TEST_METHOD (AnErrorArrivesWithTheLastByteRead)
        {
            ComRig  rig;
            Byte    status = 0;



            rig.port->inbound          = { 'a', 'b' };
            rig.port->errors.isFraming = true;
            rig.port->errors.isParity  = true;

            Assert::AreEqual<Byte> ('a', rig.ReadNextByte (status));
            Assert::IsTrue ((status & (Acia6551::kStatusFramingError | Acia6551::kStatusParityError)) == 0, L"the first byte is clean");

            Assert::AreEqual<Byte> ('b', rig.ReadNextByte (status));
            Assert::IsTrue ((status & Acia6551::kStatusFramingError) != 0);
            Assert::IsTrue ((status & Acia6551::kStatusParityError)  != 0);
        }


        TEST_METHOD (ABreakArrivesAsZeroWithAFramingError)
        {
            ComRig  rig;
            Byte    status = 0;



            rig.port->errors.isBreak = true;

            Assert::AreEqual<Byte> (0, rig.ReadNextByte (status));
            Assert::IsTrue ((status & Acia6551::kStatusFramingError) != 0);
        }

        TEST_METHOD (AnOverrunOnThePcIsAnOverrunInTheChip)
        {
            ComRig  rig;



            rig.port->inbound          = { 'z' };
            rig.port->errors.isOverrun = true;
            rig.Run (Acia6551::kIdlePollCycles + (uint64_t) ceil (rig.acia.GetTransmitCharacterCycles()) * 2);

            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusOverrun) != 0);
        }


        //  SC-002: every rate the 6551 offers up to 19,200, both ways.
        TEST_METHOD (EveryRatePassesDataBothWays)
        {
            constexpr int     kHighestCode = 15;
            constexpr double  kBitsPer8N1  = 10.0;
            ComRig            rig;
            int               code         = 0;
            double            baud         = 0.0;
            uint64_t          charCycles   = 0;



            for (code = 1; code <= kHighestCode; code++)
            {
                baud       = SerialLineFormat::GetBaudRate (code);
                charCycles = (uint64_t) ceil (Acia6551::kDefaultCpuClockHz * kBitsPer8N1 / baud);

                rig.port->written.clear();
                rig.acia.Write (kAddrCtrl, (Byte) (Acia6551::kControlRxClockBaud | code));
                rig.Run (Acia6551::kIdlePollCycles);
                Assert::AreEqual ((double) llround (baud), rig.port->formats.back().baud, std::format (L"code {}", code).c_str());

                rig.acia.Write (kAddrData, (Byte) ('@' + code));
                rig.Run (charCycles + Acia6551::kIdlePollCycles * 2);
                Assert::IsTrue (std::vector<Byte> { (Byte) ('@' + code) } == rig.port->written, std::format (L"sent at code {}", code).c_str());

                rig.port->inbound = { (Byte) ('a' + code) };
                rig.Run (charCycles * 2 + Acia6551::kIdlePollCycles * 2);
                Assert::AreEqual<Byte> ((Byte) ('a' + code), rig.acia.Read (kAddrData), std::format (L"received at code {}", code).c_str());
            }
        }


        //  SC-003: a far end at another rate produces framing errors.
        TEST_METHOD (AMismatchedRateProducesFramingErrors)
        {
            ComRig  rig;
            int     errors = 0;
            int     i      = 0;



            for (i = 0; i < 4; i++)
            {
                rig.port->inbound          = { 0xF8 };
                rig.port->errors.isFraming = true;
                rig.Run (Acia6551::kIdlePollCycles + (uint64_t) ceil (rig.acia.GetTransmitCharacterCycles()) * 2);

                errors += (rig.acia.GetStatus() & Acia6551::kStatusFramingError) ? 1 : 0;
                (void) rig.acia.Read (kAddrData);
            }

            Assert::AreEqual (4, errors);
        }


        TEST_METHOD (TheExternalClockSettingRunsAt115200)
        {
            ComRig  rig;



            rig.acia.Write (kAddrCtrl, 0x00);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (115200.0, rig.port->formats.back().baud);
        }


        TEST_METHOD (AnUnsupportedRateUsesTheNearestAndIsReportedOnce)
        {
            ComRig    rig;
            uint64_t  generation = 0;



            rig.port->isAnyRate = false;
            rig.acia.SetEndpoint (nullptr);
            rig.acia.SetEndpoint (&rig.endpoint);
            rig.acia.Write (kAddrCtrl, 0x14);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (110.0, rig.port->formats.back().baud, L"134.58 is nearest 110 among the port's rates");
            Assert::AreEqual (std::string ("134.5 baud is not available on COM3. It runs at 110 baud."), rig.GetReport(), L"by the rate the program chose");

            generation = rig.reports.GetGeneration();
            rig.acia.Write (kAddrCtrl, kCtrl9600);
            rig.Run (Acia6551::kIdlePollCycles);
            rig.acia.Write (kAddrCtrl, 0x14);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (generation, rig.reports.GetGeneration(), L"once per rate");
        }


        //  The 6551's 110 baud is 109.92, which a port runs at 110, the rate the
        //  program chose, so nothing is reported. Its 134.5 is 134.58, which a port that
        //  takes only whole rates runs at 135, off by any amount, so it is.
        TEST_METHOD (ARateOffTheProgramsChoiceByAnyAmountIsReported)
        {
            ComRig  rig;



            rig.acia.Write (kAddrCtrl, 0x13);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (110.0, rig.port->formats.back().baud);
            Assert::AreEqual (std::string(), rig.GetReport(), L"110 is the rate chosen");

            rig.acia.Write (kAddrCtrl, 0x14);
            rig.Run (Acia6551::kIdlePollCycles);

            Assert::AreEqual (135.0, rig.port->formats.back().baud);
            Assert::AreEqual (std::string ("134.5 baud is not available on COM3. It runs at 135 baud."), rig.GetReport());
        }


        TEST_METHOD (AnAbsentPortIsReportedWithDcdOff)
        {
            ComRig  rig (kCmdPolled, "COM9");



            Assert::AreEqual (std::string ("COM9 is not on this PC."), rig.GetReport());
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) != 0);
        }


        TEST_METHOD (APortInUseIsReported)
        {
            Acia6551                              acia (kBase);
            FakeHostSerialPorts                   ports;
            std::shared_ptr<FakeSerialPortState>  port     = ports.AddPort ("COM3");
            SerialLinkReports                     reports;
            AciaComPortEndpoint                   endpoint (acia, &ports, reports, kTitle, "COM3");



            port->isInUse = true;
            acia.SetEndpoint (&endpoint);

            Assert::AreEqual (std::string ("COM3 is in use by another program."), reports.GetReports()[0].text);
            Assert::IsTrue ((acia.GetStatus() & Acia6551::kStatusDcd) != 0);

            acia.SetEndpoint (nullptr);
        }


        TEST_METHOD (UnpluggingIsReportedAndPluggingBackReopens)
        {
            ComRig  rig;



            rig.port->isUnplugged = true;
            rig.Run (Acia6551::kIdlePollCycles * 2);

            Assert::AreEqual (std::string ("COM3 was disconnected."), rig.GetReport());
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) != 0, L"DCD off");
            Assert::IsFalse (rig.port->isOpen);

            rig.port->isUnplugged = false;
            rig.Run (AciaComPortEndpoint::kReopenSeconds * kSecond + Acia6551::kIdlePollCycles * 2);

            Assert::IsTrue (rig.port->isOpen);
            Assert::AreEqual (std::string(), rig.GetReport());
            Assert::IsTrue ((rig.acia.GetStatus() & Acia6551::kStatusDcd) == 0, L"the device's DCD again");
            Assert::IsTrue (rig.port->formats.size() >= 2, L"the format is set again on the reopened port");
        }


        TEST_METHOD (NoPortSelectedIsReported)
        {
            ComRig  rig (kCmdPolled, "");



            Assert::AreEqual (std::string ("No COM port is selected."), rig.GetReport());
        }
    };
}
