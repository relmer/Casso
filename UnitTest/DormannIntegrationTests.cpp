#include "Pch.h"


#include "Assembler.h"
#include "TestHelpers.h"
#include "TestCpu65C02.h"





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
        cpu.InitForTest();
        return Assembler (cpu.GetInstructionSet(), opts);
    }


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler65C02
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler65C02 (AssemblerOptions opts = {})
    {
        TestCpu65C02 cpu;
        cpu.InitForTest();
        return Assembler (cpu.GetInstructionSet(), opts);
    }


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SelectDormannOpcodeSubset
    //
    //  Casso's Cpu65C02 models the Rockwell R65C02: RMB/SMB/BBR/BBS present (the
    //  assembler emits them in as65's `<bit>,<zp>[,<target>]` operand form), but no
    //  WDC WAI/STP. So this path keeps the Rockwell tier enabled (rkwl_wdc_op = 1 --
    //  the Rockwell CPU decodes $x7/$xF as real bit ops, not NOPs) and only disables
    //  wdc_op, leaving WAI/STP tested as NOPs. Only the line-anchored assignment is
    //  rewritten; the `if ... = 1` conditionals that gate subtest assembly stay
    //  intact.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::string SelectDormannOpcodeSubset (const std::string & source)
    {
        return std::regex_replace (
            source, std::regex (R"(^wdc_op = 1)", std::regex::multiline), "wdc_op = 0");
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DownloadFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static HRESULT DownloadFile (const std::string & url, const std::string & destPath)
    {
        int  exitCode = 0;



        // Use curl.exe directly rather than `powershell Invoke-WebRequest`.
        // Windows Defender heuristically flags the
        //   cmd.exe -> powershell -NoProfile -Command Invoke-WebRequest -Uri ... -OutFile ...
        // pattern as Trojan:Win32/ClickFix.R!ml (the same false-positive
        // worked around in scripts/RunDormannTest.ps1). curl.exe ships with
        // Win10 1803+ / Win11 and avoids the heuristic.
        std::string cmd      = "curl.exe -sSL -o \"" + destPath + "\" \"" + url + "\"";
        exitCode = system (cmd.c_str());

        return exitCode == 0 ? S_OK : E_FAIL;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadBinaryFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::vector<Byte> ReadBinaryFile (const std::string & path)
    {
        std::vector<Byte>  data;



        std::ifstream      file (path, std::ios::binary | std::ios::ate);

        // The Dormann binaries are downloaded on demand, so "absent" is an
        // expected state; it reads as empty and the caller skips.
        if (file.is_open())
        {
            auto size = file.tellg();

            file.seekg (0, std::ios::beg);
            data.resize ((size_t) size);
            file.read (reinterpret_cast<char *> (data.data()), size);
        }

        return data;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReadTextFile
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::string ReadTextFile (const std::string & path)
    {
        std::ostringstream  ss;
        std::string         text;



        std::ifstream       file (path);

        // Same contract as ReadBinaryFile: absent reads as empty.
        if (file.is_open())
        {
            ss << file.rdbuf();
            text = ss.str();
        }

        return text;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DormannAssemblyTests
    //
    //  Assembling Klaus Dormann's 6502 functional test suite -- an
    //  independently-authored source of several thousand lines.
    //
    //  The value is that NOBODY HERE WROTE IT. Every other assembler test uses
    //  source written alongside the feature it exercises, which quietly encodes
    //  this assembler's own assumptions about syntax. Dormann's suite was
    //  written for a different assembler entirely, so assembling it at all is
    //  evidence the accepted grammar matches the wider world's.
    //
    //  It is also large enough to reach constructs no hand-written fixture
    //  bothers with -- deep macros, long expression chains, and heavy
    //  conditional assembly.
    //
    //  Marked as an Integration category because it fetches the source, so a
    //  run without network access can exclude it rather than fail.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DormannAssemblyTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DormannAssemblesSuccessfully
        //
        //  Fetches the functional-test source and asserts it assembles with no
        //  errors.
        //
        //  Fetched from upstream rather than vendored, so the test tracks the
        //  suite as it is maintained rather than a snapshot that slowly drifts
        //  out of date.
        //
        //  This is a prerequisite for the execution test rather than an end in
        //  itself: an assembly failure here would otherwise surface as an
        //  incomprehensible CPU failure over there, so the two are separated to
        //  keep the diagnosis clear.
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (DormannAssemblesSuccessfully)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE()

        TEST_METHOD (DormannAssemblesSuccessfully)
        {
            HRESULT           hrDownload   = S_OK;
            std::string       source;
            AssemblerOptions  opts;
            uint32_t          vectorOffset = 0;
            AssemblyResult    result;



            const std::string sourceUrl =
                "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/6502_functional_test.a65";

            std::string sourceFile = "dormann_test_source.dormann.tmp";

            // Download source
            hrDownload = DownloadFile (sourceUrl, sourceFile);

            if (FAILED (hrDownload))
            {
                Logger::WriteMessage ("SKIPPED: Cannot download Dormann source (no network?)");
                return;
            }

            // Read source
            source = ReadTextFile (sourceFile);
            remove (sourceFile.c_str());

            Assert::IsFalse (source.empty(), L"Source file is empty");

            // Assemble
            opts.fillByte = 0x00;

            Assembler  a      = BuildAssembler (opts);
            result = a.Assemble (source);

            // Check for assembly errors (ignore warnings)
            if (!result.success)
            {
                std::wstring msg = L"Assembly failed with errors:";

                for (size_t i = 0; i < result.errors.size() && i < 10; i++)
                {
                    msg += L"\n  Line " + std::to_wstring (result.errors[i].lineNumber)
                         + L": " + std::wstring (result.errors[i].message.begin(), result.errors[i].message.end());
                }

                Assert::Fail (msg.c_str());
            }

            // Verify output covers expected address range
            Assert::IsTrue (result.bytes.size() > 60000, L"Output should be close to 64KB");
            Assert::AreEqual ((Word) 0x000A, result.startAddress, L"Start address should be $000A");

            // Verify vectors are present at $FFFA
            // NMI, RESET, IRQ vectors should be at the end of the output
            vectorOffset = 0xFFFA - result.startAddress;
            Assert::IsTrue (vectorOffset < result.bytes.size(), L"Vectors should be within output");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DormannCpuTests
    //
    //  RUNNING the Dormann functional test suite -- the single strongest
    //  statement of NMOS 6502 correctness this project makes.
    //
    //  The suite exercises every documented instruction in every addressing
    //  mode, with flag checks after each, and it is self-verifying: on any
    //  discrepancy it traps to a tight loop at a known address. So the pass
    //  condition is reaching the designated success address, and the failure
    //  condition is looping anywhere else.
    //
    //  That trap PC is what makes a failure diagnosable. The address maps back
    //  to a specific test in the source, so a failure names the instruction and
    //  the case rather than merely reporting that something went wrong.
    //
    //  It is bounded by a cycle limit, since the failure mode is an infinite
    //  loop by design.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DormannCpuTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DormannRunsInCpu
        //
        //  Loads the assembled suite and runs it to completion, asserting it
        //  reaches the success address.
        //
        //  A trap is detected as the PC ceasing to advance -- the suite's
        //  failure handler is a branch to itself -- and the reported PC is the
        //  diagnosis, since it maps to the specific sub-test in the source.
        //
        //  Decimal mode is included in the run. It is the part of the suite most
        //  emulators skip, and it is where this project's NMOS flag quirks
        //  actually get exercised end to end rather than in isolation.
        //
        //  The cycle bound is generous but finite: the suite legitimately runs
        //  tens of millions of cycles, and an unbounded run would hang the
        //  suite on the very failure it exists to catch.
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (DormannRunsInCpu)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE()

        TEST_METHOD (DormannRunsInCpu)
        {
            std::string   source;
            const char *  skipReason = nullptr;
            HRESULT       hrDownload = S_OK;



            const std::string sourceUrl =
                "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/6502_functional_test.a65";

            std::string   sourceFile = "dormann_cpu_source.dormann.tmp";

            // No network and no source are environment problems, not defects.
            // This test is Integration-tagged and skips rather than failing a
            // build that cannot reach GitHub.
            hrDownload = DownloadFile (sourceUrl, sourceFile);

            if (FAILED (hrDownload))
            {
                skipReason = "SKIPPED: Cannot download Dormann source (no network?)";
            }

            if (skipReason == nullptr)
            {
                source = ReadTextFile (sourceFile);
                remove (sourceFile.c_str());

                if (source.empty())
                {
                    skipReason = "SKIPPED: Source file is empty";
                }
            }

            if (skipReason != nullptr)
            {
                Logger::WriteMessage (skipReason);
            }
            else
            {
                AssemblyResult  result;

                // Assemble
                AssemblerOptions opts;
                opts.fillByte = 0xFF;

                Assembler  a      = BuildAssembler (opts);
                result = a.Assemble (source);

                // Assembler defects belong to DormannAssemblesSuccessfully,
                // which reports them properly -- failing here too would just
                // double-count the same bug.
                if (!result.success)
                {
                    Logger::WriteMessage ("SKIPPED: Assembly failed (see DormannAssemblesSuccessfully)");
                }
                else
                {
                    const int  maxInstructions = 100000000;
                    Word       prevPC          = 0;
                    int        sameCount       = 0;
                    int        executed        = 0;
                    int        i               = 0;
                    bool       passed          = false;
                    Word       successTrap     = 0;

                    // Load into CPU
                    TestCpu cpu;
                    cpu.InitForTest (0x0400);

                    for (size_t i = 0; i < result.bytes.size(); i++)
                    {
                        cpu.Poke ((Word) (result.startAddress + i), result.bytes[i]);
                    }

                    // The Dormann test starts at $0400
                    cpu.RegPC() = 0x0400;

                    // Run with a cycle limit — informational only
                    prevPC = 0xFFFF;
                    successTrap = 0x3469; // Dormann success address

                    for (i = 0; !passed && i < maxInstructions; i++)
                    {
                        Word currentPC = cpu.RegPC();

                        if (currentPC == successTrap)
                        {
                            // Success is silent: arriving at $3469 IS the pass.
                            passed = true;
                        }
                        else
                        {
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
                            cpu.Step();
                            executed++;
                        }
                    }

                    if (!passed)
                    {
                        wchar_t msg[256];
                        swprintf (msg, 256,
                                  L"Dormann test reached the %d-instruction ceiling without "
                                  L"hitting the success trap at $%04X. CPU is at $%04X. Either "
                                  L"the limit needs raising or the CPU is making progress but "
                                  L"no longer converges.",
                                  maxInstructions, successTrap, cpu.RegPC());
                        Assert::Fail (msg);
                    }
                }
            }
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Dormann65C02Tests
    //
    //  The CMOS half of the suite, run against the 65C02 core.
    //
    //  A SEPARATE suite because the 65C02 is not a superset: it adds
    //  instructions and addressing modes, and it also CHANGES documented NMOS
    //  behavior -- the JMP indirect page-boundary bug is fixed, and decimal-mode
    //  flags are computed after the adjustment rather than before.
    //
    //  So running both suites is what pins the divergence in both directions.
    //  A single shared implementation would fail one or the other, and a
    //  65C02-only test would let CMOS behavior leak into the NMOS core
    //  unnoticed.
    //
    //  The Rockwell bit instructions (RMB/SMB/BBR/BBS) are reached here too,
    //  which is where the assembler's two accepted operand syntaxes for them
    //  get exercised against real code.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Dormann65C02Tests)
    {
    public:

        static constexpr const char * kSourceUrl =
            "https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/65C02_extended_opcodes_test.a65c";


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dormann65C02AssemblesSuccessfully
        //
        //  Probe: can the 65C02-aware assembler assemble the Rockwell-tier Dormann
        //  extended-opcodes source (rkwl_wdc_op=1, BBR/BBS/RMB/SMB in as65's operand
        //  form)? Surfaces any addressing-mode/mnemonic gaps before the CPU run.
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (Dormann65C02AssemblesSuccessfully)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE()

        TEST_METHOD (Dormann65C02AssemblesSuccessfully)
        {
            HRESULT           hrDownload = S_OK;
            std::string       source;
            AssemblerOptions  opts;
            AssemblyResult    result;



            std::string sourceFile = "dormann65_source.dormann.tmp";

            hrDownload = DownloadFile (kSourceUrl, sourceFile);

            if (FAILED (hrDownload))
            {
                Logger::WriteMessage ("SKIPPED: Cannot download Dormann 65C02 source (no network?)");
                return;
            }

            source = ReadTextFile (sourceFile);
            remove (sourceFile.c_str());

            Assert::IsFalse (source.empty(), L"Source file is empty");

            source = SelectDormannOpcodeSubset (source);

            opts.fillByte = 0x00;

            Assembler a      = BuildAssembler65C02 (opts);
            result = a.Assemble (source);

            if (!result.success)
            {
                std::wstring msg = L"65C02 assembly failed with errors:";

                for (size_t i = 0; i < result.errors.size() && i < 15; i++)
                {
                    msg += L"\n  Line " + std::to_wstring (result.errors[i].lineNumber)
                         + L": " + std::wstring (result.errors[i].message.begin(), result.errors[i].message.end());
                }

                Assert::Fail (msg.c_str());
            }

            Assert::IsTrue (result.bytes.size() > 8000, L"Output should span the 10K code segment");
        }


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dormann65C02RunsInCpu
        //
        //  Assembles the Rockwell-tier extended-opcodes suite and runs it in a
        //  flat-memory Cpu65C02 to the success self-trap. A self-loop at any other
        //  address is a failing Dormann subtest (its PC identifies which).
        //
        ////////////////////////////////////////////////////////////////////////////////

        BEGIN_TEST_METHOD_ATTRIBUTE (Dormann65C02RunsInCpu)
            TEST_METHOD_ATTRIBUTE (L"TestCategory", L"Integration")
        END_TEST_METHOD_ATTRIBUTE()

        TEST_METHOD (Dormann65C02RunsInCpu)
        {
            std::string   source;
            const char *  skipReason = nullptr;
            HRESULT       hrDownload = S_OK;



            std::string   sourceFile = "dormann65_cpu_source.dormann.tmp";

            // No network and no source are environment problems, not defects:
            // an Integration-tagged test skips rather than failing a build that
            // cannot reach GitHub.
            hrDownload = DownloadFile (kSourceUrl, sourceFile);

            if (FAILED (hrDownload))
            {
                skipReason = "SKIPPED: Cannot download Dormann 65C02 source (no network?)";
            }

            if (skipReason == nullptr)
            {
                source = ReadTextFile (sourceFile);
                remove (sourceFile.c_str());

                if (source.empty())
                {
                    skipReason = "SKIPPED: Source file is empty";
                }
            }

            if (skipReason != nullptr)
            {
                Logger::WriteMessage (skipReason);
            }
            else
            {
                AssemblerOptions  opts;
                AssemblyResult    result;

                source = SelectDormannOpcodeSubset (source);

                opts.fillByte = 0xFF;

                Assembler a      = BuildAssembler65C02 (opts);
                result = a.Assemble (source);

                // Assembler defects belong to Dormann65C02AssemblesSuccessfully,
                // which reports them properly; failing here as well would just
                // double-count the same bug.
                if (!result.success)
                {
                    Logger::WriteMessage ("SKIPPED: Assembly failed (see Dormann65C02AssemblesSuccessfully)");
                }
                else
                {
                    TestCpu65C02  cpu;
                    const int     maxInstructions = 200000000;
                    Word          successTrap     = 0;
                    Word          prevPC          = 0;
                    int           sameCount       = 0;
                    int           i               = 0;
                    bool          passed          = false;
                    cpu.InitForTest (0x0400);

                    for (size_t i = 0; i < result.bytes.size(); i++)
                    {
                        cpu.Poke ((Word) (result.startAddress + i), result.bytes[i]);
                    }

                    cpu.RegPC() = 0x0400;

                    // With the Rockwell-tier subset (wdc_op=0, rkwl_wdc_op=1) the extended-
                    // opcodes suite runs to a success self-trap at $2569 (~22M instructions).
                    // A self-loop at any other PC is a failing Dormann subtest; its address
                    // identifies which.
                    successTrap = 0x2569;
                    prevPC = 0xFFFF;

                    for (i = 0; !passed && i < maxInstructions; i++)
                    {
                        Word currentPC = cpu.RegPC();

                        if (currentPC == successTrap)
                        {
                            // Success is silent: arriving at $2569 IS the pass.
                            passed = true;
                        }
                        else
                        {
                            // Detect a trap (same PC twice in a row = self-loop).
                            if (currentPC == prevPC)
                            {
                                if (++sameCount >= 2)
                                {
                                    wchar_t msg[256];
                                    swprintf (msg, 256,
                                              L"Dormann 65C02 trap at $%04X after %d instructions "
                                              L"(success trap is $%04X). A trap at any other PC is a "
                                              L"failing subtest -- the address identifies which one in "
                                              L"the Dormann source.",
                                              currentPC, i, successTrap);
                                    Assert::Fail (msg);
                                }
                            }
                            else
                            {
                                sameCount = 0;
                            }

                            prevPC = currentPC;
                            cpu.Step();
                        }
                    }

                    if (!passed)
                    {
                        Assert::Fail (L"Dormann 65C02 reached the instruction ceiling without "
                                      L"hitting the success trap at $2569.");
                    }
                }
            }
        }
    };
}
