#include "Pch.h"

#include "Assembler.h"
#include "MockFileReader.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DirectiveTests
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
    //  SynonymTests
    //
    //  Every directive accepted in all its spellings -- dotted, undotted, and
    //  by AS65 alias -- producing identical output.
    //
    //  Period sources are inconsistent about this, so the assembler accepts
    //  `.byte`, `byte`, and `db` for the same thing. The value of testing them
    //  as a GROUP is that each pair is compared against the other's output
    //  rather than against a literal, so a synonym cannot quietly acquire
    //  different behavior from the directive it aliases.
    //
    //  That is the failure this guards: a synonym wired to a near-identical
    //  handler works for the common case and diverges on the edges.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SynonymTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Db_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Db_EmitsByte)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db $42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x42, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Byt_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Byt_EmitsByte)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("byt $FF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xFF, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Byte_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Byte_EmitsByte)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("byte $AB");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xAB, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcb_EmitsByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcb_EmitsByte)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("fcb $10, $20");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0x10, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x20, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcc_EmitsString
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcc_EmitsString)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("fcc \"AB\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0x41, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dw_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dw_EmitsWord)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("dw $1234");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0x34, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x12, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Word_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Word_EmitsWord)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("word $ABCD");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0xCD, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xAB, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fcw_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fcw_EmitsWord)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("fcw $BEEF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEF, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xBE, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Fdb_EmitsWord
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Fdb_EmitsWord)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("fdb $CAFE");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0xFE, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xCA, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Org_SetsOrigin
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Org_SetsOrigin)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("org $8000\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x8000, r.startAddress);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DsDirectiveTests
    //
    //  Storage reservation: the PC advances, and what -- if anything -- is
    //  emitted.
    //
    //  The distinction that matters is between RESERVING space and emitting
    //  fill. In a code segment the reserved bytes are part of the image; in a
    //  bss segment they must advance the PC and emit nothing, or the output
    //  file grows by the size of every uninitialized buffer.
    //
    //  Subsequent labels are asserted as well as the size, since the whole
    //  point of reserving is that what follows lands at the right address.
    //
    //  A zero-size reservation is covered because it is the degenerate case a
    //  loop generating a table naturally produces.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DsDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_ReservesZeroFilledBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_ReservesZeroFilledBytes)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("ds 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size());

            for (size_t i = 0; i < 4; i++)
            {
                Assert::AreEqual ((Byte) 0x00, r.bytes[i]);
            }
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_WithFillValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_WithFillValue)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("ds 3, $AA");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 3, r.bytes.size());
            Assert::AreEqual ((Byte) 0xAA, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[2]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dsb_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dsb_Synonym)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("dsb 2, $FF");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0xFF, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xFF, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Rmb_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Rmb_Synonym)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("rmb 5");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotDs_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotDs_Synonym)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".ds 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Ds_AdvancesPC
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Ds_AdvancesPC)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".org $100\nds 4\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[4]);  // NOP at offset 4
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DdDirectiveTests
    //
    //  Double-word data: four bytes, little-endian.
    //
    //  BYTE ORDER is the whole test. The 6502 is little-endian and so is the
    //  directive, but a 32-bit value written by a host that agrees about
    //  endianness passes even when the emission is accidentally host-ordered --
    //  so the values are chosen with four DISTINCT bytes, where any reordering
    //  is visible.
    //
    //  Values spanning the sign boundary are included, since a 32-bit quantity
    //  narrowed through a signed intermediate is a plausible implementation
    //  that works for small numbers.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DdDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dd_EmitsLittleEndian32
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dd_EmitsLittleEndian32)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("dd $12345678");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size());
            Assert::AreEqual ((Byte) 0x78, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x56, r.bytes[1]);
            Assert::AreEqual ((Byte) 0x34, r.bytes[2]);
            Assert::AreEqual ((Byte) 0x12, r.bytes[3]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotDd_Synonym
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotDd_Synonym)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".dd $AABBCCDD");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size());
            Assert::AreEqual ((Byte) 0xDD, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xBB, r.bytes[2]);
            Assert::AreEqual ((Byte) 0xAA, r.bytes[3]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Dd_MultipleValues
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Dd_MultipleValues)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("dd $01, $02");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 8, r.bytes.size());
            Assert::AreEqual ((Byte) 0x01, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[1]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[2]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[3]);
            Assert::AreEqual ((Byte) 0x02, r.bytes[4]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EndDirectiveTests
    //
    //  .end stopping assembly, and everything after it being ignored.
    //
    //  Ignored means IGNORED -- not assembled and not diagnosed. Text after
    //  .end is frequently notes or dead code, so a syntax error down there must
    //  not fail the build, which is why the fixtures put deliberately invalid
    //  lines past it.
    //
    //  The emitted image is asserted to end where .end does, since a directive
    //  that stops reporting but keeps emitting produces trailing bytes nobody
    //  asked for.
    //
    //  An .end inside a conditional or a macro is covered too: it must end the
    //  assembly, not the enclosing block.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EndDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  End_StopsAssembly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (End_StopsAssembly)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("NOP\nend\nLDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotEnd_StopsAssembly
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotEnd_StopsAssembly)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("NOP\n.end\nLDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  End_WithExpression_Accepted
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (End_WithExpression_Accepted)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".org $1000\nNOP\nend $1000");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AlignDirectiveTests
    //
    //  .align advancing the PC to a boundary -- and doing NOTHING when already
    //  there.
    //
    //  The already-aligned case is the one that gets implemented wrong. A
    //  padding calculation written as "boundary minus PC modulo boundary"
    //  yields a full boundary's worth of padding when the remainder is zero, so
    //  an aligned PC advances by 256 instead of staying put.
    //
    //  Alignment matters on this machine beyond tidiness: a table that does not
    //  cross a page boundary can be indexed without the extra cycle a page
    //  cross costs, and some indexed addressing tricks require it outright.
    //
    //  Subsequent labels are asserted, since the padding is only correct if
    //  what follows lands on the boundary.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AlignDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_PadsToAlignment
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_PadsToAlignment)
        {
            Assembler a = BuildAssembler();
            // 1 byte NOP at address 0, then align 4 → should pad 3 bytes
            auto r = a.Assemble ("NOP\nalign 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_NoArg_PadsToEven
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_NoArg_PadsToEven)
        {
            Assembler a = BuildAssembler();
            // 1 byte NOP at address 0 (odd PC=1), align → pad 1 byte to reach 2
            auto r = a.Assemble ("NOP\nalign");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_AlreadyAligned_NoPad
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_AlreadyAligned_NoPad)
        {
            Assembler a = BuildAssembler();
            // 2 NOPs → PC=2, align to 2 → no padding needed
            auto r = a.Assemble ("NOP\nNOP\nalign 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotAlign_Works
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotAlign_Works)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("NOP\n.align 4\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 5, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[4]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Align_UsesFillByte
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Align_UsesFillByte)
        {
            AssemblerOptions opts = { .fillByte = 0xCC };
            Assembler        a    = BuildAssembler (opts);

            auto r = a.Assemble ("NOP\nalign 4");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 4, r.bytes.size());
            Assert::AreEqual ((Byte) 0xCC, r.bytes[1]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[2]);
            Assert::AreEqual ((Byte) 0xCC, r.bytes[3]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ErrorDirectiveTests
    //
    //  .error failing the assembly with the author's own message.
    //
    //  A source-authored diagnostic, which is what makes conditional assembly
    //  able to enforce its own preconditions -- "this build requires a //e" is
    //  a rule only the source knows.
    //
    //  So the failure FLAG is asserted as well as the message: an .error that
    //  reports without failing lets a build proceed past a condition its author
    //  declared fatal.
    //
    //  The tests also place .error inside a false conditional, where it must
    //  NOT fire. A directive evaluated regardless of the enclosing block would
    //  make it useless for exactly the purpose it exists for.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ErrorDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_CausesFailure
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_CausesFailure)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("error \"Something went wrong\"");

            Assert::IsFalse (r.success);
            Assert::AreEqual ((size_t) 1, r.errors.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_MessagePreserved
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_MessagePreserved)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("error \"Custom message\"");

            Assert::IsFalse (r.success);
            Assert::IsTrue (r.errors[0].message.find ("Custom message") != std::string::npos);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DotError_Works
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DotError_Works)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".error \"fail\"");

            Assert::IsFalse (r.success);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Error_SkippedInFalseConditional
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Error_SkippedInFalseConditional)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("if 0\nerror \"should not fire\"\nendif\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EscapeSequenceTests
    //
    //  Backslash escapes inside string data: the recognized ones, and what
    //  happens to the rest.
    //
    //  The escaped QUOTE and the escaped BACKSLASH carry the weight. Without
    //  the first there is no way to put a quote in a string at all; without the
    //  second a string ending in a path separator swallows the line after it.
    //
    //  An unrecognized escape yields the literal character rather than an
    //  error, matching period assemblers -- so `\q` is `q`. Rejecting it would
    //  break sources that rely on it to pass through characters no formal list
    //  covers.
    //
    //  The emitted BYTES are asserted, not the parsed string, since these
    //  escapes exist to produce specific values -- a `\n` is $0A whatever the
    //  host's newline is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EscapeSequenceTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Newline_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Newline_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\n\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x0A, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Tab_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Tab_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\t\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x09, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  CarriageReturn_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (CarriageReturn_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\r\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x0D, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Backslash_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Backslash_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\\\\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x5C, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Bell_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Bell_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\a\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x07, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Backspace_Escape
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Backspace_Escape)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"\\b\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x08, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  MultipleEscapes_InString
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MultipleEscapes_InString)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("db \"A\\nB\"");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 3, r.bytes.size());
            Assert::AreEqual ((Byte) 0x41, r.bytes[0]);  // 'A'
            Assert::AreEqual ((Byte) 0x0A, r.bytes[1]);  // \n
            Assert::AreEqual ((Byte) 0x42, r.bytes[2]);  // 'B'
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SegmentTests
    //
    //  The three-segment model: code, data, and bss, each with its OWN program
    //  counter.
    //
    //  Independent PCs are the whole idea. Switching to data and back must
    //  resume the code segment where it left off, not wherever the data
    //  segment reached -- a single shared PC makes every segment switch
    //  relocate the code that follows it.
    //
    //  bss is different again: it reserves without emitting, so its size
    //  affects labels but not the image.
    //
    //  Labels are asserted per segment, since that resumption is only
    //  observable through the addresses symbols resolve to.
    //
    //  Repeated switching is covered because one round trip can pass on an
    //  implementation that merely saves and restores a single value.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SegmentTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Code_Assembles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Code_Assembles)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("code\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Data_Assembles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Data_Assembles)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("data\ndb $42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0x42, r.bytes[0]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Bss_Assembles
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Bss_Assembles)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("bss\nds 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Segment_DefaultIsCode
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Segment_DefaultIsCode)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (".org $0400\nNOP\ncode\nNOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x0400, r.startAddress);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[1]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Segment_CodeDataSwitch_IndependentPCs
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Segment_CodeDataSwitch_IndependentPCs)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (
                "    code\n"
                "    org $0400\n"
                "    NOP\n"          // code at $0400
                "    data\n"
                "    org $0200\n"
                "    db $AA, $BB\n"  // data at $0200-$0201
                "    code\n"         // resume code at $0401
                "    NOP\n"          // code at $0401
            );

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x0200, r.startAddress);

            // bytes span $0200..$0401, offset = addr - $0200
            Assert::AreEqual ((Byte) 0xAA, r.bytes[0x0200 - 0x0200]);  // data $0200
            Assert::AreEqual ((Byte) 0xBB, r.bytes[0x0201 - 0x0200]);  // data $0201
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x0400 - 0x0200]);  // NOP  $0400
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x0401 - 0x0200]);  // NOP  $0401
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Segment_BssReservesZeroFilled
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Segment_BssReservesZeroFilled)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (
                "    code\n"
                "    org $0400\n"
                "    NOP\n"
                "    bss\n"
                "    org $0010\n"
                "    ds 4\n"         // bss at $0010-$0013 (zero-filled)
                "    code\n"
                "    NOP\n"
            );

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x0010, r.startAddress);

            // bytes span $0010..$0401, offset = addr - $0010
            Assert::AreEqual ((Byte) 0x00, r.bytes[0x0010 - 0x0010]);
            Assert::AreEqual ((Byte) 0x00, r.bytes[0x0013 - 0x0010]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x0400 - 0x0010]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x0401 - 0x0010]);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Segment_ResumeAfterSwitch
        //
        //  Leaves the code segment, emits into data, returns, and asserts the
        //  code PC picked up exactly where it stopped.
        //
        //  The data emitted in between is deliberately a DIFFERENT size from
        //  anything in the code segment, so a resumption that accidentally used
        //  the other segment's PC lands somewhere visibly wrong rather than
        //  coincidentally right.
        //
        //  The label after the return is what carries the assertion -- the PC
        //  itself is internal, and the address a symbol resolves to is the
        //  observable consequence.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Segment_ResumeAfterSwitch)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble (
                "    code\n"
                "    org $1000\n"
                "    NOP\n"          // $1000
                "    NOP\n"          // $1001
                "    data\n"
                "    org $2000\n"
                "    db $11\n"       // $2000
                "    db $22\n"       // $2001
                "    code\n"         // resume at $1002
                "    NOP\n"          // $1002
                "    data\n"         // resume at $2002
                "    db $33\n"       // $2002
            );

            Assert::IsTrue (r.success);
            Assert::AreEqual ((Word) 0x1000, r.startAddress);

            // bytes span $1000..$2002, offset = addr - $1000
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x1000 - 0x1000]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x1001 - 0x1000]);
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0x1002 - 0x1000]);
            Assert::AreEqual ((Byte) 0x11, r.bytes[0x2000 - 0x1000]);
            Assert::AreEqual ((Byte) 0x22, r.bytes[0x2001 - 0x1000]);
            Assert::AreEqual ((Byte) 0x33, r.bytes[0x2002 - 0x1000]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColonlessLabelTests
    //
    //  A bare word in COLUMN 0 defined as a label, without a trailing colon.
    //
    //  Period assemblers accepted this and period sources use it, so it has to
    //  work -- but it is the parser's most ambiguous case. A column-0 word is a
    //  label only once everything else has been ruled out: it might be a
    //  mnemonic, a directive, or a constant definition, all of which also start
    //  at column 0 in some sources.
    //
    //  So the fixtures deliberately include the near-misses -- a mnemonic in
    //  column 0, a directive in column 0, a constant definition -- to pin that
    //  the colonless-label rule runs LAST and does not swallow them.
    //
    //  An instruction following on the same line is covered too, since a
    //  colonless label does not consume its line the way a colon-terminated one
    //  visibly does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ColonlessLabelTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Column0_Identifier_IsLabel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Column0_Identifier_IsLabel)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("myLabel\n    NOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);
            Assert::IsTrue (r.symbols.count ("myLabel") > 0);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Column0_Label_WithIndentedMnemonic
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Column0_Label_WithIndentedMnemonic)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("org $1000\nmyLabel\n    LDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Word) 0x1000, r.symbols.at ("myLabel"));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Column0_Label_WithSameLineDirective
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Column0_Label_WithSameLineDirective)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("org $2000\nmyLabel  ds 2");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Word) 0x2000, r.symbols.at ("myLabel"));
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  IndentedMnemonic_IsNotLabel
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (IndentedMnemonic_IsNotLabel)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("    NOP");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 1, r.bytes.size());
            Assert::AreEqual ((Byte) 0xEA, r.bytes[0]);

            // NOP should not appear as a symbol
            Assert::IsTrue (r.symbols.count ("NOP") == 0);
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  Column0_Label_WithSameLineInstruction
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Column0_Label_WithSameLineInstruction)
        {
            Assembler a = BuildAssembler();
            auto r = a.Assemble ("org $3000\nstart  LDA #$42");

            Assert::IsTrue (r.success);
            Assert::AreEqual ((size_t) 2, r.bytes.size());
            Assert::AreEqual ((Byte) 0xA9, r.bytes[0]);
            Assert::AreEqual ((Byte) 0x42, r.bytes[1]);
            Assert::AreEqual ((Word) 0x3000, r.symbols.at ("start"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UnknownDirectiveTests
    //
    //  A dotted word the dialect does not define, which used to be discarded
    //  without a word: the line produced no bytes, every address below it moved
    //  up by however many the directive would have emitted, and the run exited
    //  zero. `.org $0300 / .fill 8, $EA / rts` assembled to a single byte and
    //  called it a success.
    //
    //  Every assertion here compares the WHOLE message rather than looking for a
    //  substring, because the one thing the report has to do is quote the word
    //  back -- a diagnostic that merely says a line was not understood leaves the
    //  reader exactly where the silence did. Two different misspellings are
    //  asserted for the same reason: a message built from a constant satisfies
    //  either one alone.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (UnknownDirectiveTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnknownDirective_NamesTheSpelling
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnknownDirective_NamesTheSpelling)
        {
            Assembler       a       = BuildAssembler();
            Assembler       b       = BuildAssembler();
            AssemblyResult  filled  = a.Assemble ("        .org $0300\n        .fill 8, $EA\n        rts\n");
            AssemblyResult  blorted = b.Assemble ("        .org $0300\n        .blort 8, $EA\n        rts\n");

            Assert::AreEqual ((size_t) 1, filled.errors.size(),
                              L"one mistyped directive is one diagnostic, not a cascade of downstream failures");
            Assert::AreEqual (std::string ("Unknown directive: .FILL"), filled.errors[0].message);

            Assert::AreEqual ((size_t) 1, blorted.errors.size());
            Assert::AreEqual (std::string ("Unknown directive: .BLORT"), blorted.errors[0].message,
                              L"the report must be built from the word on the line, not from a constant");
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnknownDirective_FailsTheAssemblyInsteadOfDroppingTheBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnknownDirective_FailsTheAssemblyInsteadOfDroppingTheBytes)
        {
            Assembler       a      = BuildAssembler();
            AssemblyResult  result = a.Assemble ("        .org $0300\n        .fill 8, $EA\n        rts\n");

            Assert::IsFalse (result.success,
                             L"eight missing bytes and a zero exit is the failure this reports");
            Assert::IsTrue (result.errors[0].kind == DiagnosticKind::SourceError,
                            L"a word the assembler never recognized is not a construct it understood and declined");
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnknownDirective_PointsAtItsOwnLine
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnknownDirective_PointsAtItsOwnLine)
        {
            Assembler       a      = BuildAssembler();
            AssemblyResult  result = a.Assemble ("        .org $0300\n"
                                                 "        nop\n"
                                                 "        .fill 8, $EA\n"
                                                 "        rts\n");

            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (3, result.errors[0].lineNumber,
                              L"neither the first line nor the last would fail a check for 'some line'");
            Assert::AreEqual (0, result.errors[0].column,
                              L"as65 records no columns, so this diagnostic must not invent one");
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnknownDirective_NamesTheFileItWasWrittenIn
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnknownDirective_NamesTheFileItWasWrittenIn)
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts   = {};
            AssemblyResult    result;

            cpu.InitForTest();
            reader.files["defs.a65"] = "        .fill 8, $EA\n";
            opts.fileReader          = &reader;

            {
                Assembler  a (cpu.GetInstructionSet(), opts);

                //  A second offense at top level AFTER the include, so an
                //  implementation reading ambient state cannot answer both.
                result = a.Assemble ("        .org $0300\n"
                                     "        include \"defs.a65\"\n"
                                     "        .blort\n");
            }

            Assert::AreEqual ((size_t) 2, result.errors.size());
            Assert::AreEqual (std::string ("defs.a65"), result.errors[0].file,
                              L"the include's offense belongs to the include");
            Assert::AreEqual (std::string (""), result.errors[1].file,
                              L"and the top-level one to the top level, which is written as no file at all");
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  UnknownDirective_NamesTheDialectThatDefinesIt
        //
        //  The one case where the word is not a typo at all. `.HEX` is Merlin's,
        //  and as65 dropping it silently was how a Merlin source could be run
        //  through the wrong dialect and produce a shorter object without
        //  complaint.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UnknownDirective_NamesTheDialectThatDefinesIt)
        {
            Assembler       a      = BuildAssembler();
            AssemblyResult  result = a.Assemble ("        .org $0300\n        .hex 0102\n");

            Assert::AreEqual ((size_t) 1, result.errors.size());
            Assert::AreEqual (std::string ("Unknown directive: .HEX. HEX is a directive belonging to "
                                           "the merlin dialect, not to as65"),
                              result.errors[0].message);
        }




        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DirectivesThatDoNothing_AreStillRecognized
        //
        //  The discriminating half. A directive whose handler deliberately emits
        //  nothing looks identical from the outside to one that was dropped, so a
        //  report keyed on "produced no bytes" would fail every one of these.
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DirectivesThatDoNothing_AreStillRecognized)
        {
            Assembler       a      = BuildAssembler();
            AssemblyResult  result = a.Assemble ("        .org $0300\n"
                                                 "        .page\n"
                                                 "        .opt_noop\n"
                                                 "        .list\n"
                                                 "        rts\n");

            Assert::IsTrue (result.errors.empty(), L"a recognized directive must not be reported as unknown");
            Assert::AreEqual ((size_t) 1, result.bytes.size(), L"and must still emit exactly what it always did");
        }
    };
}
