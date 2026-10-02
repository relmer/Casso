#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Machines/Apple2/Common/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputVideoStateTests
//
//  Save and load of the video timing, the speaker, the keyboards, the game
//  port, the //c mouse and the Joyport. Each round trip fills every saved
//  field of a source and a target from seeds whose bits are complements, so
//  every field differs; a field either side skips keeps the target's value
//  and fails the compare. The run-after-load tests save, run the device,
//  load, run it again the same way, and expect the same observable sequence.
//
////////////////////////////////////////////////////////////////////////////////

namespace InputVideoState
{
    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);

    // Complementary seeds: every bit differs, so every bool derived from a bit
    // differs too.
    static constexpr Byte     kSeed        = 0x5A;
    static constexpr Byte     kTargetSeed  = 0xA5;


    static bool Bit (Byte seed, int index)
    {
        return ((seed >> (index % CHAR_BIT)) & 1) != 0;
    }


    static std::vector<Byte> Save (const IMachineState & part)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = part.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    // Loads a blob into a part; on success the part must have consumed it all.
    static HRESULT LoadFrom (IMachineState & target, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);
        HRESULT      hr = S_OK;



        hr = target.LoadState (reader);

        if (SUCCEEDED (hr))
        {
            Assert::IsTrue (reader.IsAtEnd());
        }

        return hr;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  KeyboardProbe
    //
    //  The base keyboard with its state opened up.
    //
    ////////////////////////////////////////////////////////////////////////////

    class KeyboardProbe : public AppleKeyboard
    {
    public:
        void Fill (Byte seed)
        {
            m_latchedKey.store (static_cast<Byte> (seed + 1), memory_order_release);
            m_anyKeyDown.store (Bit (seed, 0),                memory_order_release);
            m_repeatKey.store  (static_cast<Byte> (seed + 2), memory_order_release);

            m_repeatAccumUs = 1000u + seed;
            m_repeatStarted = Bit (seed, 1);
            m_lastRepeatKey = static_cast<Byte> (seed + 3);
        }

        void AssertSameState (const KeyboardProbe & other) const
        {
            Assert::AreEqual (m_latchedKey.load(), other.m_latchedKey.load(), L"latched key");
            Assert::AreEqual (m_anyKeyDown.load(), other.m_anyKeyDown.load(), L"any key down");
            Assert::AreEqual (m_repeatKey.load(),  other.m_repeatKey.load(),  L"repeat key");
            Assert::AreEqual (m_repeatAccumUs,     other.m_repeatAccumUs,     L"repeat accumulator");
            Assert::AreEqual (m_repeatStarted,     other.m_repeatStarted,     L"repeat started");
            Assert::AreEqual (m_lastRepeatKey,     other.m_lastRepeatKey,     L"last repeat key");
        }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Apple2eKeyboardProbe
    //
    //  The //e keyboard with its own and its base's state opened up.
    //
    ////////////////////////////////////////////////////////////////////////////

    class Apple2eKeyboardProbe : public Apple2eKeyboard
    {
    public:
        void Fill (Byte seed)
        {
            m_latchedKey.store (static_cast<Byte> (seed + 1), memory_order_release);
            m_anyKeyDown.store (Bit (seed, 0),                memory_order_release);
            m_repeatKey.store  (static_cast<Byte> (seed + 2), memory_order_release);

            m_repeatAccumUs = 1000u + seed;
            m_repeatStarted = Bit (seed, 1);
            m_lastRepeatKey = static_cast<Byte> (seed + 3);

            m_openApple.store            (Bit (seed, 2),   memory_order_release);
            m_closedApple.store          (Bit (seed, 3),   memory_order_release);
            m_holdOpenApple.store        (Bit (seed, 4),   memory_order_release);
            m_holdClosedApple.store      (Bit (seed, 5),   memory_order_release);
            m_resetHoldCycles.store      (seed * 1000u,    memory_order_release);
            m_shift.store                (Bit (seed, 6),   memory_order_release);
            m_eightyColSwitchIn.store    (Bit (seed, 7),   memory_order_release);
            m_keyboardSwitchDvorak.store (Bit (seed, 8),   memory_order_release);
        }

        void AssertSameState (const Apple2eKeyboardProbe & other) const
        {
            Assert::AreEqual (m_latchedKey.load(),           other.m_latchedKey.load(),           L"latched key");
            Assert::AreEqual (m_anyKeyDown.load(),           other.m_anyKeyDown.load(),           L"any key down");
            Assert::AreEqual (m_repeatKey.load(),            other.m_repeatKey.load(),            L"repeat key");
            Assert::AreEqual (m_repeatAccumUs,               other.m_repeatAccumUs,               L"repeat accumulator");
            Assert::AreEqual (m_repeatStarted,               other.m_repeatStarted,               L"repeat started");
            Assert::AreEqual (m_lastRepeatKey,               other.m_lastRepeatKey,               L"last repeat key");
            Assert::AreEqual (m_openApple.load(),            other.m_openApple.load(),            L"Open Apple");
            Assert::AreEqual (m_closedApple.load(),          other.m_closedApple.load(),          L"Closed Apple");
            Assert::AreEqual (m_holdOpenApple.load(),        other.m_holdOpenApple.load(),        L"held Open Apple");
            Assert::AreEqual (m_holdClosedApple.load(),      other.m_holdClosedApple.load(),      L"held Closed Apple");
            Assert::AreEqual (m_resetHoldCycles.load(),      other.m_resetHoldCycles.load(),      L"reset hold cycles");
            Assert::AreEqual (m_shift.load(),                other.m_shift.load(),                L"Shift");
            Assert::AreEqual (m_eightyColSwitchIn.load(),    other.m_eightyColSwitchIn.load(),    L"80/40 switch");
            Assert::AreEqual (m_keyboardSwitchDvorak.load(), other.m_keyboardSwitchDvorak.load(), L"keyboard switch");
        }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  GamePortProbe
    //
    ////////////////////////////////////////////////////////////////////////////

    class GamePortProbe : public AppleGamePort
    {
    public:
        void Fill (Byte seed)
        {
            int  i = 0;



            m_paddleTriggerCycle = 0x0000ABCD00000000ull + seed;

            for (i = 0; i < s_knButtonCount; i++)
            {
                m_buttonState[i].store (Bit (seed, i), memory_order_release);
            }

            for (i = 0; i < s_knPaddleAxisCount; i++)
            {
                m_paddlePosition[i].store (static_cast<Byte> (seed + i * 17), memory_order_release);
            }
        }

        void AssertSameState (const GamePortProbe & other) const
        {
            int  i = 0;



            Assert::AreEqual (m_paddleTriggerCycle, other.m_paddleTriggerCycle, L"paddle trigger cycle");

            for (i = 0; i < s_knButtonCount; i++)
            {
                Assert::AreEqual (m_buttonState[i].load(), other.m_buttonState[i].load(), L"button");
            }

            for (i = 0; i < s_knPaddleAxisCount; i++)
            {
                Assert::AreEqual (m_paddlePosition[i].load(), other.m_paddlePosition[i].load(), L"paddle");
            }
        }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MouseProbe
    //
    ////////////////////////////////////////////////////////////////////////////

    class MouseProbe : public AppleMouse
    {
    public:
        void Fill (Byte seed)
        {
            m_hostDx.store     (seed - 300,                      std::memory_order_release);
            m_hostDy.store     (seed + 300,                      std::memory_order_release);
            m_hostButton.store (Bit (seed, 0),                   std::memory_order_release);
            m_hostTarget.store (0x12340000u + seed,              std::memory_order_release);
            m_hasTarget.store  (Bit (seed, 1),                   std::memory_order_release);

            m_retargetCountdown = 100u + seed;
            m_pendingX          = seed - 128;
            m_pendingY          = 128 - seed;
            m_xInt              = Bit (seed, 2);
            m_yInt              = Bit (seed, 3);
            m_vblInt            = Bit (seed, 4);
            m_mouX1             = static_cast<Byte> (seed + 1);
            m_mouY1             = static_cast<Byte> (seed + 2);
            m_xyEnabled         = Bit (seed, 5);
            m_vblEnabled        = Bit (seed, 6);
            m_x0EdgeFalling     = Bit (seed, 7);
            m_y0EdgeFalling     = Bit (seed, 8);
            m_iouAccessEnabled  = Bit (seed, 9);
            m_lastInVblank      = Bit (seed, 10);
            m_sampleAccum       = 3u + seed;
        }

        void AssertSameState (const MouseProbe & other) const
        {
            Assert::AreEqual (m_hostDx.load(),      other.m_hostDx.load(),      L"host dx");
            Assert::AreEqual (m_hostDy.load(),      other.m_hostDy.load(),      L"host dy");
            Assert::AreEqual (m_hostButton.load(),  other.m_hostButton.load(),  L"host button");
            Assert::AreEqual (m_hostTarget.load(),  other.m_hostTarget.load(),  L"host target");
            Assert::AreEqual (m_hasTarget.load(),   other.m_hasTarget.load(),   L"has target");
            Assert::AreEqual (m_retargetCountdown,  other.m_retargetCountdown,  L"retarget countdown");
            Assert::AreEqual (m_pendingX,           other.m_pendingX,           L"pending x");
            Assert::AreEqual (m_pendingY,           other.m_pendingY,           L"pending y");
            Assert::AreEqual (m_xInt,               other.m_xInt,               L"x interrupt");
            Assert::AreEqual (m_yInt,               other.m_yInt,               L"y interrupt");
            Assert::AreEqual (m_vblInt,             other.m_vblInt,             L"VBL interrupt");
            Assert::AreEqual (m_mouX1,              other.m_mouX1,              L"MOUX1");
            Assert::AreEqual (m_mouY1,              other.m_mouY1,              L"MOUY1");
            Assert::AreEqual (m_xyEnabled,          other.m_xyEnabled,          L"XY enabled");
            Assert::AreEqual (m_vblEnabled,         other.m_vblEnabled,         L"VBL enabled");
            Assert::AreEqual (m_x0EdgeFalling,      other.m_x0EdgeFalling,      L"X0 edge");
            Assert::AreEqual (m_y0EdgeFalling,      other.m_y0EdgeFalling,      L"Y0 edge");
            Assert::AreEqual (m_iouAccessEnabled,   other.m_iouAccessEnabled,   L"IOU access");
            Assert::AreEqual (m_lastInVblank,       other.m_lastInVblank,       L"last in VBL");
            Assert::AreEqual (m_sampleAccum,        other.m_sampleAccum,        L"sample accumulator");
        }

        void SetPendingX (int pending) { m_pendingX = pending; }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  JoyportProbe
    //
    ////////////////////////////////////////////////////////////////////////////

    class JoyportProbe : public SiriusJoyport
    {
    public:
        void Fill (Byte seed)
        {
            constexpr unsigned long  kMask = (1ul << static_cast<unsigned long> (JoystickSwitch::Count)) - 1;

            size_t  jack = 0;



            for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
            {
                m_jacks[jack].store ((seed >> jack) & kMask, memory_order_release);
            }

            m_resetCycle    = 0x0000001200000000ull + seed;
            m_hasResetStamp = Bit (seed, 0);
        }

        void AssertSameState (const JoyportProbe & other) const
        {
            size_t  jack = 0;



            for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
            {
                Assert::AreEqual (m_jacks[jack].load(), other.m_jacks[jack].load(), L"jack switches");
            }

            Assert::AreEqual (m_resetCycle,    other.m_resetCycle,    L"reset cycle");
            Assert::AreEqual (m_hasResetStamp, other.m_hasResetStamp, L"has reset stamp");
        }
    };


    // Fills a source and a target from different seeds, round-trips the
    // source into the target, and compares every field. Each test runs it
    // both ways round, so a skipped field cannot match by chance when the
    // load leaves it at a default that equals one seed's value.
    template <typename Probe>
    static void RoundTrip (Byte sourceSeed, Byte targetSeed)
    {
        Probe    source;
        Probe    target;
        HRESULT  hr = S_OK;



        source.Fill (sourceSeed);
        target.Fill (targetSeed);

        hr = LoadFrom (target, Save (source));
        Assert::AreEqual (S_OK, hr);

        source.AssertSameState (target);
    }


    TEST_CLASS (InputVideoStateTests)
    {
    public:
        TEST_METHOD (VideoTimingRoundTripsTheCycleInFrame)
        {
            constexpr uint32_t  kSourceCycle = 12'345;
            constexpr uint32_t  kTargetCycle = 77;
            VideoTiming         source;
            VideoTiming         target;
            HRESULT             hr = S_OK;



            source.Tick (kSourceCycle);
            target.Tick (kTargetCycle);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (kSourceCycle, target.GetCycleInFrame());
        }


        TEST_METHOD (VideoTimingRunsTheSameAfterLoad)
        {
            constexpr uint32_t  kStep  = 7;
            constexpr int       kSteps = 3000;
            VideoTiming         timing;
            std::vector<Byte>   bytes;
            std::vector<bool>   first;
            std::vector<bool>   second;
            int                 i      = 0;
            HRESULT             hr     = S_OK;



            timing.Tick (VideoTiming::kVblankStartCycle - 1000);
            bytes = Save (timing);

            for (i = 0; i < kSteps; i++)
            {
                timing.Tick (kStep);
                first.push_back (timing.IsInVblank());
            }

            hr = LoadFrom (timing, bytes);
            Assert::AreEqual (S_OK, hr);

            for (i = 0; i < kSteps; i++)
            {
                timing.Tick (kStep);
                second.push_back (timing.IsInVblank());
            }

            Assert::IsTrue (first == second);
        }


        TEST_METHOD (SpeakerRoundTripsItsLevelBothWays)
        {
            AppleSpeaker  high;
            AppleSpeaker  low;
            AppleSpeaker  target;
            HRESULT       hr = S_OK;



            high.Read (0xC030);

            hr = LoadFrom (target, Save (high));
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (high.GetSpeakerState(), target.GetSpeakerState());

            hr = LoadFrom (target, Save (low));
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (low.GetSpeakerState(), target.GetSpeakerState());
        }


        TEST_METHOD (KeyboardRoundTripsEveryField)
        {
            RoundTrip<KeyboardProbe> (kSeed,       kTargetSeed);
            RoundTrip<KeyboardProbe> (kTargetSeed, kSeed);
        }


        TEST_METHOD (KeyboardRepeatsTheSameAfterLoad)
        {
            constexpr uint32_t  kStepUs = 10'000;
            constexpr int       kSteps  = 200;
            AppleKeyboard       keyboard;
            std::vector<Byte>   bytes;
            std::vector<Byte>   first;
            std::vector<Byte>   second;
            int                 i       = 0;
            HRESULT             hr      = S_OK;



            keyboard.PressKey       ('A');
            keyboard.SetKeyDown     (true);
            keyboard.BeginKeyRepeat ('A');
            keyboard.TickAutoRepeat (kStepUs);
            bytes = Save (keyboard);

            for (i = 0; i < kSteps; i++)
            {
                keyboard.TickAutoRepeat (kStepUs);
                first.push_back (keyboard.Read (0xC000));
                keyboard.Read (0xC010);
            }

            hr = LoadFrom (keyboard, bytes);
            Assert::AreEqual (S_OK, hr);

            for (i = 0; i < kSteps; i++)
            {
                keyboard.TickAutoRepeat (kStepUs);
                second.push_back (keyboard.Read (0xC000));
                keyboard.Read (0xC010);
            }

            Assert::IsTrue (first == second);
        }


        TEST_METHOD (Apple2eKeyboardRoundTripsEveryField)
        {
            RoundTrip<Apple2eKeyboardProbe> (kSeed,       kTargetSeed);
            RoundTrip<Apple2eKeyboardProbe> (kTargetSeed, kSeed);
        }


        TEST_METHOD (Apple2eKeyboardBlobIsNotABaseKeyboardBlob)
        {
            Apple2eKeyboardProbe  source;
            KeyboardProbe         target;
            HRESULT               hr = S_OK;



            source.Fill (kSeed);

            hr = LoadFrom (target, Save (source));
            Assert::IsTrue (FAILED (hr));
        }


        TEST_METHOD (GamePortRoundTripsEveryField)
        {
            RoundTrip<GamePortProbe> (kSeed,       kTargetSeed);
            RoundTrip<GamePortProbe> (kTargetSeed, kSeed);
        }


        TEST_METHOD (GamePortPaddleTimesTheSameAfterLoad)
        {
            constexpr int       kSteps = 300;
            constexpr uint64_t  kStep  = 11;
            AppleGamePort       port;
            uint64_t            cycles = 5000;
            std::vector<Byte>   bytes;
            std::vector<Byte>   first;
            std::vector<Byte>   second;
            int                 i      = 0;
            HRESULT             hr     = S_OK;



            port.SetCpuCycleSource (&cycles);
            port.SetPaddle (0, 200);
            port.Read (0xC070);
            cycles += 100;
            bytes = Save (port);

            for (i = 0; i < kSteps; i++)
            {
                cycles += kStep;
                first.push_back (port.Read (0xC064));
            }

            hr = LoadFrom (port, bytes);
            Assert::AreEqual (S_OK, hr);

            cycles -= kStep * kSteps;

            for (i = 0; i < kSteps; i++)
            {
                cycles += kStep;
                second.push_back (port.Read (0xC064));
            }

            Assert::IsTrue (first == second);
        }


        TEST_METHOD (MouseRoundTripsEveryField)
        {
            RoundTrip<MouseProbe> (kSeed,       kTargetSeed);
            RoundTrip<MouseProbe> (kTargetSeed, kSeed);
        }


        TEST_METHOD (MouseRejectsAnOversizedMovementQueue)
        {
            MouseProbe  source;
            MouseProbe  target;
            HRESULT     hr = S_OK;



            source.SetPendingX (5000);

            hr = LoadFrom (target, Save (source));
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (MouseMovesTheSameAfterLoad)
        {
            constexpr int       kSteps = 400;
            constexpr uint32_t  kStep  = 50;
            AppleMouse          mouse;
            std::vector<Byte>   bytes;
            std::vector<Byte>   first;
            std::vector<Byte>   second;
            int                 i      = 0;
            HRESULT             hr     = S_OK;



            mouse.MoveBy (40, -25);
            mouse.Tick   (AppleMouse::kSampleQuantum);
            bytes = Save (mouse);

            for (i = 0; i < kSteps; i++)
            {
                mouse.Tick (kStep);
                first.push_back (static_cast<Byte> (mouse.ReadXInterruptStatus() | (mouse.ReadMouX1() >> 1) |
                                                    (mouse.ReadYInterruptStatus() >> 2) | (mouse.ReadMouY1() >> 3)));
                mouse.AccessRstXY();
            }

            hr = LoadFrom (mouse, bytes);
            Assert::AreEqual (S_OK, hr);

            for (i = 0; i < kSteps; i++)
            {
                mouse.Tick (kStep);
                second.push_back (static_cast<Byte> (mouse.ReadXInterruptStatus() | (mouse.ReadMouX1() >> 1) |
                                                     (mouse.ReadYInterruptStatus() >> 2) | (mouse.ReadMouY1() >> 3)));
                mouse.AccessRstXY();
            }

            Assert::IsTrue (first == second);
        }


        TEST_METHOD (JoyportRoundTripsEveryField)
        {
            RoundTrip<JoyportProbe> (kSeed,       kTargetSeed);
            RoundTrip<JoyportProbe> (kTargetSeed, kSeed);
        }
    };
}
