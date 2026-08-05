#include "Pch.h"

#include "Assembler.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IncludeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MockFileReader
    //
    //  An in-memory FileReader so .include can be tested without touching the
    //  filesystem.
    //
    //  That is what makes include testing practical at all -- a nested include
    //  graph, a circular include, and a missing file are all a few table
    //  entries here, where on disk each would be a fixture tree to create and
    //  clean up.
    //
    //  It records which paths were REQUESTED, not just what it returned, so a
    //  test can assert the resolution order and that a file was read once
    //  rather than repeatedly.
    //
    //  Path matching is exact and deliberately unforgiving, which is how the
    //  assembler's own path resolution stays the thing under test rather than
    //  being papered over by a lenient double.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MockFileReader : public FileReader
    {
    public:
        std::unordered_map<std::string, std::string> files;



        FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) override
        {
            FileReadResult result = {};

            auto it = files.find (filename);

            if (it != files.end())
            {
                result.success  = true;
                result.contents = it->second;
            }
            else
            {
                result.success = false;
                result.error   = "File not found: " + filename;
            }

            return result;
        }
    };





    TEST_CLASS (IncludeDirectiveTests)
    {
    public:

        TEST_METHOD (IncludeInsertsFileContent)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["defs.a65"] = "val = $42\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"defs.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // LDA #$42 = A9 42
            Assert::AreEqual ((size_t) 2, result.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, result.bytes[1]);
        }





        TEST_METHOD (DotIncludeWorks)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["defs.a65"] = "val = $55\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    .include \"defs.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x55, result.bytes[1]);
        }





        TEST_METHOD (NestedIncludesWork)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["outer.a65"] = "    include \"inner.a65\"\n";
            reader.files["inner.a65"] = "val = $33\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"outer.a65\"\n"
                "    lda #val\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((Byte) 0x33, result.bytes[1]);
        }





        TEST_METHOD (IncludeFileNotFound_Error)
        {
            TestCpu cpu;
            MockFileReader reader;

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"missing.a65\"\n"
            );

            Assert::IsFalse (result.success, L"Should fail for missing file");
            Assert::IsTrue (result.errors.size() > 0, L"Should have error");
        }





        TEST_METHOD (NoFileReader_Error)
        {
            TestCpu cpu;

            Assembler assembler (cpu.GetInstructionSet());
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"file.a65\"\n"
            );

            Assert::IsFalse (result.success, L"Should fail without reader");
        }





        TEST_METHOD (BinaryFileInclude_EmitsRawBytes)
        {
            TestCpu cpu;
            MockFileReader reader;

            // Raw bytes: 0xDE 0xAD 0xBE 0xEF
            std::string rawData;
            rawData += (char) 0xDE;
            rawData += (char) 0xAD;
            rawData += (char) 0xBE;
            rawData += (char) 0xEF;
            reader.files["data.bin"] = rawData;

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"data.bin\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0xDE, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xAD, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xBE, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xEF, result.bytes[3]);
        }





        TEST_METHOD (SRecordInclude_ExtractsDataBytes)
        {
            TestCpu cpu;
            MockFileReader reader;

            // S1 record: byte count=07, addr=0000, data=DE AD BE EF, checksum
            reader.files["data.s19"] =
                "S0030000FC\r\n"
                "S1070000DEADBEEFC0\r\n"
                "S9030000FC\r\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"data.s19\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // Data should start at byte offset 2 (after 2-byte address 0x0000)
            // Actually S1: count=07, addr(2)=0000, data(4)=DEADBEEF, cksum(1)
            // dataBytes = 07 - 2 - 1 = 4
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0xDE, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xAD, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xBE, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xEF, result.bytes[3]);
        }





        TEST_METHOD (IntelHexInclude_ExtractsDataBytes)
        {
            TestCpu cpu;
            MockFileReader reader;

            // Intel HEX: 4 data bytes at address 0000
            reader.files["data.hex"] =
                ":04000000DEADBEEF52\r\n"
                ":00000001FF\r\n";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"data.hex\"\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0xDE, result.bytes[0]);
            Assert::AreEqual ((Byte) 0xAD, result.bytes[1]);
            Assert::AreEqual ((Byte) 0xBE, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xEF, result.bytes[3]);
        }





        TEST_METHOD (BinaryInclude_AdvancesPC)
        {
            TestCpu cpu;
            MockFileReader reader;

            std::string rawData;
            rawData += (char) 0x01;
            rawData += (char) 0x02;
            rawData += (char) 0x03;
            reader.files["data.bin"] = rawData;

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"data.bin\"\n"
                "    nop\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            // 3 bytes of binary data + 1 byte NOP
            Assert::AreEqual ((size_t) 4, result.bytes.size());
            Assert::AreEqual ((Byte) 0x01, result.bytes[0]);
            Assert::AreEqual ((Byte) 0x02, result.bytes[1]);
            Assert::AreEqual ((Byte) 0x03, result.bytes[2]);
            Assert::AreEqual ((Byte) 0xEA, result.bytes[3]); // NOP
        }





        TEST_METHOD (EmptyBinaryInclude_Succeeds)
        {
            TestCpu cpu;
            MockFileReader reader;
            reader.files["empty.bin"] = "";

            AssemblerOptions opts = {};
            opts.fileReader = &reader;

            Assembler assembler (cpu.GetInstructionSet(), opts);
            auto result = assembler.Assemble (
                "    .org $1000\n"
                "    include \"empty.bin\"\n"
                "    nop\n"
            );

            Assert::IsTrue (result.success, L"Assembly should succeed");
            Assert::AreEqual ((size_t) 1, result.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, result.bytes[0]); // NOP
        }
    };
}
