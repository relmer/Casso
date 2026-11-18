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
};
