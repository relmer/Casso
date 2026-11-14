#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "Core/TextEncoding.h"
#include "Devices/RamDevice.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramKeyTests
//
//  The keys the program sends the host: F on the standard screen, and Q or
//  Space at a Full Send's pause or during a read. The program only reports
//  each key; the host starts and ends the Full Send, which the FakeApple
//  session tests cover. The program runs on an emulated //c, and the host
//  decodes its packets.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ProgramKeyTests)
{
public:

    static constexpr uint64_t  kClockHz     = 1'020'484;
    static constexpr uint64_t  kPressAfter  = 200'000;    // cycles into a request before a key is pressed
    static constexpr uint64_t  kQuitWithin  = 2 * kClockHz;
    static constexpr Byte      kFastControl = 0x10;       // 115,200 baud
    static constexpr Byte      kTracks      = 35;

    static std::vector<Byte>  Dos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }

    //  `key` pressed once, `after` cycles from now in the stream's count.
    static void  PressDuring (ProgramRig & rig, Byte key, uint64_t after, uint64_t & outPressedAt)
    {
        uint64_t  at = rig.stream->GetCycles() + after;



        outPressedAt = 0;

        rig.stream->SetStepHook ([&rig, key, at, &outPressedAt] ()
        {
            if (outPressedAt == 0 && rig.stream->GetCycles() >= at)
            {
                outPressedAt = rig.stream->GetCycles();
                rig.Press (key);
            }
        });
    }



    TEST_METHOD (F_OnTheStandardScreen_IsAFullSendRequest)
    {
        ProgramRig  rig (Dos33());



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->SetColumns (false));
        Assert::IsFalse (rig.host->TakeFullSendRequest(), L"no request before F");

        rig.Press ('F');
        AssertSucceeded (rig.host->SetColumns (false));

        Assert::IsTrue (rig.host->TakeFullSendRequest(), L"F is reported");
    }



    TEST_METHOD (QAndSpace_AtAPause_AreTheKeysWaitedFor)
    {
        static const std::vector<std::string>  kLines = { "PRESS SPACE OR Q" };

        ProgramRig  rig (Dos33());
        Byte        key       = 0;
        uint64_t    pressedAt = 0;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        for (Byte pressed : { (Byte) ' ', (Byte) 'Q' })
        {
            PressDuring (rig, pressed, kPressAfter, pressedAt);
            AssertSucceeded (rig.host->WaitForKey (kLines, key));

            Assert::IsTrue (pressedAt != 0, L"the key was pressed during the pause");
            Assert::AreEqual<int> (pressed == 'Q' ? CassoLinkCodec::kKeyQuit : CassoLinkCodec::kKeySpace, key);
        }
    }



    //  A line typed on the Apple, such as a Full Send's prefix, is edited on
    //  the PC: each key is sent as its code, Left arrow and Return too.
    TEST_METHOD (EveryKey_AtALinesPrompt_IsSentAsItsCode)
    {
        static const std::vector<std::string>  kLines = { "PREFIX: _" };

        struct Key { Byte pressed; Byte code; };

        static constexpr Key  kKeys[] = { { 'B', 'B' | 0x80 }, { 'x', 'x' | 0x80 }, { '-', '-' | 0x80 },
                                          { 0x08, CassoLinkCodec::kKeyLeft }, { '\r', CassoLinkCodec::kKeyReturn } };

        ProgramRig  rig (Dos33());
        Byte        key       = 0;
        uint64_t    pressedAt = 0;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        for (const Key & k : kKeys)
        {
            PressDuring (rig, k.pressed, kPressAfter, pressedAt);
            AssertSucceeded (rig.host->WaitForAnyKey (kLines, key));

            Assert::AreEqual<int> (k.code, key, std::format (L"the key ${:02X}", k.pressed).c_str());
        }
    }



    //  No drive spins during a pause, which can last as long as the user
    //  takes to change disks.
    TEST_METHOD (APause_TurnsTheMotorOff)
    {
        static const std::vector<std::string>  kLines = { "PRESS SPACE" };

        static constexpr uint64_t  kCheckFrom = 3 * kClockHz / 2;   // past the drive's spindown
        static constexpr uint64_t  kPressAt   = 3 * kClockHz;

        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        Byte               key      = 0;
        uint64_t           start    = 0;
        uint64_t           checked  = 0;
        bool               pressed  = false;
        bool               spun     = false;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->ReadSectors (1, 0, sector, data, status));

        start = rig.stream->GetCycles();

        rig.stream->SetStepHook ([&] ()
        {
            uint64_t  now = rig.stream->GetCycles() - start;

            if (!pressed && now >= kCheckFrom)
            {
                spun = spun || rig.machine.GetRefs().diskController->IsMotorOn();
                checked++;
            }

            if (!pressed && now >= kPressAt)
            {
                pressed = true;
                rig.Press (' ');
            }
        });

        AssertSucceeded (rig.host->WaitForKey (kLines, key));

        Assert::IsTrue  (pressed && checked > 0, L"the pause lasted past the check");
        Assert::IsFalse (spun, L"no drive spins during the pause");
    }



    //  The text page as its characters, each row in 40 columns and then in
    //  80, where each auxiliary memory byte is ahead of the main memory byte
    //  at the same offset.
    static std::string  ReadScreen (ProgramRig & rig)
    {
        const Byte   * mainRam = rig.machine.GetRefs().mainRamDev->GetData();
        const Byte   * auxRam  = rig.machine.GetMmu()->GetAuxBuffer();
        std::string    narrow;
        std::string    wide;

        for (int row = 0; row < 24; row++)
        {
            Word  base = (Word) (0x0400 + (row % 8) * 0x80 + (row / 8) * 0x28);

            for (int i = 0; i < 40; i++)
            {
                narrow += (char) (mainRam[base + i] & 0x7F);
                wide   += (char) (auxRam[base + i] & 0x7F);
                wide   += (char) (mainRam[base + i] & 0x7F);
            }

            narrow += '\n';
            wide   += '\n';
        }

        return narrow + wide;
    }



    //  The row for an image being booted is drawn on the Apple, and the
    //  tones for a boot result sound: each takes the cycles of its notes
    //  longer than a screen with no tone, the buzz longer than the beeps.
    TEST_METHOD (ASessionScreen_DrawsTheValidatingRow_AndSoundsTheBootTones)
    {
        static constexpr Byte                  kBooted   = 5;
        static constexpr Byte                  kNotBoot  = 6;
        static constexpr uint64_t              kBeeps    = 100'000;     // four rising notes are about 124,000 cycles
        static constexpr uint64_t              kBuzz     = 600'000;     // two low ones about 720,000

        ProgramRig                rig (Dos33());
        FullSendScreen            session;
        std::vector<std::string>  kRows;
        std::string  wanted = "D1: VALIDATING IMAGE ON PC, PLEASE WAIT";
        std::string  screen;
        uint64_t     start  = 0;
        uint64_t     quiet  = 0;
        uint64_t     beeps  = 0;
        uint64_t     buzz   = 0;



        session.drives[0] = FullSendDriveState::Validating;
        kRows             = session.GetRows();

        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        //  A screen is sent without a wait for the Apple, and the request
        //  after it waits until the Apple has drawn it and sounded its tone.
        //  So each screen's cycles are those of the request after it.
        AssertSucceeded (rig.host->SendSessionScreen (true, 0, kRows));

        start = rig.stream->GetCycles();
        AssertSucceeded (rig.host->SendSessionScreen (true, kBooted, kRows));
        quiet = rig.stream->GetCycles() - start;

        screen = ReadScreen (rig);

        Assert::IsTrue (screen.find (wanted) != std::string::npos, L"The row is on the Apple's screen");

        start = rig.stream->GetCycles();
        AssertSucceeded (rig.host->SendSessionScreen (true, kNotBoot, kRows));
        beeps = rig.stream->GetCycles() - start;

        start = rig.stream->GetCycles();
        AssertSucceeded (rig.host->SendSessionScreen (true, 0, kRows));
        buzz = rig.stream->GetCycles() - start;

        Assert::IsTrue (beeps > quiet + kBeeps, std::format (L"The beeps took {} cycles over a screen's {}", beeps, quiet).c_str());
        Assert::IsTrue (buzz > quiet + kBuzz, std::format (L"The buzz took {} cycles over a screen's {}", buzz, quiet).c_str());
    }



    //  The pause after an image that did not boot: each line fits a
    //  40-column row and is drawn while the Apple waits for Space.
    TEST_METHOD (ThePauseAfterAFailedBoot_IsDrawnOnTheApple)
    {
        static const std::vector<std::string>  kLines = { "0024 DID NOT BOOT. CAPTURE IT AGAIN", "OR PUT IN A NEW DISK, THEN PRESS SPACE.", "0025 STILL DOES NOT BOOT." };

        ProgramRig   rig (Dos33());
        std::string  screen;
        Byte         key       = 0;
        uint64_t     pressedAt = 0;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        //  The screen is read while the Apple waits, then Space is pressed.
        pressedAt = rig.stream->GetCycles() + kPressAfter;

        rig.stream->SetStepHook ([&] ()
        {
            if (screen.empty() && rig.stream->GetCycles() >= pressedAt)
            {
                screen = ReadScreen (rig);
                rig.Press (' ');
            }
        });

        AssertSucceeded (rig.host->WaitForKey (kLines, key));

        for (const std::string & line : kLines)
        {
            Assert::IsTrue (line.size() <= 40, L"A line fits a row");
            Assert::IsTrue (screen.find (line) != std::string::npos, TextEncoding::NarrowToWide (line).c_str());
        }

        Assert::AreEqual<int> (CassoLinkCodec::kKeySpace, key);
    }



    //  During a Full Send, set by its Session screen, the program reads the
    //  keyboard during a read, so Q ends a transfer within two seconds.
    TEST_METHOD (Q_DuringAFullSendsRead_IsReportedWithinTwoSeconds)
    {
        static const std::vector<std::string>  kRows = { "FULL SEND" };

        ProgramRig         rig (Dos33());
        std::vector<Byte>  sectors (16);
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        uint64_t           pressAt   = 0;
        uint64_t           pressedAt = 0;
        uint64_t           notedAt   = 0;



        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->SendSessionScreen (true, 0, kRows));

        pressAt = rig.stream->GetCycles() + kPressAfter;

        //  Q pressed once, then the host's note of it looked for at each step,
        //  so the time is the packet's arrival rather than the read's end.
        rig.stream->SetStepHook ([&] ()
        {
            if (pressedAt == 0 && rig.stream->GetCycles() >= pressAt)
            {
                pressedAt = rig.stream->GetCycles();
                rig.Press ('Q');
            }
            else if (pressedAt != 0 && notedAt == 0 && rig.host->TakeQuit())
            {
                notedAt = rig.stream->GetCycles();
            }
        });

        for (Byte track = 0; track < kTracks && notedAt == 0; track++)
        {
            AssertSucceeded (rig.host->ReadSectors (1, track, sectors, data, status));
        }

        Assert::IsTrue (pressedAt != 0, L"Q was pressed during a read");
        Assert::IsTrue (notedAt != 0, L"Q is reported");
        Assert::IsTrue (notedAt - pressedAt < kQuitWithin, std::format (L"Q reported {} cycles after it was pressed", notedAt - pressedAt).c_str());
    }
};
