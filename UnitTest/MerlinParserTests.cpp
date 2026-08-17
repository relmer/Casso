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



        //  From CLOCK.S. The equate sign sits in the OPCODE field with the name
        //  beside it in the label field, so this is a field-model fact and not an
        //  expression one: a parser hunting for the sign inside the operand finds
        //  it in the wrong place and leaves the line looking like an instruction
        //  named for it. 128 equates across the vendor sources and no other
        //  spelling.
        //
        //  The three fields must also be CLEARED. Leaving the name in the label
        //  field binds it twice -- once at the program counter and once as the
        //  constant -- and the second binding is reported as a duplicate of the
        //  first, which is how a whole file of equates fails on itself.
        TEST_METHOD (AnEquateIsReadFromTheFieldsAndThenClearsThem)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine ("HOURS = VERSION-25", 1);

            Assert::IsTrue (line.isConstant, L"the sign in the opcode field makes this an equate");
            Assert::AreEqual (std::string ("HOURS"),      line.constantName, L"the name comes from the label field");
            Assert::AreEqual (std::string ("VERSION-25"), line.constantExpr, L"and the expression from the operand");
            Assert::IsTrue (line.label.empty(),    L"the label must not also bind at the program counter");
            Assert::IsTrue (line.mnemonic.empty(), L"and the sign must not survive as an instruction");
        }



        //  From KEYMAC.S, and from three more in the linker demo. The label
        //  validator rejected all four before this, which fails a file the vendor
        //  shipped a working object for.
        TEST_METHOD (AQuestionMarkIsPartOfALabel)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine ("CMD? LDA #$00", 1);

            Assert::AreEqual (std::string ("CMD?"), line.label,    L"'?' is a label character, not a field break");
            Assert::AreEqual (std::string ("LDA"),  line.mnemonic, L"and the opcode follows it");
        }



        //  A character constant may hold a SPACE, so a whitespace-delimited scan
        //  keeps `#"` and hands the rest to the comment field. That is not a
        //  string-directive rule -- the opcode here is an instruction -- which is
        //  why it was diagnosed as a missing operator and was really the scanner.
        TEST_METHOD (ACharacterConstantMayHoldASpace)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" LDA #\" \"", 1);

            Assert::AreEqual (std::string ("LDA"),      line.mnemonic, L"an ordinary instruction");
            Assert::AreEqual (std::string ("#\" \""),   line.operand,
                              L"the space inside the quotes is the constant, not the end of the operand");
        }



        //  From KEYMAC.S: `NI <<<`. The body's own branch target sits on the line
        //  that CLOSES the definition -- precisely the line closing a body throws
        //  away. Twelve hand-written macro tests were green before the vendor
        //  source found it.
        TEST_METHOD (AMacroTerminatorMayCarryALabel)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine ("NI <<<", 1);

            Assert::AreEqual (std::string ("NI"), line.label, L"the terminator line may name a target");
            Assert::IsTrue (line.directiveToken == Directive::MacroEnd, L"and still closes the definition");
        }



        //  A variable symbol standing where an ordinary label would. It binds as
        //  the REASSIGNABLE kind, which is the whole of the difference: CLOCK.S
        //  names eight separate loop targets `]LOOP`, and a symbol that could only
        //  be defined once reports seven duplicates.
        TEST_METHOD (AVariableSymbolStandsWhereALabelDoesAndIsReassignable)
        {
            MerlinDialect  merlin;
            ParsedLine     variable = merlin.ParseLine ("]LOOP LDA #$00", 1);
            ParsedLine     ordinary = merlin.ParseLine ("LOOP LDA #$00", 2);

            Assert::AreEqual (std::string ("LDA"), variable.mnemonic, L"the rest of the line is unaffected");
            Assert::IsTrue (variable.labelKind == SymbolKind::Set,   L"a variable label may be redefined");
            Assert::IsTrue (ordinary.labelKind == SymbolKind::Label, L"an ordinary one may not");
        }



        //  Every positioned diagnostic rests on these. Three DIFFERENT columns in
        //  one line, because an implementation stamping one constant satisfies any
        //  assertion that only looks at one field -- and they are 1-based, so a
        //  0-based reading is off by one everywhere rather than visibly wrong.
        TEST_METHOD (EachFieldRecordsWhereItWasWritten)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine ("START  LDA   #$41", 1);

            Assert::AreEqual (1,  line.labelColumn,    L"the label opens the line");
            Assert::AreEqual (8,  line.mnemonicColumn, L"the opcode follows two spaces after a five-character label");
            Assert::AreEqual (14, line.operandColumn,  L"and the operand three after a three-character opcode");
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinDirectiveTests
    //
    //  The spelling table, and the fact that the operand scanner now reads from
    //  it rather than from a private copy.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinDirectiveTests)
    {
    public:

        //  Six spellings, ONE token. They differ in high-bit handling,
        //  inversion, and terminator convention -- parameters of one operation,
        //  not six operations, which is why five near-identical tokens would
        //  multiply the dispatch table for no behavioral gain.
        TEST_METHOD (EveryStringSpellingCarriesTheOneStringToken)
        {
            const char *  spellings[] = { "ASC", "DCI", "INV", "FLS", "STR", "REV" };

            for (const char * pszSpelling : spellings)
            {
                std::wstring  message = L"every string spelling must carry Directive::StringData";

                Assert::IsTrue (MerlinDirectiveTable::FromSpelling (pszSpelling) == Directive::StringData,
                                message.c_str());
            }
        }



        //  REV was found in the vendor sources and is absent from the spec's
        //  list of the string family. It is here to prove the operand scanner
        //  reads the real table: a spelling added there must reach the scanner
        //  with no second edit, or the two can disagree about what a string is.
        TEST_METHOD (RevGetsDelimitedOperandScanningLikeTheRestOfItsFamily)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" REV \"AB CD\"", 1);

            Assert::AreEqual (std::string ("\"AB CD\""), line.operand,
                              L"REV is a string directive, so whitespace inside its text is payload");
        }



        //  An operation Merlin and as65 share is ONE token with two spellings,
        //  not a Merlin-specific duplicate. That is the whole reason the token
        //  enum is shared while the tables are not.
        TEST_METHOD (SharedOperationsReuseExistingTokens)
        {
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DFB") == Directive::Byte,   L"DFB is a byte directive");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DA")  == Directive::Word,   L"DA is a word directive");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("PUT") == Directive::Include, L"PUT includes a file");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("USE") == Directive::Include, L"and so does USE");
        }



        //  CLOCK.S turns one source into two different objects with these, which
        //  is what makes it the corpus's best single specimen.
        TEST_METHOD (ConditionalSpellingsMapToTheSharedConditionalTokens)
        {
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DO")   == Directive::If,    L"DO opens a conditional");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("ELSE") == Directive::Else,  L"ELSE is the alternative");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("FIN")  == Directive::Endif, L"FIN closes it");
        }



        //  `<<<` ends a macro DEFINITION. Reading it as an invocation would be a
        //  silent misparse of every macro in the vendor library, all 18 of which
        //  end with it.
        TEST_METHOD (TripleAngleIsTheMacroTerminator)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" <<<", 1);

            Assert::IsTrue (line.directiveToken == Directive::MacroEnd,
                            L"'<<<' terminates a macro definition rather than invoking one");
        }



        //  Recognized so they can be refused BY NAME. An unknown-directive error
        //  would read as "Merlin support is broken" rather than "this is outside
        //  the subset", which is the distinction FR-016..FR-019 turn on.
        TEST_METHOD (SubsetBoundaryDirectivesAreRecognizedNotUnknown)
        {
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("REL") == Directive::Relocatable,    L"REL is known");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("ENT") == Directive::EntrySymbol,    L"ENT is known");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("EXT") == Directive::ExternalSymbol, L"EXT is known");
        }



        //  A word that is not a directive stays a mnemonic. Telling an
        //  instruction from a macro invocation needs the macro table, which the
        //  parser does not have and should not consult.
        TEST_METHOD (AnUnknownOpcodeWordIsNotADirective)
        {
            MerlinDialect  merlin;
            ParsedLine     macroCall = merlin.ParseLine (" STADR PTR", 1);

            Assert::IsFalse (macroCall.isDirective, L"STADR is a macro in the vendor library, not a directive");
            Assert::AreEqual (std::string ("STADR"), macroCall.mnemonic, L"and it stays in the opcode field");
        }



        //  Merlin directives have no dotted form, so a diagnostic quoting one
        //  must take its name from Merlin's table rather than as65's.
        TEST_METHOD (MerlinDirectivesAreNamedFromMerlinsOwnTable)
        {
            MerlinDialect  merlin;
            ParsedLine     line = merlin.ParseLine (" HEX 0A0B", 1);

            Assert::IsTrue (line.isDirective, L"HEX is a Merlin directive");
            Assert::AreEqual (std::string ("HEX"), line.directive, L"and is quoted back the way Merlin spells it");
        }



        //  The two word directives differ in BYTE ORDER, so they cannot share a
        //  token: one token would make the emitter pick an order from nothing.
        //  Asserted as a pair, because either one alone is satisfied by a table
        //  that maps both spellings to whichever token is being checked.
        TEST_METHOD (TheTwoWordDirectivesCarryDifferentTokens)
        {
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DA")  == Directive::Word,
                            L"DA is the processor's own order");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DW")  == Directive::Word,
                            L"and DW is a second spelling of it");
            Assert::IsTrue (MerlinDirectiveTable::FromSpelling ("DDB") == Directive::WordHighFirst,
                            L"DDB is the reversed order and must not resolve to the same operation");
        }



        //  Merlin holds no dotted form of anything, which is what makes a
        //  diagnostic quoting `.org` at a Merlin line describe a construct that
        //  cannot exist. Swept over the whole vocabulary rather than spot-checked,
        //  since one dotted spelling admitted later would be invisible otherwise.
        TEST_METHOD (NoMerlinSpellingIsWrittenWithALeadingDot)
        {
            std::span<const MerlinDirectiveTable::Spelling>  spellings = MerlinDirectiveTable::GetAllSpellings();

            Assert::IsTrue (spellings.size() > 0, L"a sweep over an empty vocabulary checks nothing");

            for (const MerlinDirectiveTable::Spelling & spelling : spellings)
            {
                std::string   name = spelling.name;
                std::wstring  what (name.begin(), name.end());

                Assert::IsFalse (name.empty(), L"a spelling with no characters cannot be written at all");
                Assert::AreNotEqual ('.', name[0], (what + L" is spelled the way as65 spells a directive").c_str());
            }
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
            const DialectProfile  &  byId   = DialectRegistry::Get (DialectId::Merlin);
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
