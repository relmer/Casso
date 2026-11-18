#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/Acia6551.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple3/Apple3System.h"
#include "Shell/MachineConnectors.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SerialTests
//
//  The built-in 6551 at $C0F0 is serial port 1, and the "serial" port
//  setting attaches its device. The shipped setting is empty, the loopback
//  plug, so a byte sent comes back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SerialTests)
{
public:

    static constexpr uint64_t  kTicksPerSecond = 2040968;
    static constexpr Word      kData           = 0xC0F0;
    static constexpr Word      kStatus         = 0xC0F1;
    static constexpr Word      kCommand        = 0xC0F2;
    static constexpr Word      kControl        = 0xC0F3;

    //  8 data bits, one stop bit, 9600 baud from the internal clock; DTR on,
    //  receive interrupt off, transmit interrupt off with RTS low.
    static constexpr Byte      k9600Baud       = 0x1E;
    static constexpr Byte      kCommandOn      = 0x0B;


    TEST_METHOD (TheBuiltInAciaIsSerialPort1)
    {
        TestMachine  machine ("Apple3");



        Assert::IsFalse (machine.GetConnectors().IsSerialPortOnCard (1), L"the port is built in");
        Assert::AreEqual (S_OK, machine.GetConnectors().ConnectSerialPort (1, std::string(), nullptr, nullptr), L"an empty device is the loopback plug");
    }


    TEST_METHOD (Loopback_ASentByteComesBack)
    {
        TestMachine  machine ("Apple3");
        EmuCpu     & cpu      = *machine.GetCpu();
        bool         received = false;



        machine.GetRunner().RunCycles (kTicksPerSecond / 10);

        cpu.WriteByte (kControl, k9600Baud);
        cpu.WriteByte (kCommand, kCommandOn);
        cpu.WriteByte (kData,    0x55);

        for (int step = 0; step < 100 && !received; step++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond / 1000);
            received = (machine.GetApple3()->GetAcia().Read (kStatus) & Acia6551::kStatusRxFull) != 0;
        }

        Assert::IsTrue   (received, L"the byte arrived within a tenth of a second");
        Assert::AreEqual<int> (0x55, machine.GetApple3()->GetAcia().Read (kData));
    }


    //  At each rate of the internal clock, from 50 to 19200 baud, a byte
    //  comes back through the loopback plug after two character times of a
    //  start bit, 8 data bits and a stop bit. The 6551 model transmits the
    //  byte, and its receiver then shifts the byte in.
    TEST_METHOD (Loopback_EachBaudRate_TakesTwoCharacterTimes)
    {
        TestMachine     machine ("Apple3");
        EmuCpu        & cpu      = *machine.GetCpu();
        Apple3System  & system   = *machine.GetApple3();



        machine.GetRunner().RunCycles (kTicksPerSecond / 10);
        cpu.WriteByte (kCommand, kCommandOn);

        for (int code = 1; code <= Acia6551::kControlBaudMask; code++)
        {
            double    baud      = Acia6551::GetBaudRate (code);
            double    character = 10.0 / baud;
            uint64_t  step      = std::max<uint64_t> (1, (uint64_t) (kTicksPerSecond * character / 200));
            uint64_t  start     = 0;
            double    elapsed   = 0;
            bool      received  = false;



            cpu.WriteByte (kControl, (Byte) ((k9600Baud & ~Acia6551::kControlBaudMask) | code));
            (void) cpu.ReadByte (kData);

            start = system.GetTimeBase().GetMasterClocks();
            cpu.WriteByte (kData, (Byte) code);

            for (int slice = 0; slice < 600 && !received; slice++)
            {
                machine.GetRunner().RunCycles (step);
                received = (system.GetAcia().Read (kStatus) & Acia6551::kStatusRxFull) != 0;
            }

            elapsed = (double) (system.GetTimeBase().GetMasterClocks() - start) / Apple3TimeBase::kMasterClockHz;

            Logger::WriteMessage (std::format ("{} baud: {:.6f} s, character {:.6f} s\n", baud, elapsed, character).c_str());

            Assert::IsTrue        (received, std::format (L"a byte at {} baud came back", baud).c_str());
            Assert::AreEqual<int> (code, system.GetAcia().Read (kData));
            Assert::AreEqual      (2 * character, elapsed, character * 0.1, std::format (L"{} baud", baud).c_str());
        }
    }
};
