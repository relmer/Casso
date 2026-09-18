#include "Pch.h"

#include "Assembler.h"
#include "Debugger/DebugFileReader.h"
#include "Debugger/DebugFileWriter.h"
#include "Debugger/LineTable.h"
#include "EmuTests/FixtureProvider.h"
#include "TestHelpers.h"
#include "MockFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileRoundTripTests
//
//  A real source through each assembler, out to a debug file and back in
//  (FR-066). Every emitted address must come back with the lines that
//  produced it, outermost first, at the right depth.
//
//  The fixtures are one program in each dialect: an include, and a macro that
//  invokes another, so an address inside the inner body belongs to three lines.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static std::string ReadFixture (const std::string & name)
    {
        FixtureProvider       provider;
        std::vector<uint8_t>  bytes;
        HRESULT               hr = provider.OpenFixture ("Debugger/Sources/" + name, bytes);



        Assert::AreEqual (S_OK, hr, std::wstring (name.begin(), name.end()).c_str());
        return std::string (bytes.begin(), bytes.end());
    }


    TEST_CLASS (DebugFileRoundTripTests)
    {
    public:

        struct RoundTrip
        {
            AssemblyResult  result;
            DebugFile       file;
            LineTable       table;
        };



        static RoundTrip Run (DialectId dialect, const std::string & mainName, const std::string & includeName)
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;
            RoundTrip         trip;
            std::string       error;



            reader.files[includeName] = ReadFixture (includeName);
            opts.fileReader           = &reader;
            opts.dialect              = dialect;
            trip.result               = Assembler (cpu.GetInstructionSet(), opts).Assemble (ReadFixture (mainName));

            Assert::IsTrue (trip.result.success);
            Assert::IsTrue (trip.result.sourceTexts.contains (includeName), L"the include was read");

            Assert::AreEqual (S_OK, DebugFileReader::Read (
                DebugFileWriter::Format (DebugFileWriter::Build (trip.result, { { "", { mainName, 0 } } })),
                trip.file, error));

            trip.table.Build (trip.file);
            return trip;
        }



        //  Every byte the assembly emitted, looked up in the table read back,
        //  gives the record's positions: same files, same lines, same depths.
        static size_t AssertEveryAddressResolves (const RoundTrip & trip, const std::string & mainName)
        {
            size_t  checked = 0;



            for (const DebugLineRecord & record : trip.result.debugLines)
            {
                for (size_t offset = 0; offset < record.size; offset++)
                {
                    const std::vector<SourcePosition> & at = trip.table.GetPositionsAt ((Word) (record.address + offset));

                    Assert::AreEqual (record.positions.size(), at.size());

                    for (size_t depth = 0; depth < at.size(); depth++)
                    {
                        const std::string & expected = record.positions[depth].file.empty() ? mainName
                                                                                            : record.positions[depth].file;

                        Assert::AreEqual (expected,                        trip.file.files.at (at[depth].file).name);
                        Assert::AreEqual (record.positions[depth].line,    at[depth].line);
                        Assert::AreEqual ((int) depth,                     at[depth].depth);
                    }

                    checked++;
                }
            }

            Assert::AreEqual (trip.result.bytes.size(), checked, L"every byte was checked");
            return checked;
        }



        static const SourcePosition & Innermost (const RoundTrip & trip, Word address)
        {
            const std::vector<SourcePosition> & at = trip.table.GetPositionsAt (address);



            Assert::IsFalse (at.empty());
            return at.back();
        }



        TEST_METHOD (As65EveryAddressResolvesToItsLines)
        {
            AssertEveryAddressResolves (Run (DialectId::As65, "include-macro.a65", "include-macro.inc"), "include-macro.a65");
        }


        TEST_METHOD (As65InnerMacroBodyIsThreeDeep)
        {
            RoundTrip  trip = Run (DialectId::As65, "include-macro.a65", "include-macro.inc");



            //  ldx #3 at $0300, then the store expansion: lda #0 at $0302.
            Assert::AreEqual ((size_t) 3, trip.table.GetPositionsAt (0x0302).size());
            Assert::AreEqual (14, trip.table.GetPositionsAt (0x0302)[0].line, L"the invocation");
            Assert::AreEqual (9,  trip.table.GetPositionsAt (0x0302)[1].line, L"clear, in store's body");
            Assert::AreEqual (5,  Innermost (trip, 0x0302).line,              L"lda #0, in clear's body");
            Assert::AreEqual (2,  Innermost (trip, 0x0302).depth);
        }


        TEST_METHOD (As65IncludedRoutineNamesItsFile)
        {
            RoundTrip  trip  = Run (DialectId::As65, "include-macro.a65", "include-macro.inc");
            Word       setup = trip.result.symbols.at ("setup");



            Assert::AreEqual (std::string ("include-macro.inc"), trip.file.files.at (Innermost (trip, setup).file).name);
            Assert::AreEqual (2, Innermost (trip, setup).line);
        }


        TEST_METHOD (MerlinEveryAddressResolvesToItsLines)
        {
            AssertEveryAddressResolves (Run (DialectId::Merlin, "INCLUDE.MACRO.S", "T.INCLUDE.MACRO"), "INCLUDE.MACRO.S");
        }


        TEST_METHOD (MerlinInnerMacroBodyIsThreeDeep)
        {
            RoundTrip  trip = Run (DialectId::Merlin, "INCLUDE.MACRO.S", "T.INCLUDE.MACRO");



            Assert::AreEqual ((size_t) 3, trip.table.GetPositionsAt (0x0302).size());
            Assert::AreEqual (14, trip.table.GetPositionsAt (0x0302)[0].line, L"the invocation");
            Assert::AreEqual (9,  trip.table.GetPositionsAt (0x0302)[1].line, L"CLEAR, in STORE's body");
            Assert::AreEqual (5,  Innermost (trip, 0x0302).line,              L"LDA #0, in CLEAR's body");
        }


        TEST_METHOD (MerlinPutRoutineNamesItsFile)
        {
            RoundTrip  trip  = Run (DialectId::Merlin, "INCLUDE.MACRO.S", "T.INCLUDE.MACRO");
            Word       setup = trip.result.symbols.at ("SETUP");



            Assert::AreEqual (std::string ("T.INCLUDE.MACRO"), trip.file.files.at (Innermost (trip, setup).file).name);
            Assert::AreEqual (2, Innermost (trip, setup).line);
        }
    };
}
