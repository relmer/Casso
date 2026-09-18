#include "Pch.h"

#include "TestHelpers.h"
#include "EmuTests/FixtureProvider.h"
#include "Disassembler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorListing1979Tests
//
//  The original Apple II Monitor, disassembled by Casso, must agree line for
//  line with the listing Apple printed in the 1979 Reference Manual.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MonitorListing1979Tests)
    {
    public:

        struct ListingLine
        {
            Word               address = 0;
            std::vector<Byte>  bytes;
            std::string        mnemonic;
            std::string        operand;
        };

        struct DataRange
        {
            int  first = 0;
            int  last  = 0;
        };

        struct Listing
        {
            std::vector<ListingLine>  lines;
            std::vector<DataRange>    data;
            size_t                    declaredCount = 0;
        };

        static constexpr int     kRomBase      = 0xD000;
        static constexpr size_t  kRomSize      = 0x3000;
        static constexpr int     kMonitorStart = 0xF800;
        static constexpr int     kAddressEnd   = 0x10000;



        ////////////////////////////////////////////////////////////////////////
        //
        //  SplitTokens
        //
        ////////////////////////////////////////////////////////////////////////

        static std::vector<std::string> SplitTokens (const std::string & line)
        {
            std::vector<std::string> tokens;
            std::istringstream       stream (line);
            std::string              token;



            while (stream >> token)
            {
                tokens.push_back (token);
            }

            return tokens;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ParseListing
        //
        //  Instruction lines are ADDR, two-digit byte tokens, MNEMONIC, then
        //  the operand. DATA lines give inclusive ranges. The header carries
        //  the instruction count as "; instructions: N".
        //
        ////////////////////////////////////////////////////////////////////////

        static Listing ParseListing (const std::vector<uint8_t> & text)
        {
            static constexpr const char * kCountPrefix = "; instructions:";

            Listing             listing;
            std::istringstream  stream (std::string (text.begin(), text.end()));
            std::string         line;



            while (std::getline (stream, line))
            {
                std::vector<std::string> tokens;
                ListingLine              entry;
                size_t                   index = 1;



                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                if (line.starts_with (kCountPrefix))
                {
                    listing.declaredCount = std::stoul (line.substr (strlen (kCountPrefix)));
                    continue;
                }

                tokens = SplitTokens (line);

                if (tokens.empty() || tokens[0].starts_with (";"))
                {
                    continue;
                }

                if (tokens[0] == "DATA")
                {
                    listing.data.push_back ({ std::stoi (tokens[1].substr (0, 4), nullptr, 16),
                                              std::stoi (tokens[1].substr (5, 4), nullptr, 16) });
                    continue;
                }

                entry.address = (Word) std::stoul (tokens[0], nullptr, 16);

                while (index < tokens.size() && tokens[index].size() == 2)
                {
                    entry.bytes.push_back ((Byte) std::stoul (tokens[index], nullptr, 16));
                    ++index;
                }

                Assert::IsTrue (index < tokens.size(), L"listing line has no mnemonic");
                entry.mnemonic = tokens[index++];

                for (; index < tokens.size(); ++index)
                {
                    entry.operand += tokens[index];
                }

                listing.lines.push_back (entry);
            }

            return listing;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  FindDataRangeEnd
        //
        //  The last address of the DATA range starting at address, or -1.
        //
        ////////////////////////////////////////////////////////////////////////

        static int FindDataRangeEnd (const Listing & listing, int address)
        {
            for (const DataRange & range : listing.data)
            {
                if (range.first == address)
                {
                    return range.last;
                }
            }

            return -1;
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  LoadFixtures
        //
        ////////////////////////////////////////////////////////////////////////

        static void LoadFixtures (std::vector<uint8_t> & rom, Listing & listing)
        {
            FixtureProvider      provider;
            std::vector<uint8_t> text;
            HRESULT              hr = S_OK;



            hr = provider.OpenFixture ("Apple2.rom", rom);
            Assert::AreEqual (S_OK, hr, L"Apple2.rom fixture missing; run scripts/FetchRoms.ps1 -Fixtures");
            Assert::AreEqual (kRomSize, rom.size());

            hr = provider.OpenFixture ("Debugger/AppleII-1979-MonitorListing.txt", text);
            Assert::AreEqual (S_OK, hr, L"1979 listing fixture missing");

            listing = ParseListing (text);
            Assert::IsTrue   (listing.lines.size() > 0, L"listing parsed to zero instructions");
            Assert::AreEqual (listing.declaredCount, listing.lines.size());
        }



        TEST_METHOD (RomMatchesListingBytes)
        {
            std::vector<uint8_t> rom;
            Listing              listing;



            LoadFixtures (rom, listing);

            for (const ListingLine & line : listing.lines)
            {
                for (size_t i = 0; i < line.bytes.size(); ++i)
                {
                    Assert::AreEqual (line.bytes[i], rom[line.address + i - kRomBase],
                                      std::format (L"ROM byte differs from listing at ${:04X}", line.address + i).c_str());
                }
            }
        }



        TEST_METHOD (DisassemblyMatchesListing)
        {
            std::vector<uint8_t>    rom;
            Listing                 listing;
            TestCpu                 cpu;
            DisassembledInstruction instruction;
            int                     pc      = kMonitorStart;
            size_t                  matched = 0;
            HRESULT                 hr      = S_OK;



            LoadFixtures (rom, listing);
            cpu.InitForTest();

            while (pc < kAddressEnd)
            {
                int                    dataEnd   = FindDataRangeEnd (listing, pc);
                size_t                 available = std::min ((size_t) (kAddressEnd - pc), Disassembler::kMaxInstructionBytes);
                std::span<const Byte>  bytes (rom.data() + (pc - kRomBase), available);
                std::wstring           where     = std::format (L"at ${:04X}", pc);
                const ListingLine    * expected  = nullptr;



                if (dataEnd >= 0)
                {
                    pc = dataEnd + 1;
                    continue;
                }

                Assert::IsTrue (matched < listing.lines.size(), (L"disassembly ran past the listing " + where).c_str());
                expected = &listing.lines[matched];

                hr = Disassembler (cpu.GetInstructionSet()).DisassembleOne ((Word) pc, bytes, instruction);
                Assert::AreEqual (S_OK, hr, where.c_str());

                Assert::AreEqual (expected->address,      instruction.address,      where.c_str());
                Assert::AreEqual (expected->bytes.size(), instruction.bytes.size(), where.c_str());
                Assert::AreEqual (expected->mnemonic,     instruction.mnemonic,     where.c_str());
                Assert::AreEqual (expected->operand,      instruction.operand,      where.c_str());

                pc += (int) instruction.bytes.size();
                ++matched;
            }

            Assert::AreEqual (listing.lines.size(), matched);
        }
    };
}
