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
    //  MerlinDiagnosticPositionTests
    //
    //  WHERE on the line a Merlin diagnostic points.
    //
    //  Every assertion here states an exact column rather than "not zero". A
    //  non-zero check passes for any implementation that stamps a constant, and
    //  the constant that would be stamped -- 1 -- is a column real source uses.
    //  So the sources are deliberately indented by differing amounts and no two
    //  expected columns in this class are the same number.
    //
    //  The AS65 half is asserted alongside, because "additive" is a claim about
    //  what did NOT change and is otherwise untestable. as65 records no columns,
    //  so every one of its diagnostics must still report 0 and print without a
    //  column at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinDiagnosticPositionTests)
    {
    public:

        //  A refusal is a DEFERRED diagnostic -- it is composed after the last
        //  line has been read -- so its column has to be captured where the
        //  construct was met. The trailing lines are what make this
        //  discriminating: an implementation reading ambient state at reporting
        //  time gets the LAST line's column, and the last line here is indented
        //  differently on purpose.
        TEST_METHOD (BoundaryRefusal_PointsAtTheConstructNotAtTheLastLine)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect = DialectId::Merlin;

            {
                Assembler  merlin (cpu.GetInstructionSet(), opts);

                //  REL sits at column 4; the final line's opcode at column 16.
                result = merlin.Assemble ("   REL\n"
                                          "               LDA #$00\n");
            }

            Assert::AreEqual ((size_t) 1, result.errors.size(), L"exactly one construct is outside the subset here");
            Assert::IsTrue (result.errors[0].kind == DiagnosticKind::SubsetBoundary,
                            L"a refusal must be structurally distinguishable from a syntax error");
            Assert::AreEqual (1, result.errors[0].lineNumber);
            Assert::AreEqual (4, result.errors[0].column,
                              L"the refusal must point at REL, not at whatever line was processed last");
        }



        //  Two refusals at two different columns in one file. A single captured
        //  column shared by both would satisfy either assertion alone.
        TEST_METHOD (TwoRefusals_EachKeepItsOwnColumn)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect = DialectId::Merlin;

            {
                Assembler  merlin (cpu.GetInstructionSet(), opts);

                result = merlin.Assemble ("  REL\n"
                                          "         ENT START\n"
                                          "START    LDA #$00\n");
            }

            Assert::AreEqual ((size_t) 2, result.errors.size(), L"both constructs are refused, not just the first");
            Assert::AreEqual (3, result.errors[0].column, L"REL begins in column 3");
            Assert::AreEqual (10, result.errors[1].column, L"ENT begins in column 10");
        }



        //  A diagnostic about the OPERAND points at the operand, not at the
        //  opcode that owns it. The two differ by a known amount here, so a
        //  column pinned to the line rather than to the field fails.
        TEST_METHOD (ExpressionError_PointsAtTheOperand)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect = DialectId::Merlin;

            {
                Assembler  merlin (cpu.GetInstructionSet(), opts);

                //  LDA at column 6, its operand at column 10.
                result = merlin.Assemble ("     LDA #$00+\n");
            }

            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (10, result.errors[0].column,
                              L"the operand is the subject, so the position is the operand's");
        }



        //  And a diagnostic about the LABEL points at the label, which on a
        //  Merlin line is a third distinct column.
        TEST_METHOD (DuplicateLabel_PointsAtTheLabel)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect = DialectId::Merlin;

            {
                Assembler  merlin (cpu.GetInstructionSet(), opts);

                result = merlin.Assemble ("SAME     LDA #$00\n"
                                          "SAME     LDA #$01\n");
            }

            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (2, result.errors[0].lineNumber);
            Assert::AreEqual (1, result.errors[0].column, L"the duplicated label begins in column 1");
        }



        //  A DEFERRED diagnostic whose construct opened elsewhere. The
        //  conditional opens on line 1 at column 3 and is never closed; the last
        //  line processed is indented far further, so ambient state cannot
        //  answer.
        TEST_METHOD (UnclosedConditional_KeepsTheColumnItOpenedAt)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect              = DialectId::Merlin;
            opts.predefinedSymbols["BUILD"] = 1;

            {
                Assembler  merlin (cpu.GetInstructionSet(), opts);

                result = merlin.Assemble ("  DO BUILD\n"
                                          "                    LDA #$00\n");
            }

            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (1, result.errors[0].lineNumber);
            Assert::AreEqual (3, result.errors[0].column,
                              L"the unclosed conditional must report where it OPENED, on the line and across it");
        }



        //  The additive half. as65 records no columns, so its diagnostics must
        //  keep reporting none -- including the ones this feature rewrote.
        TEST_METHOD (As65Diagnostics_StillCarryNoColumn)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            AssemblyResult    result;

            cpu.InitForTest();
            opts.dialect = DialectId::As65;

            {
                Assembler  as65 (cpu.GetInstructionSet(), opts);

                result = as65.Assemble ("  .org $800\n"
                                        "SAME: LDA #$00\n"
                                        "SAME: LDA #$01\n"
                                        "  LDA #$00+\n");
            }

            Assert::IsFalse (result.errors.empty(), L"both mistakes must still be errors");

            for (const AssemblyError & error : result.errors)
            {
                Assert::AreEqual (0, error.column,
                                  L"as65 records no columns, and every one of its diagnostics must still say so");
            }
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
