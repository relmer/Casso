#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ConformanceTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BuildAssembler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static Assembler BuildAssembler ()
    {
        TestCpu cpu;
        cpu.InitForTest ();
        return Assembler (cpu.GetInstructionSet ());
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GetTestDataDir
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::string GetTestDataDir ()
    {
        // __FILE__ points to UnitTest/ConformanceTests.cpp
        // Navigate up to repo root, then into testdata/conformance
        std::string thisFile = __FILE__;
        size_t      lastSep  = thisFile.find_last_of ("\\/");
        std::string unitDir  = thisFile.substr (0, lastSep);

        lastSep = unitDir.find_last_of ("\\/");
        std::string repoRoot = unitDir.substr (0, lastSep);

        return repoRoot + "\\specs\\002-as65-assembler-compat\\testdata\\conformance";
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
    //  FormatBytes — hex dump for diagnostics
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::string FormatBytes (const std::vector<Byte> & bytes)
    {
        std::ostringstream ss;

        for (size_t i = 0; i < bytes.size (); i++)
        {
            if (i > 0)
            {
                ss << " ";
            }

            char buf[4];
            snprintf (buf, sizeof (buf), "%02X", bytes[i]);
            ss << buf;
        }

        return ss.str ();
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RunOneConformanceTest
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void RunOneConformanceTest (const std::string & a65Path,
                                       const std::string & binPath,
                                       const std::string & testName)
    {
        std::string source = ReadTextFile (a65Path);

        Assert::IsFalse (source.empty (),
            (L"Cannot read source: " + std::wstring (testName.begin (), testName.end ())).c_str ());

        std::vector<Byte> expected = ReadBinaryFile (binPath);

        Assert::IsFalse (expected.empty (),
            (L"Cannot read expected: " + std::wstring (testName.begin (), testName.end ())).c_str ());

        Assembler         asm6502 = BuildAssembler ();
        AssemblyResult    result  = asm6502.Assemble (source);

        if (!result.success)
        {
            std::string errMsg = testName + " assembly failed:";

            for (auto & e : result.errors)
            {
                errMsg += "\n  Line " + std::to_string (e.lineNumber) + ": " + e.message;
            }

            Assert::Fail (std::wstring (errMsg.begin (), errMsg.end ()).c_str ());
        }

        if (result.bytes.size () != expected.size ())
        {
            std::string msg = testName + " size mismatch: expected "
                              + std::to_string (expected.size ()) + " bytes, got "
                              + std::to_string (result.bytes.size ())
                              + "\n  Expected: " + FormatBytes (expected)
                              + "\n  Actual:   " + FormatBytes (result.bytes);

            Assert::Fail (std::wstring (msg.begin (), msg.end ()).c_str ());
        }

        for (size_t i = 0; i < expected.size (); i++)
        {
            if (result.bytes[i] != expected[i])
            {
                std::string msg = testName + " byte mismatch at offset "
                                  + std::to_string (i)
                                  + "\n  Expected: " + FormatBytes (expected)
                                  + "\n  Actual:   " + FormatBytes (result.bytes);

                Assert::Fail (std::wstring (msg.begin (), msg.end ()).c_str ());
            }
        }
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConformanceTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConformanceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_ExprBasic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_ExprBasic)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\expr_basic.a65",
                                  dir + "\\expr_basic.expected.bin",
                                  "expr_basic");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_ExprLoHi
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_ExprLoHi)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\expr_lohi.a65",
                                  dir + "\\expr_lohi.expected.bin",
                                  "expr_lohi");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_Constants
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_Constants)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\constants.a65",
                                  dir + "\\constants.expected.bin",
                                  "constants");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_Conditionals
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_Conditionals)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\conditionals.a65",
                                  dir + "\\conditionals.expected.bin",
                                  "conditionals");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_MacrosBasic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_MacrosBasic)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\macros_basic.a65",
                                  dir + "\\macros_basic.expected.bin",
                                  "macros_basic");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_LabelsColonless
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_LabelsColonless)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\labels_colonless.a65",
                                  dir + "\\labels_colonless.expected.bin",
                                  "labels_colonless");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_DirectivesDs
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_DirectivesDs)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\directives_ds.a65",
                                  dir + "\\directives_ds.expected.bin",
                                  "directives_ds");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_DirectivesAlign
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_DirectivesAlign)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\directives_align.a65",
                                  dir + "\\directives_align.expected.bin",
                                  "directives_align");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_AddressingModes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_AddressingModes)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\addressing_modes.a65",
                                  dir + "\\addressing_modes.expected.bin",
                                  "addressing_modes");
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Conformance_ForwardRef
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Conformance_ForwardRef)
        {
            std::string dir = GetTestDataDir ();
            RunOneConformanceTest (dir + "\\forward_ref.a65",
                                  dir + "\\forward_ref.expected.bin",
                                  "forward_ref");
        }
    };
}

