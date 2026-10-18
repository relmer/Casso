#include "Pch.h"

#include "Debugger/Handlers/MemoryHandlers.h"
#include "Debugger/MemoryMap.h"
#include "HandlerTestRig.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapTests
//
//  MAP: the resolved memory map from the language card, auxiliary memory and
//  ROM switches, in every mode.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryMapTests)
    {
    private:
        struct Targets
        {
            std::string  read;
            std::string  write;
        };

        static Targets Find (const std::vector<MemoryMapRange> & ranges, Word address)
        {
            for (const MemoryMapRange & range : ranges)
            {
                if (address >= range.first && address <= range.last)
                {
                    return { range.read, range.write };
                }
            }

            Assert::Fail (L"an address is missing from the map");
            return {};
        }

        //  The //e's routing for one address, written per address rather than
        //  per range so it checks the range boundaries Build draws.
        static Targets Expect (const std::map<std::string, bool> & s, Word address)
        {
            auto         on    = [&] (const char * name) { return s.at (name); };
            std::string  bank  = on ("ALTZP") ? "aux RAM" : "main RAM";
            std::string  rd    = on ("RAMRD")  ? "aux RAM" : "main RAM";
            std::string  wr    = on ("RAMWRT") ? "aux RAM" : "main RAM";
            std::string  pg    = on ("PAGE2")  ? "aux RAM" : "main RAM";
            std::string  lc    = std::string (on ("ALTZP") ? "aux" : "main") + " LC";



            if (address < 0x0200)                                                  { return { bank, bank }; }
            if (address >= 0x0400 && address < 0x0800 && on ("80STORE"))           { return { pg, pg }; }
            if (address >= 0x2000 && address < 0x4000 && on ("80STORE") && on ("HIRES")) { return { pg, pg }; }
            if (address < 0xC000)                                                  { return { rd, wr }; }
            if (address < 0xC100)                                                  { return { "I/O", "I/O" }; }

            if (address < 0xC800)
            {
                bool  internal = on ("INTCXROM") || (address >= 0xC300 && address < 0xC400 && !on ("SLOTC3ROM"));

                return { internal ? "internal ROM" : "slot ROM", "nothing" };
            }

            if (address < 0xD000)
            {
                return { on ("INTCXROM") || on ("INTC8ROM") ? "internal ROM" : "slot ROM", "nothing" };
            }

            std::string  ram = address < 0xE000 ? lc + (on ("LCBANK2") ? " bank 2" : " bank 1") : lc + " RAM";

            return { on ("LCREAD") ? ram : "ROM", on ("LCWRITE") ? ram : "nothing" };
        }

    public:
        //  SC-036: every combination of the switches that route memory, at
        //  every page boundary and the byte before it.
        TEST_METHOD (EveryRange_MatchesTheIIeRouting_InEverySwitchCombination)
        {
            static const char * const  kNames[] = { "RAMRD", "RAMWRT", "ALTZP", "80STORE", "INTCXROM", "SLOTC3ROM", "INTC8ROM",
                                                    "PAGE2", "HIRES", "LCREAD", "LCWRITE", "LCBANK2" };
            constexpr int              kCount   = (int) std::size (kNames);



            for (int bits = 0; bits < (1 << kCount); ++bits)
            {
                std::vector<SoftSwitch>      switches;
                std::map<std::string, bool>  state;



                for (int i = 0; i < kCount; ++i)
                {
                    bool  value = (bits & (1 << i)) != 0;

                    switches.push_back ({ kNames[i], value });
                    state[kNames[i]] = value;
                }

                std::vector<MemoryMapRange>  ranges = MemoryMap::Build (switches);

                for (uint32_t page = 0; page < 0x10000; page += 0x100)
                {
                    for (uint32_t address : { page, page + 0xFF })
                    {
                        Targets  got  = Find   (ranges, (Word) address);
                        Targets  want = Expect (state,  (Word) address);

                        if (got.read != want.read || got.write != want.write)
                        {
                            std::wstring  where = std::format (L"switches {:03X}, ${:04X}", bits, address);

                            Assert::AreEqual (want.read,  got.read,  where.c_str());
                            Assert::AreEqual (want.write, got.write, where.c_str());
                        }
                    }
                }
            }
        }

        TEST_METHOD (AMachineWithoutAnMmuOrLanguageCard_ShowsMainRamAndRom)
        {
            std::vector<MemoryMapRange>  ranges = MemoryMap::Build ({ { "TEXT", true }, { "PAGE2", true } });



            Assert::AreEqual (size_t (4), ranges.size());
            Assert::AreEqual ((int) 0xBFFF, (int) ranges[0].last);
            Assert::AreEqual (std::string ("main RAM"), ranges[0].write);
            Assert::AreEqual (std::string ("slot ROM"), ranges[2].read);
            Assert::AreEqual (std::string ("ROM"),      ranges[3].read);
            Assert::AreEqual (std::string ("nothing"),  ranges[3].write);
        }

        TEST_METHOD (Map_RunsInEveryMode)
        {
            HandlerRig<MemoryHandlers>  rig;



            rig.target.softSwitches = { { "RAMRD", false }, { "RAMWRT", true } };

            for (auto [line, mode] : { std::pair { "MAP", CommandMode::AppleWin }, { "MAP", CommandMode::Casso },
                                       { "/MAP", CommandMode::Monitor }, { "map", CommandMode::GSSquared },
                                       { "!map", CommandMode::WinDbg } })
            {
                Reply         reply = rig.session.ExecuteLine (line, mode);
                std::wstring  where (line, line + strlen (line));
                std::string   all;



                rig.session.FormatReply (reply);

                for (const std::string & text : reply.text)
                {
                    all += text + "\n";
                }

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
                Assert::IsTrue   (all.find ("$0200-$BFFF  main RAM          aux RAM") != std::string::npos, where.c_str());
            }
        }

        //  The map follows the switches a real //e has just been set to.
        TEST_METHOD (Map_FollowsTheSwitches_OnARealIIe)
        {
            MachineHandlerRig<MemoryHandlers>  rig;
            std::string                        all;



            rig.RunOk ("OUT C005 0");      // RAMWRT on
            rig.RunOk ("IN C088");         // language card: read RAM bank 1, writes off

            for (const std::string & text : rig.RunOk ("MAP").text)
            {
                all += text + "\n";
            }

            Assert::IsTrue (all.find ("$0200-$BFFF  main RAM          aux RAM") != std::string::npos, Widen (all).c_str());
            Assert::IsTrue (all.find ("$D000-$DFFF  main LC bank 1    nothing") != std::string::npos, Widen (all).c_str());
        }

    private:
        static std::wstring Widen (const std::string & text)
        {
            return TextEncoding::NarrowToWide (text);
        }
    };
}
