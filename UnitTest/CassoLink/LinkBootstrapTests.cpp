#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrapTests
//
//  The text typed into the Apple's monitor to start the link:
//  Ctrl-X, CALL -151, the shim as lines of eight bytes from its address, and the
//  command that runs it; and the typing itself.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkBootstrapTests)
{
public:

    //  The slot finder at $3AA0, then the shim from $3B00, with the zeros
    //  between them not typed, and the finder run.
    TEST_METHOD (Lines_TypeTheFinderAndTheShimIntoTheMonitor_AndRunTheFinder)
    {
        std::vector<std::string>  lines;
        std::vector<Byte>         image;
        std::vector<Byte>         typed;
        Word                      start = 0;
        Word                      next  = LinkProgram::kSlotFinderAddress;
        LinkShimOptions           options;

        options.baud = 9600;

        AssertSucceeded (LinkBootstrap::BuildLines (9600, lines));
        AssertSucceeded (LinkProgram::BuildBootstrapImage (options, image, start));

        Assert::AreEqual (LinkProgram::kSlotFinderAddress, start);
        Assert::AreEqual (std::string ("\x18"),        lines[0], L"Ctrl-X cancels a partial line from stray bytes");
        Assert::AreEqual (std::string ("CALL -151\r"), lines[1]);
        Assert::AreEqual (std::string ("3AA0G\r"),     lines.back());

        typed.assign (image.size(), 0);

        for (size_t i = 2; i + 1 < lines.size(); i++)
        {
            const std::string  & line    = lines[i];
            Word                 address = (Word) std::stoul (line.substr (0, 4), nullptr, 16);
            size_t               at      = (size_t) (address - start);

            Assert::IsTrue   (address >= next, L"each line follows the last");
            Assert::AreEqual (':', line[4]);
            Assert::AreEqual ('\r', line.back());

            for (size_t column = 5; column + 3 <= line.size(); column += 3)
            {
                typed[at++] = (Byte) std::stoul (line.substr (column + 1, 2), nullptr, 16);
            }

            next = (Word) (start + at);
        }

        Assert::IsTrue (typed == image, L"the lines are the finder and the shim, at the bootstrap speed");
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
        Assert::AreEqual (std::string ("3AA0.3E68R 3AA0G\r"), command, L"the finder and the shim, to the shim's end");
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

        InProcessByteStream::CreatePair (pc, apple);

        AssertSucceeded (LinkBootstrap::Send (*pc, lines, 0));
        AssertSucceeded (apple->Read (buffer, sizeof (buffer), 100, got));
        Assert::AreEqual (std::string ("ONE\rTWO\rTHREE\r"), std::string (buffer, buffer + got));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), LinkBootstrap::Send (*pc, lines, 0, [&calls]() { return ++calls > 1; }));
        AssertSucceeded (apple->Read (buffer, sizeof (buffer), 100, got));
        Assert::AreEqual (std::string ("ONE\r"), std::string (buffer, buffer + got), L"one line, then canceled");
    }
};
