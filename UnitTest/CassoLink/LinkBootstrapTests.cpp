#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkConnector.h"
#include "Core/TextEncoding.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrapTests
//
//  The text typed into the Apple's monitor to start the link:
//  Ctrl-X, CALL -151, the serial loader as typed lines and the command that
//  runs it, the shim it reads, and the typing itself.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkBootstrapTests)
{
public:

    //  The serial loader typed into the monitor from $3950 and run; the shim
    //  is not typed.
    TEST_METHOD (Lines_TypeTheSerialLoaderIntoTheMonitor_AndRunIt)
    {
        std::vector<std::string>  lines;
        std::vector<Byte>         loader;
        std::vector<Byte>         typed;
        std::string               errors;
        Word                      start = LinkProgram::kSerialLoaderAddress;

        AssertSucceeded (LinkBootstrap::BuildLines (lines));
        AssertSucceeded (LinkProgram::AssembleSerialLoader (loader, errors));

        Assert::AreEqual (std::string ("\x18"),        lines[0], L"Ctrl-X cancels a partial line from stray bytes");
        Assert::AreEqual (std::string ("CALL -151\r"), lines[1]);
        Assert::AreEqual (std::string ("3950G\r"),     lines.back());

        for (size_t i = 2; i + 1 < lines.size(); i++)
        {
            const std::string  & line    = lines[i];
            Word                 address = (Word) std::stoul (line.substr (0, 4), nullptr, 16);

            Assert::AreEqual ((size_t) (address - start), typed.size(), L"each line follows the last");
            Assert::AreEqual ('\r', line.back());

            for (size_t column = 5; column + 3 <= line.size(); column += 3)
            {
                typed.push_back ((Byte) std::stoul (line.substr (column + 1, 2), nullptr, 16));
            }
        }

        Assert::IsTrue (typed == loader, L"the lines are the loader");
    }



    //  The page count, the shim padded to whole pages, and their XOR.
    TEST_METHOD (ShimTransfer_IsThePageCountThePagesAndTheirXor)
    {
        std::vector<Byte>  transfer;
        std::vector<Byte>  shim;
        Byte               sum = 0;

        AssertSucceeded (LinkBootstrap::BuildShimTransfer (9600, transfer));
        AssertSucceeded (LinkProgram::BuildShim (9600, shim));

        Assert::AreEqual ((size_t) transfer[0] * 256 + 2, transfer.size());
        Assert::IsTrue   (std::equal (shim.begin(), shim.end(), transfer.begin() + 1), L"the shim, at the bootstrap speed");

        for (size_t i = 1; i + 1 < transfer.size(); i++)
        {
            sum ^= transfer[i];
        }

        Assert::AreEqual (sum, transfer.back());
    }


    //  The cassette record: the leader, the two sync half-cycles, two equal
    //  half-cycles per bit, the checksum's eight bits and one closing half.
    TEST_METHOD (Cassette_IsTheFinderAndTheShimAsOneRecord_ReadByItsMonitorCommand)
    {
        std::vector<Byte>    shim;
        std::vector<double>  halves;
        TapeAudio            audio;
        std::string          command;
        LinkShimOptions      options;
        size_t               leader = 0;
        Byte                 check  = LinkAudioBootstrap::kChecksumSeed;
        Byte                 last   = 0;

        Word                 start  = 0;

        AssertSucceeded (LinkProgram::BuildBootstrapImage (options, shim, start));
        AssertSucceeded (LinkAudioBootstrap::BuildAudio (options, audio, command));
        Assert::AreEqual (std::string ("39A0.407ER 39A0G\r"), command, L"the finder and the shim, to the shim's end");
        Assert::AreEqual (LinkAudioBootstrap::kSampleRate, audio.sampleRate);

        LinkAudioBootstrap::AppendRecord (shim, halves);

        while (leader < halves.size() && halves[leader] == LinkAudioBootstrap::kLeaderHalfUs)
        {
            leader++;
        }

        Assert::IsTrue (leader * LinkAudioBootstrap::kLeaderHalfUs >= 3.5e6, L"the leader covers the monitor's wait");
        Assert::AreEqual (leader + 2 + (shim.size() + 1) * 16 + 1, halves.size());
        Assert::AreEqual (LinkAudioBootstrap::kSyncFirstUs,  halves[leader]);
        Assert::AreEqual (LinkAudioBootstrap::kSyncSecondUs, halves[leader + 1]);

        for (Byte value : shim)
        {
            check ^= value;
        }

        for (size_t bit = 0; bit < 8; bit++)
        {
            last = (Byte) ((last << 1) | (halves[leader + 2 + shim.size() * 16 + bit * 2] == LinkAudioBootstrap::kOneHalfUs ? 1 : 0));
        }

        Assert::AreEqual (check, last, L"the checksum, seeded with $FF");
    }



    TEST_METHOD (Send_TypesEachLine_AndStopsWhenCanceled)
    {
        std::shared_ptr<InProcessByteStream>  pc;
        std::shared_ptr<InProcessByteStream>  apple;
        std::vector<std::string>              lines = { "ONE\r", "TWO\r", "THREE\r" };
        Byte                                  buffer[64];
        size_t                                got   = 0;
        int                                   calls = 0;

        InProcessByteStream::CreateVirtualPair (pc, apple);

        AssertSucceeded (LinkBootstrap::Send (*pc, lines, 0));
        AssertSucceeded (apple->Read (buffer, sizeof (buffer), 100, got));
        Assert::AreEqual (std::string ("ONE\rTWO\rTHREE\r"), std::string (buffer, buffer + got));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), LinkBootstrap::Send (*pc, lines, 0, [&calls]() { return ++calls > 1; }));
        AssertSucceeded (apple->Read (buffer, sizeof (buffer), 100, got));
        Assert::AreEqual (std::string ("ONE\r"), std::string (buffer, buffer + got), L"one line, then canceled");
    }


    //  Each line of the bootstrap and of the prompt probe fits after its
    //  prompt on a 40-column row, carriage return excluded, so the Apple's
    //  screen doesn't scroll while the line's characters arrive.
    TEST_METHOD (TypedLines_FitAfterThePromptOnA40ColumnRow)
    {
        std::vector<std::string>  lines;

        AssertSucceeded (LinkBootstrap::BuildLines (lines));
        lines.insert (lines.end(), std::begin (CassoLinkConnector::kPromptProbe), std::end (CassoLinkConnector::kPromptProbe));

        for (const std::string & line : lines)
        {
            size_t  shown = line.ends_with ('\r') ? line.size() - 1 : line.size();

            Assert::IsTrue (1 + shown < 40, TextEncoding::NarrowToWide (line).c_str());
        }
    }


    //  The original Apple ]['s Integer BASIC fails a line with a number past
    //  32767 with *** >32767 ERR, so every number in the prompt probe is in
    //  -32767 to 32767. Run with Integer BASIC's 16-bit arithmetic, the
    //  probe's store is to slot n's 6551 data register, $C088 + n * 16, for
    //  KSW's high byte $Cn, and DOS's copy of KSW is read from $AA56.
    TEST_METHOD (ThePromptProbe_RunsInIntegerBasic_AndStoresToTheSlotsDataRegister)
    {
        static const std::regex  kNumber ("-?[0-9]+");

        std::vector<int>  numbers;



        for (const char * line : CassoLinkConnector::kPromptProbe)
        {
            std::string  text (line);

            for (auto it = std::sregex_iterator (text.begin(), text.end(), kNumber); it != std::sregex_iterator(); ++it)
            {
                int  value = std::stoi (it->str());

                Assert::IsTrue (value >= -32767 && value <= 32767, TextEncoding::NarrowToWide (text).c_str());

                numbers.push_back (value);
            }
        }

        // S=PEEK(57), IF S=158 THEN S=PEEK(a), S=S*16+b, IF ABS(S+c)<49 THEN POKE S,165.
        Assert::AreEqual<size_t> (8, numbers.size());
        Assert::AreEqual (0xAA56, (int) (uint16_t) numbers[2], L"DOS's copy of KSW's high byte");

        for (int slot = 1; slot <= 7; slot++)
        {
            int  s = (0xC0 + slot) * numbers[3] + numbers[4];

            Assert::IsTrue (s >= -32767 && s <= 32767, L"the address is an Integer BASIC number");
            Assert::AreEqual (0xC088 + slot * 16, (int) (uint16_t) s, std::format (L"slot {}'s data register", slot).c_str());
            Assert::IsTrue (std::abs (s + numbers[5]) < numbers[6], std::format (L"slot {} is stored to", slot).c_str());
        }

        // Keyboard input, KSW at $FD1B, is skipped.
        Assert::IsFalse (std::abs (0xFD * numbers[3] + numbers[4] + numbers[5]) < numbers[6], L"no store without IN#");
    }
};
