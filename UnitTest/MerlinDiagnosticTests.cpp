#include "Pch.h"

#include "Assembler.h"
#include "DiagnosticFormatter.h"
#include "TestHelpers.h"
#include "MockFileReader.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinDiagnosticTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiagnosticAttributionTests
    //
    //  Which FILE a diagnostic names.
    //
    //  Every diagnostic used to be reported against the top-level input, because
    //  that was the only path the reporting side held. An error inside an
    //  included file was therefore attributed to the file that included it,
    //  which is the wrong line in the wrong file.
    //
    //  The fix is to capture the originating file where the diagnostic is
    //  CREATED rather than where it is reported. These tests pin that, and they
    //  matter more than they look: Merlin's file-inclusion directives make
    //  multi-file assembly ordinary rather than occasional.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiagnosticAttributionTests)
    {
    public:

        //  A diagnostic from the top-level source carries no file of its own, so
        //  the reporting side supplies the input path -- exactly as before.
        TEST_METHOD (TopLevelError_CarriesNoFileOfItsOwn)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            Assembler         assembler (cpu.GetInstructionSet(), opts);
            AssemblyResult    result = assembler.Assemble ("  .org $800\n  NOTANOP\n");

            Assert::IsFalse (result.errors.empty(), L"an unknown mnemonic must be an error");
            Assert::AreEqual (std::string (""), result.errors[0].file,
                              L"a top-level diagnostic names no file; the reporter supplies the input path");
        }



        //  The defect this whole change exists for.
        TEST_METHOD (ErrorInsideInclude_NamesTheIncludedFile)
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;

            reader.files["helper.a65"] = "  NOTANOP\n";

            opts.fileReader = &reader;

            Assembler       assembler (cpu.GetInstructionSet(), opts);
            AssemblyResult  result = assembler.Assemble ("  .org $800\n  .include \"helper.a65\"\n");

            Assert::IsFalse (result.errors.empty(), L"the bad line inside the include must be an error");
            Assert::AreEqual (std::string ("helper.a65"), result.errors[0].file,
                              L"an error inside an included file must name that file, not the top-level input");
        }



        //  A DEFERRED diagnostic: the block opens inside the include and is not
        //  reported until the end of the pass, by which time ambient state names
        //  whatever file was processed last. Getting the line right and the file
        //  wrong reads as a correct diagnostic, which is what makes it worth
        //  pinning.
        TEST_METHOD (UnclosedIfInsideInclude_NamesTheIncludedFile)
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;

            reader.files["opener.a65"] = "  .ifdef NOTDEFINED\n  LDA #$01\n";

            opts.fileReader = &reader;

            //  The trailing top-level line is what makes this test discriminating.
            //  With the include last, the ambient file at end of pass would still
            //  be the include and the assertion would hold with or without the
            //  fix. Assembling a top-level line afterwards moves ambient back to
            //  the top-level input, so only a captured open-file can pass.
            Assembler       assembler (cpu.GetInstructionSet(), opts);
            AssemblyResult  result = assembler.Assemble ("  .org $800\n  .include \"opener.a65\"\n  NOP\n");

            Assert::IsFalse (result.errors.empty(), L"an unclosed conditional must be an error");
            Assert::AreEqual (std::string ("opener.a65"), result.errors[0].file,
                              L"an unclosed IF must name the file it OPENED in, not the last file processed");
        }



        //  Same shape, the other deferred carrier.
        TEST_METHOD (UnclosedMacroInsideInclude_NamesTheIncludedFile)
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;

            reader.files["opener.a65"] = "MYMAC macro\n  LDA #$01\n";

            opts.fileReader = &reader;

            //  Trailing top-level line for the same reason as above: it moves the
            //  ambient file off the include, so the assertion can only pass if the
            //  definition captured its own.
            Assembler       assembler (cpu.GetInstructionSet(), opts);
            AssemblyResult  result = assembler.Assemble ("  .org $800\n  .include \"opener.a65\"\n  NOP\n");

            Assert::IsFalse (result.errors.empty(), L"an unterminated macro must be an error");
            Assert::AreEqual (std::string ("opener.a65"), result.errors[0].file,
                              L"an unterminated macro must name the file its definition OPENED in");
        }



        //  Position fields are additive, so a diagnostic that knows no column
        //  reports 0 and the formatter leaves it out entirely.
        TEST_METHOD (DiagnosticWithoutColumn_ReportsZero)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            Assembler         assembler (cpu.GetInstructionSet(), opts);
            AssemblyResult    result = assembler.Assemble ("  .org $800\n  NOTANOP\n");

            Assert::IsFalse (result.errors.empty());
            Assert::AreEqual (0, result.errors[0].column,
                              L"AS65 knows no column, and 0 means exactly that");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiagnosticFormatterTests
    //
    //  The formatting DECISION, tested in core rather than by running the exe.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiagnosticFormatterTests)
    {
    public:

        TEST_METHOD (NoFileOfItsOwn_UsesTheFallback)
        {
            AssemblyError  e = {};

            e.lineNumber = 12;
            e.message    = "something went wrong";

            Assert::AreEqual (std::string ("main.a65:12: error: something went wrong"),
                              DiagnosticFormatter::Format (e, "main.a65", DiagnosticSeverity::Error));
        }



        TEST_METHOD (OwnFile_OverridesTheFallback)
        {
            AssemblyError  e = {};

            e.lineNumber = 3;
            e.message    = "something went wrong";
            e.file       = "helper.a65";

            Assert::AreEqual (std::string ("helper.a65:3: error: something went wrong"),
                              DiagnosticFormatter::Format (e, "main.a65", DiagnosticSeverity::Error));
        }



        //  A column of 0 is omitted rather than printed, because an editor
        //  jumping to column 0 lands somewhere arbitrary.
        TEST_METHOD (ZeroColumn_IsOmitted)
        {
            AssemblyError  e = {};

            e.lineNumber = 7;
            e.message    = "no column here";
            e.column     = 0;

            Assert::AreEqual (std::string ("main.a65:7: warning: no column here"),
                              DiagnosticFormatter::Format (e, "main.a65", DiagnosticSeverity::Warning));
        }



        TEST_METHOD (KnownColumn_IsIncluded)
        {
            AssemblyError  e = {};

            e.lineNumber = 7;
            e.message    = "bad label column";
            e.file       = "prog.s";
            e.column     = 4;

            Assert::AreEqual (std::string ("prog.s:7:4: error: bad label column"),
                              DiagnosticFormatter::Format (e, "main.a65", DiagnosticSeverity::Error));
        }
    };
}
