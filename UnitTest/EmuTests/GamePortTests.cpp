#include "Pch.h"

#include "Devices/AppleGamePort.h"
#include "Core/ComponentRegistry.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  GamePortTests
//
//  Exercises the ][/][+ AppleGamePort device: pushbutton status reads
//  ($C061-$C063), the 558 paddle one-shot ($C070 strobe + $C064-$C067
//  countdown) and the floating-bus behavior that keeps a PREAD poll loop
//  from hanging when no cycle source is wired.
//
////////////////////////////////////////////////////////////////////////////////

namespace EmuTests
{
    TEST_CLASS (GamePortTests)
    {
    public:

        TEST_METHOD (Range_CoversButtonsToTrigger)
        {
            AppleGamePort  port;

            Assert::AreEqual (static_cast<Word> (0xC061), port.GetStart (),
                L"Game port must start at PB0 ($C061)");
            Assert::AreEqual (static_cast<Word> (0xC070), port.GetEnd (),
                L"Game port must end at PTRIG ($C070)");
        }


        TEST_METHOD (Button_Bit7ReflectsPressedState)
        {
            AppleGamePort  port;

            port.Reset ();

            Assert::AreEqual (static_cast<Byte> (0x00), static_cast<Byte> (port.Read (0xC061) & 0x80),
                L"Released PB0 must read bit 7 clear");

            port.SetButton (0, true);
            Assert::AreEqual (static_cast<Byte> (0x80), static_cast<Byte> (port.Read (0xC061) & 0x80),
                L"Pressed PB0 must read bit 7 set");

            port.SetButton (0, false);
            Assert::AreEqual (static_cast<Byte> (0x00), static_cast<Byte> (port.Read (0xC061) & 0x80),
                L"Released PB0 must read bit 7 clear again");
        }


        TEST_METHOD (Button_EachAddressIsIndependent)
        {
            AppleGamePort  port;

            port.Reset ();
            port.SetButton (1, true);

            Assert::AreEqual (static_cast<Byte> (0x00), static_cast<Byte> (port.Read (0xC061) & 0x80),
                L"PB0 must stay clear when only PB1 is pressed");
            Assert::AreEqual (static_cast<Byte> (0x80), static_cast<Byte> (port.Read (0xC062) & 0x80),
                L"PB1 must read set");
            Assert::AreEqual (static_cast<Byte> (0x00), static_cast<Byte> (port.Read (0xC063) & 0x80),
                L"PB2 must stay clear");
        }


        TEST_METHOD (Paddle_NoCycleSourceReadsExpired)
        {
            AppleGamePort  port;

            port.Reset ();
            port.SetPaddle (0, 200);

            // With no cycle source the timer is modeled as already expired so
            // a PREAD poll loop can never hang.
            Assert::AreEqual (static_cast<Byte> (0x00), port.Read (0xC064),
                L"Paddle with no cycle source must read expired (0x00)");
        }


        TEST_METHOD (Paddle_TimerHoldsBit7ForPositionCycles)
        {
            AppleGamePort  port;
            uint64_t       cycles = 0;

            port.Reset ();
            port.SetCpuCycleSource (&cycles);
            port.SetPaddle (0, 100);     // full span = 100 * 11 = 1100 cycles

            // Strobe arms the one-shot at the current cycle.
            cycles = 0;
            port.Read (0xC070);

            cycles = 500;                // 500 < 1100 -> still counting
            Assert::AreEqual (static_cast<Byte> (0x80), port.Read (0xC064),
                L"Paddle must hold bit 7 high while elapsed < position*11");

            cycles = 1200;               // 1200 > 1100 -> expired
            Assert::AreEqual (static_cast<Byte> (0x00), port.Read (0xC064),
                L"Paddle must drop bit 7 once elapsed exceeds position*11");
        }


        TEST_METHOD (Paddle_DefaultsToCenter)
        {
            AppleGamePort  port;
            uint64_t       cycles = 0;

            port.Reset ();
            port.SetCpuCycleSource (&cycles);

            cycles = 0;
            port.Read (0xC070);

            // Center = 127 -> 127*11 = 1397 cycles of bit-7-high.
            cycles = 1000;
            Assert::AreEqual (static_cast<Byte> (0x80), port.Read (0xC064),
                L"Default paddle position must be center (still counting at 1000 cycles)");
        }


        TEST_METHOD (Create_ProducesUsableDevice)
        {
            ComponentRegistry  registry;
            DeviceConfig       cfg;
            MemoryBus          bus;

            ComponentRegistry::RegisterBuiltinDevices (registry);
            cfg.type = "apple2-gameport";

            auto device = registry.Create ("apple2-gameport", cfg, bus);

            Assert::IsNotNull (device.get (), L"Registry must create an apple2-gameport device");
            Assert::AreEqual (static_cast<Word> (0xC061), device->GetStart (),
                L"Created device must claim the game-port range");
        }
    };
}
