#include "Pch.h"

#include "Shell/MachineGamePortSink.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleMouse.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineGamePortSinkTests
//
//  The sink decides where each game-port line lands on each machine, so the
//  tests read the answer back through the addresses the guest reads, on real
//  devices: $C061-$C063 for the buttons and the $C070 one-shot for the axes.
//
//  PB2 is the interesting line. The ][+ has it on the game port. The //e
//  shares it with Shift, so a PB2 press reads as Shift, as on the hardware.
//  The //c wires its mouse button to that line, so the sink must leave it
//  alone there or a controller button could click the mouse.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (MachineGamePortSinkTests)
    {
    public:

        static constexpr Word      kPb0Address       = 0xC061;
        static constexpr Word      kPb1Address       = 0xC062;
        static constexpr Word      kPb2Address       = 0xC063;
        static constexpr Word      kPaddleStrobe     = 0xC070;
        static constexpr Word      kPaddle0Address   = 0xC064;
        static constexpr Byte      kButtonBit        = 0x80;
        static constexpr uint64_t  kCyclesPerUnit    = 11;


        static GamePortState MakeState (Byte x, Byte y, bool pb0, bool pb1, bool pb2)
        {
            GamePortState  state;

            state.paddle[0] = x;
            state.paddle[1] = y;
            state.buttons.set (0, pb0);
            state.buttons.set (1, pb1);
            state.buttons.set (2, pb2);
            return state;
        }


        // True when the device's paddle one-shot for `axis` is still holding
        // at `probeCycle` after a strobe at cycle 0 -- that is, the staged
        // position is greater than probeCycle / 11.
        template <typename TDevice>
        static bool IsPaddleHolding (TDevice & device, uint64_t & cycles, int axis, uint64_t probeCycle)
        {
            cycles = 0;
            device.Read (kPaddleStrobe);
            cycles = probeCycle;
            return (device.Read (static_cast<Word> (kPaddle0Address + axis)) & kButtonBit) != 0;
        }


        TEST_METHOD (Apple2Plus_WritesPaddlesAndAllThreeButtons)
        {
            std::shared_mutex    lifetime;
            AppleGamePort        port;
            uint64_t             cycles  = 0;
            GamePortTargets      targets;
            bool                 applied = false;

            port.Reset();
            port.SetCpuCycleSource (&cycles);
            targets.gamePort = &port;

            MachineGamePortSink  sink (lifetime, [&targets] { return targets; });

            applied = sink.TryApply (MakeState (100, 20, true, true, true), nullptr);

            Assert::IsTrue (applied, L"an unlocked machine must accept the write");
            Assert::IsTrue  (IsPaddleHolding (port, cycles, 0, 100 * kCyclesPerUnit - 1), L"PDL0 must hold just under 100");
            Assert::IsFalse (IsPaddleHolding (port, cycles, 0, 100 * kCyclesPerUnit + 1), L"PDL0 must expire just over 100");
            Assert::IsFalse (IsPaddleHolding (port, cycles, 1, 20 * kCyclesPerUnit + 1),  L"PDL1 must expire just over 20");
            Assert::AreEqual (kButtonBit, static_cast<Byte> (port.Read (kPb0Address) & kButtonBit), L"PB0 must read pressed");
            Assert::AreEqual (kButtonBit, static_cast<Byte> (port.Read (kPb1Address) & kButtonBit), L"PB1 must read pressed");
            Assert::AreEqual (kButtonBit, static_cast<Byte> (port.Read (kPb2Address) & kButtonBit), L"PB2 must read pressed on the ][+");
        }


        TEST_METHOD (Apple2e_WritesPaddlesAndAppleKeysWithPb2AsShift)
        {
            std::shared_mutex      lifetime;
            Apple2eSoftSwitchBank  bank (nullptr);
            Apple2eKeyboard        keyboard (nullptr);
            uint64_t               cycles = 0;
            GamePortTargets        targets;

            bank.SetCpuCycleSource (&cycles);
            targets.iieSwitches = &bank;
            targets.iieKeyboard = &keyboard;

            MachineGamePortSink  sink (lifetime, [&targets] { return targets; });

            sink.TryApply (MakeState (200, 127, true, false, true), nullptr);

            Assert::IsTrue  (IsPaddleHolding (bank, cycles, 0, 200 * kCyclesPerUnit - 1), L"PDL0 must hold just under 200");
            Assert::IsFalse (IsPaddleHolding (bank, cycles, 0, 200 * kCyclesPerUnit + 1), L"PDL0 must expire just over 200");
            Assert::IsTrue  (keyboard.IsOpenApplePressed(),    L"PB0 is Open-Apple on the //e");
            Assert::IsFalse (keyboard.IsClosedApplePressed(),  L"PB1 must stay released");
            Assert::IsTrue  (keyboard.IsShiftPressed(),        L"PB2 is the Shift line on the //e");
            Assert::AreEqual (kButtonBit, static_cast<Byte> (keyboard.Read (kPb2Address) & kButtonBit), L"$C063 must read PB2 pressed");
        }


        TEST_METHOD (Apple2c_LeavesTheMouseLineAlone)
        {
            std::shared_mutex      lifetime;
            Apple2eSoftSwitchBank  bank (nullptr);
            Apple2eKeyboard        keyboard (nullptr);
            AppleMouse             mouse;
            GamePortTargets        targets;

            keyboard.SetMouse (&mouse);
            targets.iieSwitches = &bank;
            targets.iieKeyboard = &keyboard;

            MachineGamePortSink  sink (lifetime, [&targets] { return targets; });

            sink.TryApply (MakeState (127, 127, false, true, true), nullptr);

            Assert::IsTrue  (keyboard.IsClosedApplePressed(), L"PB1 is still Solid-Apple on the //c");
            Assert::IsFalse (keyboard.IsShiftPressed(),       L"PB2 must not be written where $C063 is the mouse button");
        }


        TEST_METHOD (IncrementalWrite_TouchesOnlyChangedLines)
        {
            std::shared_mutex    lifetime;
            AppleGamePort        port;
            GamePortTargets      targets;
            GamePortState        last = MakeState (127, 127, false, false, false);

            port.Reset();
            port.SetButton (1, true);   // set behind the sink's back
            targets.gamePort = &port;

            MachineGamePortSink  sink (lifetime, [&targets] { return targets; });

            sink.TryApply (MakeState (127, 127, true, false, false), &last);

            Assert::AreEqual (kButtonBit, static_cast<Byte> (port.Read (kPb0Address) & kButtonBit), L"the changed line must be written");
            Assert::AreEqual (kButtonBit, static_cast<Byte> (port.Read (kPb1Address) & kButtonBit),
                L"an unchanged line must not be rewritten from the target state");
        }


        TEST_METHOD (NoGamePort_AcceptsAndWritesNothing)
        {
            std::shared_mutex    lifetime;
            int                  lookups = 0;
            bool                 applied = false;

            MachineGamePortSink  sink (lifetime, [&lookups] { lookups++; return GamePortTargets(); });

            applied = sink.TryApply (MakeState (0, 255, true, true, true), nullptr);

            Assert::IsTrue   (applied, L"a machine with no game port must accept the state, not leave it pending forever");
            Assert::AreEqual (1, lookups, L"the devices must be looked up under the lock");
        }


        TEST_METHOD (RebuildHoldingTheMachine_RefusesTheWrite)
        {
            std::shared_mutex    lifetime;
            std::atomic<bool>    isLocked  {false};
            std::atomic<bool>    isRelease {false};
            int                  lookups   = 0;
            bool                 applied   = true;

            MachineGamePortSink  sink (lifetime, [&lookups] { lookups++; return GamePortTargets(); });

            std::thread  rebuild ([&]
            {
                std::unique_lock<std::shared_mutex>  exclusive (lifetime);

                isLocked = true;

                while (!isRelease)
                {
                    std::this_thread::yield();
                }
            });

            while (!isLocked)
            {
                std::this_thread::yield();
            }

            applied   = sink.TryApply (MakeState (0, 0, true, false, false), nullptr);
            isRelease = true;
            rebuild.join();

            Assert::IsFalse  (applied, L"a write during a rebuild must be refused so the mixer keeps it pending");
            Assert::AreEqual (0, lookups, L"no device may be touched while the rebuild holds the machine");
        }
    };
}
