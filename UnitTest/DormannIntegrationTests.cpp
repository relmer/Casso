#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DormannIntegrationTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler (AssemblerOptions opts = {})
    {
        TestCpu cpu;
        cpu.InitForTest ();
        return Assembler (cpu.GetInstructionSet (), opts);
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DownloadFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static bool DownloadFile (const std::string & url, const std::string & destPath)
    {
        std::string cmd = "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '"
                          + url + "' -OutFile '" + destPath + "' -UseBasicParsing\"";

        return system (cmd.c_str ()) == 0;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadBinaryFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::vector<Byte> ReadBinaryFile (const std::string & path)
    {
        std::ifstream file (path, std::ios::binary | std::ios::ate);

        if (!file.is_open ())
        {
            return {};
        }

        auto size = file.tellg ();
        file.seekg (0, std::ios::beg);

        std::vector<Byte> data ((size_t) size);
        file.read (reinterpret_cast<char *> (data.data ()), size);
        return data;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadTextFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::string ReadTextFile (const std::string & path)
    {
        std::ifstream file (path);

        if (!file.is_open ())
        {
            return {};
        }

        std::ostringstream ss;
        ss << file.rdbuf ();
        return ss.str ();
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DormannAssemblyTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DormannAssemblyTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DormannAssemblesSuccessfully
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (DormannAssemblesSuccessfully)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE ()

        TEST_METHOD (DormannAssemblesSuccessfully)
        {
            const std::string sourceUrl =
                "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/6502_functional_test.a65";

            std::string sourceFile = "dormann_test_source.dormann.tmp";

            // Download source
            if (!DownloadFile (sourceUrl, sourceFile))
            {
                Logger::WriteMessage ("SKIPPED: Cannot download Dormann source (no network?)");
                return;
            }

            // Read source
            std::string source = ReadTextFile (sourceFile);
            remove (sourceFile.c_str ());

            Assert::IsFalse (source.empty (), L"Source file is empty");

            // Assemble
            AssemblerOptions opts;
            opts.fillByte = 0x00;

            Assembler a = BuildAssembler (opts);
            auto result = a.Assemble (source);

            // Check for assembly errors (ignore warnings)
            if (!result.success)
            {
                std::wstring msg = L"Assembly failed with errors:";

                for (size_t i = 0; i < result.errors.size () && i < 10; i++)
                {
                    msg += L"\n  Line " + std::to_wstring (result.errors[i].lineNumber)
                         + L": " + std::wstring (result.errors[i].message.begin (), result.errors[i].message.end ());
                }

                Assert::Fail (msg.c_str ());
            }

            // Verify output covers expected address range
            Assert::IsTrue (result.bytes.size () > 60000, L"Output should be close to 64KB");
            Assert::AreEqual ((Word) 0x000A, result.startAddress, L"Start address should be $000A");

            // Verify vectors are present at $FFFA
            // NMI, RESET, IRQ vectors should be at the end of the output
            uint32_t vectorOffset = 0xFFFA - result.startAddress;
            Assert::IsTrue (vectorOffset < result.bytes.size (), L"Vectors should be within output");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DormannCpuTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DormannCpuTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DormannRunsInCpu
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (DormannRunsInCpu)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE ()

        TEST_METHOD (DormannRunsInCpu)
        {
            const std::string sourceUrl =
                "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/6502_functional_test.a65";

            std::string sourceFile = "dormann_cpu_source.dormann.tmp";

            // Download source
            if (!DownloadFile (sourceUrl, sourceFile))
            {
                Logger::WriteMessage ("SKIPPED: Cannot download Dormann source (no network?)");
                return;
            }

            std::string source = ReadTextFile (sourceFile);
            remove (sourceFile.c_str ());

            if (source.empty ())
            {
                Logger::WriteMessage ("SKIPPED: Source file is empty");
                return;
            }

            // Assemble
            AssemblerOptions opts;
            opts.fillByte = 0xFF;

            Assembler a = BuildAssembler (opts);
            auto result = a.Assemble (source);

            if (!result.success)
            {
                Logger::WriteMessage ("SKIPPED: Assembly failed (see DormannAssemblesSuccessfully)");
                return;
            }

            // Load into CPU
            TestCpu cpu;
            cpu.InitForTest (0x0400);

            for (size_t i = 0; i < result.bytes.size (); i++)
            {
                cpu.Poke ((Word) (result.startAddress + i), result.bytes[i]);
            }

            // The Dormann test starts at $0400
            cpu.RegPC () = 0x0400;

            // Run with a cycle limit — informational only
            const int    maxInstructions = 100000000;
            Word         prevPC          = 0xFFFF;
            int          sameCount       = 0;
            int          executed        = 0;
            const Word   successTrap     = 0x3469;   // Dormann success address

            for (int i = 0; i < maxInstructions; i++)
            {
                Word currentPC = cpu.RegPC ();

                if (currentPC == successTrap)
                {
                    return;  // Success: silent
                }

                // Detect infinite loop (same PC twice in a row = trap)
                if (currentPC == prevPC)
                {
                    sameCount++;

                    if (sameCount >= 2)
                    {
                        wchar_t msg[256];
                        swprintf (msg, 256,
                                  L"Dormann CPU trap at $%04X after %d instructions "
                                  L"(success trap is $%04X). A trap at any other PC "
                                  L"means a Dormann subtest failed -- the trapping "
                                  L"address identifies which subtest in the Dormann "
                                  L"source.",
                                  currentPC, i, successTrap);
                        Assert::Fail (msg);
                    }
                }
                else
                {
                    sameCount = 0;
                }

                prevPC = currentPC;
                cpu.Step ();
                executed++;
            }

            wchar_t msg[256];
            swprintf (msg, 256,
                      L"Dormann test reached the %d-instruction ceiling without "
                      L"hitting the success trap at $%04X. CPU is at $%04X. Either "
                      L"the limit needs raising or the CPU is making progress but "
                      L"no longer converges.",
                      maxInstructions, successTrap, cpu.RegPC ());
            Assert::Fail (msg);
        }
    };
}
