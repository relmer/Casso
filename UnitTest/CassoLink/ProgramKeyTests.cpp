#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoLink/CassoLinkCodec.h"
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
