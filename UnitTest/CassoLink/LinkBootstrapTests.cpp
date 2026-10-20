#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrapTests
//
//  The text typed into the Apple's monitor to start the link (FR-092, R23):
//  Ctrl-X, CALL -151, the shim as lines of eight bytes from its address, and the
//  command that runs it; and the typing itself.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkBootstrapTests)
{
public:

    TEST_METHOD (Lines_TypeTheShimIntoTheMonitor_AndRunIt)
    {
        std::vector<std::string>  lines;
        std::vector<Byte>         shim;
        std::vector<Byte>         typed;
        Word                      next = LinkProgram::kShimAddress;

        AssertSucceeded (LinkBootstrap::BuildLines (9600, lines));
        AssertSucceeded (LinkProgram::BuildShim (9600, shim));

        Assert::AreEqual (std::string ("\x18"),        lines[0], L"Ctrl-X cancels a partial line from stray bytes");
        Assert::AreEqual (std::string ("CALL -151\r"), lines[1]);
        Assert::AreEqual (std::string ("3D00G\r"),     lines.back());

        for (size_t i = 2; i + 1 < lines.size(); i++)
        {
            const std::string  & line    = lines[i];
            Word                 address = (Word) std::stoul (line.substr (0, 4), nullptr, 16);

            Assert::AreEqual (next, address, L"each line follows the last");
            Assert::AreEqual (':', line[4]);
            Assert::AreEqual ('\r', line.back());

            for (size_t at = 5; at + 3 <= line.size(); at += 3)
            {
                typed.push_back ((Byte) std::stoul (line.substr (at + 1, 2), nullptr, 16));
            }

            next = (Word) (LinkProgram::kShimAddress + typed.size());
        }

        Assert::IsTrue (typed == shim, L"the lines are the shim, at the bootstrap speed");
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
