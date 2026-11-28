#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleFitTests
//
//  The program and each module as assembled: they have no 65C02 instruction,
//  and their code and buffers stay between the module area and the top of
//  main memory at $C000, which is all an Apple II or II Plus has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ModuleFitTests)
{
public:

    static constexpr size_t  kMemoryTop = 0xC000;

    struct ModuleCase
    {
        const wchar_t *            name;
        std::string                source;
        std::vector<const char *>  buffers;
    };

    static std::vector<LinkChip>  GetChips()
    {
        return { LinkChip::Acia, LinkChip::W5100, LinkChip::Cassette, LinkChip::Network };
    }

    static std::vector<ModuleCase>  GetModules()
    {
        return
        {
            { L"capture",         LinkProgram::GetCaptureModuleSource(),        { "CAPBUF", "CAPEND", "CAPCODE" } },
            { L"block",           LinkProgram::GetBlockModuleSource(),          { "BLKBUF", "VFYBUF", "ZPBUF", "DEVTAB", "STBUF" } },
            { L"13-sector",       LinkProgram::GetThirteenSectorModuleSource(), { "NBUF", "NEND", "SBUF" } },
        };
    }

    static void  AssertInsideModuleArea (const wchar_t * what, size_t address)
    {
        std::wstring  message = std::format (L"{} is at ${:04X}, outside ${:04X} to ${:04X}",
                                             what, address, (size_t) LinkProgram::kModuleAddress, kMemoryTop - 1);



        Assert::IsTrue (address >= LinkProgram::kModuleAddress && address <= kMemoryTop, message.c_str());
    }


    TEST_METHOD (Program_EndsBelowTheTopOfMainMemory)
    {
        std::vector<LinkChip>  chips = GetChips();



        Assert::IsTrue (!chips.empty());

        for (LinkChip chip : chips)
        {
            std::vector<Byte>                      bytes;
            std::string                            errors;
            std::unordered_map<std::string, Word>  symbols;
            size_t                                 end = 0;



            AssertSucceeded (LinkProgram::AssembleProgram (bytes, errors, chip));
            AssertSucceeded (LinkProgram::GetProgramSymbols (chip, symbols));

            end = LinkProgram::kProgramAddress + bytes.size();

            Assert::IsTrue (end <= kMemoryTop, std::format (L"The program ends at ${:X} for chip {}", end, (int) chip).c_str());

            for (const char * name : { "RXEND", "TXEND" })
            {
                auto  found = symbols.find (name);

                if (found != symbols.end())
                {
                    Assert::IsTrue (found->second <= kMemoryTop, TextEncoding::NarrowToWide (std::format ("{} is at ${:X} for chip {}", name, found->second, (int) chip)).c_str());
                }
            }
        }
    }


    TEST_METHOD (EveryModule_AndItsBuffers_FitInTheModuleArea)
    {
        std::vector<ModuleCase>  modules = GetModules();
        std::vector<LinkChip>    chips   = GetChips();



        Assert::IsTrue (!modules.empty());
        Assert::IsTrue (!chips.empty());

        for (const ModuleCase & module : modules)
        {
            for (LinkChip chip : chips)
            {
                std::vector<Byte>                      bytes;
                std::string                            errors;
                std::unordered_map<std::string, Word>  symbols;
                size_t                                 end = 0;



                AssertSucceeded (LinkProgram::AssembleModule (module.source, bytes, errors, chip, false, &symbols));

                end = LinkProgram::kModuleAddress + bytes.size();

                AssertInsideModuleArea (std::format (L"The {} module's end for chip {}", module.name, (int) chip).c_str(), end);

                for (const char * name : module.buffers)
                {
                    auto  found = symbols.find (name);

                    Assert::IsTrue (found != symbols.end(), TextEncoding::NarrowToWide (std::format ("The {} module has no symbol {}", TextEncoding::WideToNarrow (module.name), name)).c_str());
                    AssertInsideModuleArea (TextEncoding::NarrowToWide (name).c_str(), found->second);
                }
            }
        }
    }


    //  A module whose only difference is a 65C02 instruction fails to
    //  assemble, so a module that assembles has 6502 instructions only.
    TEST_METHOD (Module_WithA65C02Instruction_DoesNotAssemble)
    {
        static constexpr const char *  kHeader =
            "        .org $5000\n"
            "        .byte 'C', 'M', 7, 3\n"
            "        .word PROGVER\n"
            "        .word modend - $5000\n"
            "        .word 0\n"
            "        .byte 1\n"
            "        .byte $F0\n"
            "        .word echo\n";

        static constexpr const char *  kNmosTail = "echo:   nop\n        jmp p_main\nmodend:\n";
        static constexpr const char *  kCmosTail = "echo:   bra echo\n        jmp p_main\nmodend:\n";

        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            nmos = LinkProgram::AssembleModule (std::string (kHeader) + kNmosTail, bytes, errors);
        HRESULT            cmos = LinkProgram::AssembleModule (std::string (kHeader) + kCmosTail, bytes, errors);



        Assert::IsTrue (SUCCEEDED (nmos), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (FAILED (cmos),    L"bra assembled, so the instruction set has 65C02 instructions");
    }
};
