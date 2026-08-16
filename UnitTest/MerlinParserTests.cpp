#include "Pch.h"

#include "MerlinDialect.h"
#include "DialectRegistry.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinParserTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinLineModelTests
    //
    //  The field-based line model.
    //
    //  Every line asserted here is either taken verbatim from the committed
    //  vendor sources or is the minimal shape of one. Inventing plausible Merlin
    //  source to test against would prove the parser agrees with my idea of
    //  Merlin, which is the thing actually in question.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinLineModelTests)
    {
    public:

        //  Column 0 is the only significant column: a word there is a label.
        TEST_METHOD (LabelIsRecognizedOnlyInColumnZero)
        {
            MerlinDialect  merlin;
            ParsedLine     labeled   = merlin.ParseLine (":ABORT RTS", 1);
            ParsedLine     unlabeled = merlin.ParseLine (" RTS", 2);

            Assert::AreEqual (std::string (":ABORT"), labeled.label,   L"column 0 opens the label field");
            Assert::AreEqual (std::string ("RTS"),    labeled.mnemonic, L"and the opcode follows it");
            Assert::AreEqual (std::string (""),       unlabeled.label,  L"leading whitespace means no label");
            Assert::AreEqual (std::string ("RTS"),    unlabeled.mnemonic, L"so the line begins at the opcode");
        }



        //  Tabs separate fields exactly as spaces do, with no tab-stop
        //  expansion -- tab stops affect display and nothing else.
        TEST_METHOD (TabsSeparateFieldsLikeSpaces)
        {
            MerlinDialect  merlin;
            ParsedLine     tabbed = merlin.ParseLine ("LOOP\tLDA\t#$00", 1);

            Assert::AreEqual (std::string ("LOOP"),  tabbed.label,    L"a tab ends the label field");
            Assert::AreEqual (std::string ("LDA"),   tabbed.mnemonic, L"and separates the opcode");
            Assert::AreEqual (std::string ("#$00"),  tabbed.operand,  L"and the operand");
        }



        //  Runs of whitespace collapse: the columns in a Merlin listing are the
        //  editor's formatting, not a requirement on the source.
        TEST_METHOD (RunsOfWhitespaceSeparateFieldsRegardlessOfWidth)
        {
            MerlinDialect  merlin;
            ParsedLine     spaced = merlin.ParseLine ("START         LDA      #$41", 1);

            Assert::AreEqual (std::string ("START"), spaced.label,    L"any run of spaces ends a field");
            Assert::AreEqual (std::string ("LDA"),   spaced.mnemonic, L"no field is required at a fixed column");
            Assert::AreEqual (std::string ("#$41"),  spaced.operand,  L"and the operand follows");
        }



        //  From CLOCK.S. A semicolon beginning the field after the operand
        //  starts a comment, and the comment is not part of any field.
        TEST_METHOD (SemicolonBeginningAFieldStartsTheComment)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" BNE :ABORT ;Ignore if not", 1);

            Assert::AreEqual (std::string ("BNE"),    line.mnemonic, L"the opcode is unaffected");
            Assert::AreEqual (std::string (":ABORT"), line.operand,  L"the operand stops at the comment");
        }



        //  An implied-mode opcode with nothing but a comment after it. From
        //  CLOCK.S: the comment must not be mistaken for an operand.
        TEST_METHOD (CommentAfterAnOperandlessOpcodeIsNotAnOperand)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" SEI ;Disable interupts while", 1);

            Assert::AreEqual (std::string ("SEI"), line.mnemonic, L"the opcode stands alone");
            Assert::AreEqual (std::string (""),    line.operand,  L"a comment is not an operand");
        }



        //  THE bug this rule exists to prevent. Merlin separates macro arguments
        //  with semicolons INSIDE the operand field, so a parser stripping from
        //  the first semicolon truncates every macro call on the vendor disk to
        //  its first argument -- wrong bytes, no error. From PI.ADD.S.
        TEST_METHOD (SemicolonInsideTheOperandIsDataNotAComment)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" ADD SUMSTR;DEFLEN;PL", 1);

            Assert::AreEqual (std::string ("ADD"), line.mnemonic, L"the macro name is the opcode field");
            Assert::AreEqual (std::string ("SUMSTR;DEFLEN;PL"), line.operand,
                              L"all three macro arguments must survive; semicolons here are separators, not a comment");
        }



        //  Both whole-line comment forms. The semicolon case is not a special
        //  rule: with no label, column 1 is the first field boundary. Eight such
        //  lines appear across three vendor sources.
        TEST_METHOD (WholeLineCommentsAreEmptyLines)
        {
            MerlinDialect  merlin;
            ParsedLine     star = merlin.ParseLine ("*** APPLE PI ***", 1);
            ParsedLine     semi = merlin.ParseLine ("; and return to caller", 2);

            Assert::IsTrue (star.isEmpty, L"'*' in column 1 is a whole-line comment");
            Assert::IsTrue (semi.isEmpty, L"';' in column 1 is one too -- it begins the first field");
        }



        //  From PI.START.S. Whitespace inside quoted text is payload, so the
        //  operand must not end at the first space -- and the leading and
        //  trailing spaces inside the quotes are data bytes.
        TEST_METHOD (QuotedTextKeepsItsSpaces)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" ASC \"(1-10) \"", 1);

            Assert::AreEqual (std::string ("ASC"), line.mnemonic, L"the directive is the opcode field");
            Assert::AreEqual (std::string ("\"(1-10) \""), line.operand,
                              L"the trailing space inside the quotes is a data byte, not a field separator");
        }



        //  From KEYMAC.S, and the reason the scanner takes its delimiter from
        //  the source rather than from a fixed quote set. This line chooses '!'
        //  precisely BECAUSE its text contains quotes; a '"'-only scanner ends
        //  the operand inside the data and silently emits different bytes.
        TEST_METHOD (AnyCharacterCanDelimitStringText)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" ASC !\" ASC \"\"!", 1);

            Assert::AreEqual (std::string ("ASC"), line.mnemonic, L"the directive is unaffected");
            Assert::AreEqual (std::string ("!\" ASC \"\"!"), line.operand,
                              L"the delimiter is the character that opened the text, quotes inside it are data");
        }



        //  A trailing byte after the closing delimiter belongs to the operand,
        //  not to a comment.
        TEST_METHOD (TextFollowedByATrailingByteStaysOneOperand)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" ASC \"ABC\",8D", 1);

            Assert::AreEqual (std::string ("\"ABC\",8D"), line.operand,
                              L"the byte after the closing delimiter is part of the operand");
        }



        //  A label with no opcode is a real line, not an empty one.
        TEST_METHOD (ALabelAloneIsNotAnEmptyLine)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine ("SENDMSG", 1);

            Assert::AreEqual (std::string ("SENDMSG"), line.label, L"the label is read");
            Assert::IsFalse (line.isEmpty, L"and a line carrying a label is not empty");
        }



        TEST_METHOD (BlankAndWhitespaceOnlyLinesAreEmpty)
        {
            MerlinDialect  merlin;
            ParsedLine     blank      = merlin.ParseLine ("", 1);
            ParsedLine     whitespace = merlin.ParseLine ("   \t  ", 2);

            Assert::IsTrue (blank.isEmpty,      L"an empty line is empty");
            Assert::IsTrue (whitespace.isEmpty, L"and so is one holding only whitespace");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinProfileIdentityTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinProfileIdentityTests)
    {
    public:

        TEST_METHOD (RegistryResolvesMerlinByNameAndByEnumerator)
        {
            const DialectProfile  &  byId = DialectRegistry::Get (DialectId::Merlin);
            DialectId                byName = DialectId::As65;

            Assert::IsTrue (DialectRegistry::TryLookUpByName ("merlin", byName), L"'merlin' must name a dialect");
            Assert::IsTrue (byName == DialectId::Merlin, L"and resolve to the Merlin enumerator");
            Assert::AreEqual ("merlin", byId.GetName(), L"the profile names itself the way it is selected");
        }



        //  Merlin takes its CPU from XC in the source, never from a flag. This is
        //  what lets the shared command-line parser refuse --cpu without knowing
        //  which dialect it is refusing it for.
        TEST_METHOD (MerlinTakesItsCpuFromSource)
        {
            const DialectProfile  &  merlin = DialectRegistry::Get (DialectId::Merlin);

            Assert::IsTrue (merlin.GetCpuSelectionSource() == CpuSelectionSource::InSource,
                            L"Merlin selects its CPU in source");
            Assert::AreEqual ("XC", merlin.GetCpuDirectiveName(),
                              L"and names the directive, so a refusal can quote it");
        }
    };
}
